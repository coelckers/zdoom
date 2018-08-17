// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2018 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** hw_portal.cpp
**   portal maintenance classes for skyboxes, horizons etc. (API independent parts)
**
*/

#include "c_dispatch.h"
#include "portal.h"
#include "p_maputl.h"
#include "hw_portal.h"
#include "hw_clipper.h"
#include "actor.h"
#include "g_levellocals.h"

EXTERN_CVAR(Int, r_mirror_recursions)

//-----------------------------------------------------------------------------
//
// StartFrame
//
//-----------------------------------------------------------------------------

void FPortalSceneState::StartFrame()
{
	if (renderdepth == 0)
	{
		inskybox = false;
		screen->instack[sector_t::floor] = screen->instack[sector_t::ceiling] = 0;
	}
	renderdepth++;
}

//-----------------------------------------------------------------------------
//
// printing portal info
//
//-----------------------------------------------------------------------------

static bool gl_portalinfo;

CCMD(gl_portalinfo)
{
	gl_portalinfo = true;
}

static FString indent;

//-----------------------------------------------------------------------------
//
// EndFrame
//
//-----------------------------------------------------------------------------

void FPortalSceneState::EndFrame(HWDrawInfo *di)
{
	IPortal * p;

	if (gl_portalinfo)
	{
		Printf("%s%d portals, depth = %d\n%s{\n", indent.GetChars(), di->Portals.Size(), renderdepth, indent.GetChars());
		indent += "  ";
	}

	// Only use occlusion query if there are more than 2 portals. 
	// Otherwise there's too much overhead.
	// (And don't forget to consider the separating null pointers!)
	bool usequery = di->Portals.Size() > 2 + (unsigned)renderdepth;

	while (di->Portals.Pop(p) && p)
	{
		if (gl_portalinfo) 
		{
			Printf("%sProcessing %s, depth = %d, query = %d\n", indent.GetChars(), p->GetName(), renderdepth, usequery);
		}
		if (p->lines.Size() > 0)
		{
			p->RenderPortal(true, usequery, di);
		}
		delete p;
	}
	renderdepth--;

	if (gl_portalinfo)
	{
		indent.Truncate(long(indent.Len()-2));
		Printf("%s}\n", indent.GetChars());
		if (indent.Len() == 0) gl_portalinfo = false;
	}
}


//-----------------------------------------------------------------------------
//
// Renders one sky portal without a stencil.
// In more complex scenes using a stencil for skies can severely stall
// the GPU and there's rarely more than one sky visible at a time.
//
//-----------------------------------------------------------------------------
bool FPortalSceneState::RenderFirstSkyPortal(int recursion, HWDrawInfo *outer_di)
{
	IPortal * p;
	IPortal * best = nullptr;
	unsigned bestindex = 0;

	// Find the one with the highest amount of lines.
	// Normally this is also the one that saves the largest amount
	// of time by drawing it before the scene itself.
	auto &portals = outer_di->Portals;
	for (int i = portals.Size() - 1; i >= 0; --i)
	{
		p = portals[i];
		if (p->lines.Size() > 0 && p->IsSky())
		{
			// Cannot clear the depth buffer inside a portal recursion
			if (recursion && p->NeedDepthBuffer()) continue;

			if (!best || p->lines.Size() > best->lines.Size())
			{
				best = p;
				bestindex = i;
			}
		}
	}

	if (best)
	{
		portals.Delete(bestindex);
		best->RenderPortal(false, false, outer_di);
		delete best;
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//
// 
//
//-----------------------------------------------------------------------------

void HWScenePortalBase::ClearClipper(HWDrawInfo *di, Clipper *clipper)
{
	auto outer_di = di->outer;
	DAngle angleOffset = deltaangle(outer_di->Viewpoint.Angles.Yaw, di->Viewpoint.Angles.Yaw);

	clipper->Clear();

	auto &lines = mOwner->lines;
	// Set the clipper to the minimal visible area
	clipper->SafeAddClipRange(0, 0xffffffff);
	for (unsigned int i = 0; i < lines.Size(); i++)
	{
		DAngle startAngle = (DVector2{lines[i].glseg.x2, lines[i].glseg.y2} - outer_di->Viewpoint.Pos).Angle() + angleOffset;
		DAngle endAngle = (DVector2{lines[i].glseg.x1, lines[i].glseg.y1} - outer_di->Viewpoint.Pos).Angle() + angleOffset;

		if (deltaangle(endAngle, startAngle) < 0)
		{
			clipper->SafeRemoveClipRangeRealAngles(startAngle.BAMs(), endAngle.BAMs());
		}
	}

	// and finally clip it to the visible area
	angle_t a1 = di->FrustumAngle();
	if (a1 < ANGLE_180) clipper->SafeAddClipRangeRealAngles(di->Viewpoint.Angles.Yaw.BAMs() + a1, di->Viewpoint.Angles.Yaw.BAMs() - a1);

	// lock the parts that have just been clipped out.
	clipper->SetSilhouette();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Common code for line to line and mirror portals
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

int HWLinePortal::ClipSeg(seg_t *seg, const DVector3 &viewpos)
{
	line_t *linedef = seg->linedef;
	if (!linedef)
	{
		return PClip_Inside;	// should be handled properly.
	}
	return P_ClipLineToPortal(linedef, line(), viewpos.XY()) ? PClip_InFront : PClip_Inside;
}

int HWLinePortal::ClipSubsector(subsector_t *sub)
{
	// this seg is completely behind the mirror
	for (unsigned int i = 0; i<sub->numlines; i++)
	{
		if (P_PointOnLineSidePrecise(sub->firstline[i].v1->fPos(), line()) == 0) return PClip_Inside;
	}
	return PClip_InFront;
}

int HWLinePortal::ClipPoint(const DVector2 &pos)
{
	if (P_PointOnLineSidePrecise(pos, line()))
	{
		return PClip_InFront;
	}
	return PClip_Inside;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Mirror Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

bool HWMirrorPortal::Setup(HWDrawInfo *di, Clipper *clipper)
{
	auto state = mOwner->mState;
	if (state->renderdepth > r_mirror_recursions)
	{
		return false;
	}

	auto &vp = di->Viewpoint;
	di->UpdateCurrentMapSection();

	di->mClipPortal = this;
	DAngle StartAngle = vp.Angles.Yaw;
	DVector3 StartPos = vp.Pos;

	vertex_t *v1 = linedef->v1;
	vertex_t *v2 = linedef->v2;

	// the player is always visible in a mirror.
	vp.showviewer = true;
	// Reflect the current view behind the mirror.
	if (linedef->Delta().X == 0)
	{
		// vertical mirror
		vp.Pos.X = 2 * v1->fX() - StartPos.X;

		// Compensation for reendering inaccuracies
		if (StartPos.X < v1->fX()) vp.Pos.X -= 0.1;
		else vp.Pos.X += 0.1;
	}
	else if (linedef->Delta().Y == 0)
	{
		// horizontal mirror
		vp.Pos.Y = 2 * v1->fY() - StartPos.Y;

		// Compensation for reendering inaccuracies
		if (StartPos.Y < v1->fY())  vp.Pos.Y -= 0.1;
		else vp.Pos.Y += 0.1;
	}
	else
	{
		// any mirror--use floats to avoid integer overflow. 
		// Use doubles to avoid losing precision which is very important here.

		double dx = v2->fX() - v1->fX();
		double dy = v2->fY() - v1->fY();
		double x1 = v1->fX();
		double y1 = v1->fY();
		double x = StartPos.X;
		double y = StartPos.Y;

		// the above two cases catch len == 0
		double r = ((x - x1)*dx + (y - y1)*dy) / (dx*dx + dy * dy);

		vp.Pos.X = (x1 + r * dx) * 2 - x;
		vp.Pos.Y = (y1 + r * dy) * 2 - y;

		// Compensation for reendering inaccuracies
		FVector2 v{-dx, dy};
		v.MakeUnit();

		vp.Pos.X += v[1] * state->renderdepth / 2;
		vp.Pos.Y += v[0] * state->renderdepth / 2;
	}
	vp.Angles.Yaw = linedef->Delta().Angle() * 2. - StartAngle;

	vp.ViewActor = nullptr;

	state->MirrorFlag++;
	di->SetClipLine(linedef);
	di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, !!(state->MirrorFlag & 1), !!(state->PlaneMirrorFlag & 1));

	clipper->Clear();

	angle_t af = di->FrustumAngle();
	if (af < ANGLE_180) clipper->SafeAddClipRangeRealAngles(vp.Angles.Yaw.BAMs() + af, vp.Angles.Yaw.BAMs() - af);

	clipper->SafeAddClipRange(linedef->v1, linedef->v2);
	return true;
}

void HWMirrorPortal::Shutdown(HWDrawInfo *di)
{
	mOwner->mState->MirrorFlag--;
}

const char *HWMirrorPortal::GetName() { return "Mirror"; }

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Line to line Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
bool HWLineToLinePortal::Setup(HWDrawInfo *di, Clipper *clipper)
{
	// TODO: Handle recursion more intelligently
	auto &state = mOwner->mState;
	if (state->renderdepth>r_mirror_recursions)
	{
		return false;
	}
	auto &vp = di->Viewpoint;
	di->mClipPortal = this;

	line_t *origin = glport->lines[0]->mOrigin;
	P_TranslatePortalXY(origin, vp.Pos.X, vp.Pos.Y);
	P_TranslatePortalXY(origin, vp.ActorPos.X, vp.ActorPos.Y);
	P_TranslatePortalAngle(origin, vp.Angles.Yaw);
	P_TranslatePortalZ(origin, vp.Pos.Z);
	P_TranslatePortalXY(origin, vp.Path[0].X, vp.Path[0].Y);
	P_TranslatePortalXY(origin, vp.Path[1].X, vp.Path[1].Y);
	if (!vp.showviewer && vp.camera != nullptr && P_PointOnLineSidePrecise(vp.Path[0].XY(), glport->lines[0]->mDestination) != P_PointOnLineSidePrecise(vp.Path[1].XY(), glport->lines[0]->mDestination))
	{
		double distp = (vp.Path[0] - vp.Path[1]).Length();
		if (distp > EQUAL_EPSILON)
		{
			double dist1 = (vp.Pos - vp.Path[0]).Length();
			double dist2 = (vp.Pos - vp.Path[1]).Length();

			if (dist1 + dist2 < distp + 1)
			{
				vp.camera->renderflags |= RF_MAYBEINVISIBLE;
			}
		}
	}

	auto &lines = mOwner->lines;

	for (unsigned i = 0; i < lines.Size(); i++)
	{
		line_t *line = lines[i].seg->linedef->getPortalDestination();
		subsector_t *sub;
		if (line->sidedef[0]->Flags & WALLF_POLYOBJ)
			sub = R_PointInSubsector(line->v1->fixX(), line->v1->fixY());
		else sub = line->frontsector->subsectors[0];
		di->CurrentMapSections.Set(sub->mapsection);
	}

	vp.ViewActor = nullptr;
	di->SetClipLine(glport->lines[0]->mDestination);
	di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, !!(state->MirrorFlag & 1), !!(state->PlaneMirrorFlag & 1));

	ClearClipper(di, clipper);
	return true;
}

void HWLineToLinePortal::RenderAttached(HWDrawInfo *di)
{
	di->ProcessActorsInPortal(glport, di->in_area);
}

const char *HWLineToLinePortal::GetName() { return "LineToLine"; }


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Skybox Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// GLSkyboxPortal::DrawContents
//
//-----------------------------------------------------------------------------

bool HWSkyboxPortal::Setup(HWDrawInfo *di, Clipper *clipper)
{
	auto state = mOwner->mState;
	old_pm = state->PlaneMirrorMode;

	if (mOwner->mState->skyboxrecursion >= 3)
	{
		return false;
	}
	auto &vp = di->Viewpoint;

	state->skyboxrecursion++;
	state->PlaneMirrorMode = 0;
	state->inskybox = true;

	AActor *origin = portal->mSkybox;
	portal->mFlags |= PORTSF_INSKYBOX;
	vp.extralight = 0;


	oldclamp = di->SetDepthClamp(false);
	vp.Pos = origin->InterpolatedPosition(vp.TicFrac);
	vp.ActorPos = origin->Pos();
	vp.Angles.Yaw += (origin->PrevAngles.Yaw + deltaangle(origin->PrevAngles.Yaw, origin->Angles.Yaw) * vp.TicFrac);

	// Don't let the viewpoint be too close to a floor or ceiling
	double floorh = origin->Sector->floorplane.ZatPoint(origin->Pos().XY());
	double ceilh = origin->Sector->ceilingplane.ZatPoint(origin->Pos().XY());
	if (vp.Pos.Z < floorh + 4) vp.Pos.Z = floorh + 4;
	if (vp.Pos.Z > ceilh - 4) vp.Pos.Z = ceilh - 4;

	vp.ViewActor = origin;

	di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, !!(state->MirrorFlag & 1), !!(state->PlaneMirrorFlag & 1));
	di->SetViewArea();
	ClearClipper(di, clipper);
	di->UpdateCurrentMapSection();
	return true;
}


void HWSkyboxPortal::Shutdown(HWDrawInfo *di)
{
	auto state = mOwner->mState;
	portal->mFlags &= ~PORTSF_INSKYBOX;
	di->SetDepthClamp(oldclamp);
	state->inskybox = false;
	state->skyboxrecursion--;
	state->PlaneMirrorMode = old_pm;
}

const char *HWSkyboxPortal::GetName() { return "Skybox"; }

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Sector stack Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// GLSectorStackPortal::SetupCoverage
//
//-----------------------------------------------------------------------------

static uint8_t SetCoverage(HWDrawInfo *di, void *node)
{
	if (level.nodes.Size() == 0)
	{
		return 0;
	}
	if (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;
		uint8_t coverage = SetCoverage(di, bsp->children[0]) | SetCoverage(di, bsp->children[1]);
		di->no_renderflags[bsp->Index()] = coverage;
		return coverage;
	}
	else
	{
		subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
		return di->ss_renderflags[sub->Index()] & SSRF_SEEN;
	}
}

void HWSectorStackPortal::SetupCoverage(HWDrawInfo *di)
{
	for (unsigned i = 0; i<subsectors.Size(); i++)
	{
		subsector_t *sub = subsectors[i];
		int plane = origin->plane;
		for (int j = 0; j<sub->portalcoverage[plane].sscount; j++)
		{
			subsector_t *dsub = &::level.subsectors[sub->portalcoverage[plane].subsectors[j]];
			di->CurrentMapSections.Set(dsub->mapsection);
			di->ss_renderflags[dsub->Index()] |= SSRF_SEEN;
		}
	}
	SetCoverage(di, ::level.HeadNode());
}

//-----------------------------------------------------------------------------
//
// GLSectorStackPortal::DrawContents
//
//-----------------------------------------------------------------------------
bool HWSectorStackPortal::Setup(HWDrawInfo *di, Clipper *clipper)
{
	auto state = mOwner->mState;
	FSectorPortalGroup *portal = origin;
	auto &vp = di->Viewpoint;

	vp.Pos += origin->mDisplacement;
	vp.ActorPos += origin->mDisplacement;
	vp.ViewActor = nullptr;

	// avoid recursions!
	if (origin->plane != -1) screen->instack[origin->plane]++;

	di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, !!(state->MirrorFlag & 1), !!(state->PlaneMirrorFlag & 1));
	SetupCoverage(di);
	ClearClipper(di, clipper);

	// If the viewpoint is not within the portal, we need to invalidate the entire clip area.
	// The portal will re-validate the necessary parts when its subsectors get traversed.
	subsector_t *sub = R_PointInSubsector(vp.Pos.XY());
	if (!(di->ss_renderflags[sub->Index()] & SSRF_SEEN))
	{
		clipper->SafeAddClipRange(0, ANGLE_MAX);
		clipper->SetBlocked(true);
	}
	return true;
}


void HWSectorStackPortal::Shutdown(HWDrawInfo *di)
{
	if (origin->plane != -1) screen->instack[origin->plane]--;
}

const char *HWSectorStackPortal::GetName() { return "Sectorstack"; }

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Plane Mirror Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// GLPlaneMirrorPortal::DrawContents
//
//-----------------------------------------------------------------------------

bool HWPlaneMirrorPortal::Setup(HWDrawInfo *di, Clipper *clipper)
{
	auto state = mOwner->mState;
	if (state->renderdepth > r_mirror_recursions)
	{
		return false;
	}
	// A plane mirror needs to flip the portal exclusion logic because inside the mirror, up is down and down is up.
	std::swap(screen->instack[sector_t::floor], screen->instack[sector_t::ceiling]);

	auto &vp = di->Viewpoint;
	old_pm = state->PlaneMirrorMode;

	// the player is always visible in a mirror.
	vp.showviewer = true;

	double planez = origin->ZatPoint(vp.Pos.XY());
	vp.Pos.Z = 2 * planez - vp.Pos.Z;
	vp.ViewActor = nullptr;
	state->PlaneMirrorMode = origin->fC() < 0 ? -1 : 1;

	state->PlaneMirrorFlag++;
	di->SetClipHeight(planez, state->PlaneMirrorMode < 0 ? -1.f : 1.f);
	di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, !!(state->MirrorFlag & 1), !!(state->PlaneMirrorFlag & 1));
	ClearClipper(di, clipper);

	di->UpdateCurrentMapSection();
	return true;
}

void HWPlaneMirrorPortal::Shutdown(HWDrawInfo *di)
{
	auto state = mOwner->mState;
	state->PlaneMirrorFlag--;
	state->PlaneMirrorMode = old_pm;
	std::swap(screen->instack[sector_t::floor], screen->instack[sector_t::ceiling]);
}

const char *HWPlaneMirrorPortal::GetName() { return origin->fC() < 0? "Planemirror ceiling" : "Planemirror floor"; }
