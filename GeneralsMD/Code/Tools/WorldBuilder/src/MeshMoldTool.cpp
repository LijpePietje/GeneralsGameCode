/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// MeshMoldTool.cpp
// Terrain shaping tool for worldbuilder.
// Author: John Ahlquist, Oct 2001


#include "StdAfx.h"
#include "resource.h"

#include "MeshMoldTool.h"
#include "CUndoable.h"
#include "MainFrm.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "WorldBuilder.h"
#include "DrawObject.h"
#include "wbview3d.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "W3DDevice/GameClient/HeightMap.h"
//
// MeshMoldTool class.
//

Bool MeshMoldTool::m_tracking = false;
Coord3D MeshMoldTool::m_toolPos;
WorldHeightMapEdit *MeshMoldTool::m_htMapEditCopy = nullptr;

/// Constructor
MeshMoldTool::MeshMoldTool() :
	Tool(ID_MOLD_TOOL, IDC_MOLD_POINTER)
{
	m_offsettingZ = false;
}

/// Destructor
MeshMoldTool::~MeshMoldTool()
{
	REF_PTR_RELEASE(m_htMapEditCopy);
}


/// Shows the mesh mold options panel.
void MeshMoldTool::activate()
{
	CMainFrame::GetMainFrame()->showOptionsDialog(IDD_MESHMOLD_OPTIONS);
	m_prevXIndex = -1;
	m_prevYIndex = -1;
	if (m_tracking) {
		DrawObject::setDoMeshFeedback(true);
	}
}

/// Shows the mesh mold options panel.
void MeshMoldTool::deactivate()
{
	m_tracking = false;
	DrawObject::setDoMeshFeedback(false);
	REF_PTR_RELEASE(m_htMapEditCopy);
	if (MeshMoldOptions::isDoingPreview()) {
		// Update the height map in case we were doing previews.
		IRegion2D partialRange = {0,0,0,0};
		CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
		if (pDoc) {
			pDoc->updateHeightMap(m_htMapEditCopy, false, partialRange);
		}
	}
}

/// Start tool.
/** Setup the tool to start mesh molding - make a copy of the height map
to edit, another copy because we need it :), and call mouseMovedDown. */
void MeshMoldTool::mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	if (m != TRACK_L) return;
	Coord3D cpt;
	pView->viewToDocCoords(viewPt, &cpt);
	if (!m_tracking) {
		m_tracking = true;
		m_toolPos = cpt;
		Real height = TheTerrainRenderObject->getHeightMapHeight(cpt.x, cpt.y, nullptr);
		MeshMoldOptions::setHeight(height);
	}
	DrawObject::setDoMeshFeedback(true);
	m_prevDocPt = cpt;
	Bool shiftKey = (0x8000 & ::GetAsyncKeyState(VK_SHIFT))!=0;
	if (shiftKey) {
		m_offsettingZ = true;
		m_prevViewPt = viewPt;
	} else {
		m_offsettingZ = false;
	}

	mouseMoved(m, viewPt, pView, pDoc);
}

/// End tool.
/** Finish the tool operation - create a command, pass it to the
doc to execute, and cleanup ref'd objects. */
void MeshMoldTool::mouseUp(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	if (m != TRACK_L) return;
	m_offsettingZ = false;
}

/// Track the mouse.
void MeshMoldTool::mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	if (m != TRACK_L) {
		return;
	}
	Coord3D cpt;
	pView->viewToDocCoords(viewPt, &cpt);
	if (m_offsettingZ) {
		Real deltaZ = (m_prevViewPt.y - viewPt.y)*MAP_HEIGHT_SCALE*0.5f;
		MeshMoldOptions::setHeight(MeshMoldOptions::getHeight()+deltaZ);
	}	else {
		m_toolPos.x += cpt.x-m_prevDocPt.x;
		m_toolPos.y += cpt.y-m_prevDocPt.y;
	}
	updateMeshLocation(false);
	m_prevViewPt = viewPt;
	m_prevDocPt = cpt;
}


///  Update the mesh location onscreen.
void MeshMoldTool::updateMeshLocation(Bool changePreview)
{
	if (m_tracking) {
		m_toolPos.z = MeshMoldOptions::getHeight();
		DrawObject::setFeedbackPos(m_toolPos);
		if (MeshMoldOptions::isDoingPreview() || changePreview) {
			// doing preview
			CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
			if (MeshMoldOptions::isDoingPreview()) {
				applyMesh(pDoc);
			} else if (m_htMapEditCopy) {
				// Unapply the mesh :)
				Int i, j;
				WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
				for (j=0; j<m_htMapEditCopy->getYExtent(); j++) {
					for (i=0; i<m_htMapEditCopy->getXExtent(); i++) {
						m_htMapEditCopy->setHeight(i, j, pMap->getHeight(i, j));
					}
				}
				IRegion2D partialRange = {0,0,0,0};
				pDoc->updateHeightMap(m_htMapEditCopy, false, partialRange);
			}
		}
	}
}

// TheSuperHackers @feature Nemellud 04/07/2026 EmbeddedMode: first pipe placement snaps mold height to terrain, matching native mouseDown
void MeshMoldTool::setToolPosY(float y)
{
	m_toolPos.y = y;
	if (!m_tracking) {
		m_tracking = true;
		if (TheTerrainRenderObject) {
			Real height = TheTerrainRenderObject->getHeightMapHeight(m_toolPos.x, m_toolPos.y, nullptr);
			MeshMoldOptions::setHeight(height);
		}
	}
}

/// Apply the tool.
/** Apply the height mesh mold at the current point. */
void MeshMoldTool::apply(CWorldBuilderDoc *pDoc)
{
	applyMesh(pDoc);
	WBDocUndoable *pUndo = new WBDocUndoable(pDoc, m_htMapEditCopy);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	REF_PTR_RELEASE(m_htMapEditCopy);

}

/// Apply the tool.
/** Apply the height mesh mold at the current point. */
void MeshMoldTool::applyMesh(CWorldBuilderDoc *pDoc)
{
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();

	HCURSOR old = SetCursor(::LoadCursor(nullptr, IDC_WAIT));
	if (m_htMapEditCopy == nullptr) {
		m_htMapEditCopy = pDoc->GetHeightMap()->duplicate();
	}	else {
		// Restore original heights to edit copy.
		Int i, j;
		for (j=0; j<m_htMapEditCopy->getYExtent(); j++) {
			for (i=0; i<m_htMapEditCopy->getXExtent(); i++) {
				m_htMapEditCopy->setHeight(i, j, pMap->getHeight(i, j));
			}
		}
	}
	Int border = m_htMapEditCopy->getBorderSize();
	// TheSuperHackers @feature Nemellud 04/07/2026 EmbeddedMode: GetActive3DView() relies on MDI focus; fallback to s_instance
	WbView3d *p3View = pDoc->GetActive3DView();
	if (!p3View) p3View = WbView3d::s_instance;
	if (p3View) {
		DrawObject *pDraw = p3View->getDrawObject();
		if (pDraw) {
			// TheSuperHackers @feature Nemellud 04/07/2026 EmbeddedMode: force mesh load for pipe-driven apply
			pDraw->updateMeshVB();
			MeshClass *pMesh = pDraw->peekMesh();
			if (pMesh) {
				SphereClass bounds;
				pDraw->getMeshBounds(&bounds);
				// TheSuperHackers @bugfix Nemellud 04/07/2026 EmbeddedMode: per-axis bounds so non-uniform stretch is not clipped to the unstretched radius
				Real boundsSX = MeshMoldOptions::getScale() * MeshMoldOptions::getScaleX();
				Real boundsSY = MeshMoldOptions::getScale() * MeshMoldOptions::getScaleY();
				Vector3 boundsCenter = bounds.Center;
				boundsCenter.Rotate_Z(MeshMoldOptions::getAngle()*PI/180.0f);
				Real boundsCX = boundsCenter.X * boundsSX;
				Real boundsCY = boundsCenter.Y * boundsSY;
				Real boundsRX = bounds.Radius * boundsSX;
				Real boundsRY = bounds.Radius * boundsSY;
				Int minX = ceil((m_toolPos.x+boundsCX-boundsRX)/MAP_XY_FACTOR);
				Int minY = ceil((m_toolPos.y+boundsCY-boundsRY)/MAP_XY_FACTOR);
				Int maxX = floor((m_toolPos.x+boundsCX+boundsRX)/MAP_XY_FACTOR);
				Int maxY = floor((m_toolPos.y+boundsCY+boundsRY)/MAP_XY_FACTOR);
				maxX++; maxY++;
				if (minX<0) minX = 0;
				if (minY<0) minY = 0;
				if (maxX > m_htMapEditCopy->getXExtent()) {
					maxX = m_htMapEditCopy->getXExtent();
				}
				if (maxY > m_htMapEditCopy->getYExtent()) {
					maxY = m_htMapEditCopy->getYExtent();
				}
				Int i, j;
				for (j=minY; j<maxY; j++) {
					for (i=minX; i<maxX; i++) {
						Real X, Y;
						X = i*MAP_XY_FACTOR;
						X -= m_toolPos.x;
						Y = j*MAP_XY_FACTOR;
						Y -= m_toolPos.y;
						// TheSuperHackers @feature Nemellud 04/07/2026 EmbeddedMode: non-uniform X/Y stretch
						X /= (MeshMoldOptions::getScale() * MeshMoldOptions::getScaleX());
						Y /= (MeshMoldOptions::getScale() * MeshMoldOptions::getScaleY());
						Vector3 vLoc(X, Y, 10000);
						vLoc.Rotate_Z(-MeshMoldOptions::getAngle()*PI/180.0f);

						LineSegClass ray(vLoc, Vector3(vLoc.X, vLoc.Y, -10000));
						CastResultStruct castResult;
						castResult.ComputeContactPoint = true;
						RayCollisionTestClass rayCollide(ray, &castResult) ;
						rayCollide.CollisionType = 0xFFFFFFFF;
						if (pMesh->Cast_Ray(rayCollide)) {
							// get the point of intersection according to W3D
							Vector3 intersection = castResult.ContactPoint;
							intersection.Z *= MeshMoldOptions::getScale();
							Int newHeight = floor( ((intersection.Z+MeshMoldOptions::getHeight())/MAP_HEIGHT_SCALE)+0.5);
							// check boundary values
							if (newHeight < m_htMapEditCopy->getMinHeightValue()) newHeight = m_htMapEditCopy->getMinHeightValue();
							if (newHeight > m_htMapEditCopy->getMaxHeightValue()) newHeight = m_htMapEditCopy->getMaxHeightValue();
							Bool apply = true;
							if (MeshMoldOptions::isRaisingOnly()) {
								apply = newHeight > m_htMapEditCopy->getHeight(i+border, j+border);
							} else if (MeshMoldOptions::isLoweringOnly()) {
								apply = newHeight < m_htMapEditCopy->getHeight(i+border, j+border);
							}
							if (apply) {
								m_htMapEditCopy->setHeight(i+border, j+border, newHeight);
							}
						}
					}
				}
				IRegion2D partialRange;
				// Offset by 2, because the normals change out two past the actual height change.
				partialRange.lo.x = minX-2+border;
				partialRange.hi.x = maxX+2+border;
				partialRange.lo.y = minY-2+border;
				partialRange.hi.y = maxY+2+border;
				pDoc->updateHeightMap(m_htMapEditCopy, true, partialRange);

			}
		}
	}
	if (old) SetCursor(old);
}
