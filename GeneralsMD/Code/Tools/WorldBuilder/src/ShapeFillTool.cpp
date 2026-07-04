/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// ShapeFillTool.cpp
// Core: static state, tool lifecycle, shape management, and persistence.
// Mouse handling: ShapeFillMouse.cpp
// Rasterization: ShapeFillRasterize.cpp
// Drawing: ShapeFillDraw.cpp
// TheSuperHackers @feature Nemellud 09/05/2026 ShapeFillTool core: static state, lifecycle, shape management, undo/redo, and persistence

#include "StdAfx.h"
#include "resource.h"

#include "ShapeFillTool.h"
#include "ShapeFillOptions.h"
#include "CUndoable.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "wbview3d.h"
#include "MainFrm.h"
#include "DrawObject.h"

#include <algorithm>
#include <cmath>

// -------------------------------------------------------------------------
// Static state definitions
// -------------------------------------------------------------------------

std::vector<ShapeDef> ShapeFillTool::m_shapes;
Int                   ShapeFillTool::m_nextId      = 1;
Int                   ShapeFillTool::m_selectedId  = -1;
ShapeDef              ShapeFillTool::m_clipboard;
Bool                  ShapeFillTool::m_hasClipboard = false;
Bool                  ShapeFillTool::m_isActive     = false;

SFToolMode ShapeFillTool::m_mode            = SF_DRAW_RECT;
Int        ShapeFillTool::m_innerHeight     = 10;
Bool       ShapeFillTool::m_autoBlend       = true;   // shapes: Out by default
Bool       ShapeFillTool::m_blendInward     = false;
Bool       ShapeFillTool::m_fillAutoBlend   = true;   // fill: In by default
Bool       ShapeFillTool::m_fillBlendInward = true;
Bool       ShapeFillTool::m_innerAutoBlend    = true;   // inner zone: Out by default
Bool       ShapeFillTool::m_innerBlendInward = false;
Int        ShapeFillTool::m_borderWidth     = 5;
Int        ShapeFillTool::m_innerTexClass  = -1;
Int        ShapeFillTool::m_borderTexClass = -1;
Bool       ShapeFillTool::m_autoSave      = true;

Bool     ShapeFillTool::m_hasDraft   = false;
ShapeDef ShapeFillTool::m_draftShape;

Bool                    ShapeFillTool::m_polyDrawing = false;
std::vector<ShapeVertex> ShapeFillTool::m_polyDraft;

std::vector<LineDef>    ShapeFillTool::m_lines;
Int                     ShapeFillTool::m_nextLineId   = 1;
Int                     ShapeFillTool::m_selectedLineId = -1;
Bool                    ShapeFillTool::m_lineDrawing   = false;
std::vector<ShapeVertex> ShapeFillTool::m_lineDraft;

Bool ShapeFillTool::m_hasSnapCorner = false;
Int  ShapeFillTool::m_snapCx = 0;
Int  ShapeFillTool::m_snapCy = 0;

// -------------------------------------------------------------------------
// Misc helpers
// -------------------------------------------------------------------------

void ShapeFillTool::invalidateBothViews()
{
	CWorldBuilderView* p2D = CWorldBuilderDoc::GetActive2DView();
	WbView3d*          p3D = CWorldBuilderDoc::GetActive3DView();
	if (p2D) p2D->Invalidate(false);
	if (p3D) p3D->Invalidate(false);
}

CString ShapeFillTool::shapefillPath(const CString& mapPath)
{
	int dot = mapPath.ReverseFind('.');
	return (dot >= 0 ? mapPath.Left(dot) : mapPath) + ".wbsession";
}

// -------------------------------------------------------------------------
// Undo/redo helpers
// -------------------------------------------------------------------------

void ShapeFillUndoable::Do()   { ShapeFillTool::restoreSnapshot(m_after);  }
void ShapeFillUndoable::Undo() { ShapeFillTool::restoreSnapshot(m_before); }

ShapeFillSnapshot ShapeFillTool::captureSnapshot()
{
	ShapeFillSnapshot snap;
	snap.shapes     = m_shapes;
	snap.lines      = m_lines;
	snap.selectedId = m_selectedId;
	snap.nextId     = m_nextId;
	snap.nextLineId = m_nextLineId;
	return snap;
}

void ShapeFillTool::restoreSnapshot(const ShapeFillSnapshot& snap)
{
	m_shapes      = snap.shapes;
	m_lines       = snap.lines;
	m_selectedId  = snap.selectedId;
	m_nextId      = snap.nextId;
	m_nextLineId  = snap.nextLineId;
	ShapeFillOptions::updateFromTool();
	invalidateBothViews();
}

void ShapeFillTool::pushUndo(CWorldBuilderDoc* pDoc, const ShapeFillSnapshot& before)
{
	if (!pDoc) return;
	ShapeFillUndoable* pUndo = new ShapeFillUndoable(before, captureSnapshot());
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo);
}

CWorldBuilderDoc* ShapeFillTool::getActiveDoc()
{
	CWnd* pWnd = AfxGetMainWnd();
	CFrameWnd* pFrame = pWnd ? (CFrameWnd*)pWnd : nullptr;
	return pFrame ? (CWorldBuilderDoc*)pFrame->GetActiveDocument() : nullptr;
}

// -------------------------------------------------------------------------
// Constructor / Destructor
// -------------------------------------------------------------------------

ShapeFillTool::ShapeFillTool() :
	Tool(ID_SHAPE_FILL_TOOL, IDC_BRUSH_CROSS),
	m_prevTopDown(false),
	m_dragging(false),
	m_dragStartTx(0), m_dragStartTy(0),
	m_movingShape(false),
	m_resizingHandle(false),
	m_moveStartTx(0), m_moveStartTy(0),
	m_movingLinePoint(false),
	m_activeLineIdx(-1),
	m_activePointIdx(-1),
	m_hasDragSnapshot(false)
{
}

ShapeFillTool::~ShapeFillTool()
{
}

// -------------------------------------------------------------------------
// Tool lifecycle
// -------------------------------------------------------------------------

void ShapeFillTool::activate()
{
	m_isActive = true;
	CMainFrame::GetMainFrame()->showOptionsDialog(IDD_SHAPE_FILL_OPTIONS);
	DrawObject::setDoBrushFeedback(false);
	ShapeFillOptions::updateTopDownState();
}

void ShapeFillTool::deactivate()
{
	m_isActive        = false;
	m_dragging        = false;
	m_movingShape     = false;
	m_resizingHandle  = false;
	m_movingLinePoint = false;
	m_polyDrawing     = false;
	m_polyDraft.clear();
}

// -------------------------------------------------------------------------
// Mode
// -------------------------------------------------------------------------

void ShapeFillTool::setMode(SFToolMode mode)
{
	m_mode = mode;
	if (mode != SF_DRAW_POLYGON) {
		m_polyDrawing = false;
		m_polyDraft.clear();
	}
	if (mode != SF_DRAW_LINE) {
		// Auto-commit any in-progress line draft instead of discarding it
		if (m_lineDrawing && m_lineDraft.size() >= 2) {
			LineDef line;
			line.id     = m_nextLineId++;
			line.points = m_lineDraft;
			m_lines.push_back(line);
		}
		m_lineDrawing = false;
		m_lineDraft.clear();
	}
}

// -------------------------------------------------------------------------
// Polygon finish
// -------------------------------------------------------------------------

void ShapeFillTool::finishPolygon()
{
	if (!m_polyDrawing || m_polyDraft.size() < 3) return;

	auto before = captureSnapshot();

	ShapeDef shape;
	shape.id             = m_nextId++;
	shape.type           = SHAPE_POLYGON;
	shape.points         = m_polyDraft;
	shape.borderWidth    = m_borderWidth;
	shape.innerHeight    = m_innerHeight;
	shape.autoBlendOuter = m_autoBlend;
	shape.innerTexClass  = m_innerTexClass;
	shape.borderTexClass = m_borderTexClass;

	m_shapes.push_back(shape);
	m_selectedId  = shape.id;
	m_polyDrawing = false;
	m_polyDraft.clear();
	setMode(SF_SELECT);
	ShapeFillOptions::updateFromTool();

	if (CWorldBuilderDoc* pDoc = getActiveDoc())
		pushUndo(pDoc, before);
}

// -------------------------------------------------------------------------
// Apply to terrain
// -------------------------------------------------------------------------

// ShapeFillApplyUndoable — declared in ShapeFillTool.h; implementations here.

ShapeFillApplyUndoable::ShapeFillApplyUndoable(CWorldBuilderDoc* pDoc,
	WorldHeightMapEdit* before, WorldHeightMapEdit* after, IRegion2D range)
	: m_pDoc(pDoc), m_pBefore(nullptr), m_pAfter(nullptr), m_range(range)
{
	REF_PTR_SET(m_pBefore, before);
	REF_PTR_SET(m_pAfter,  after);
}

ShapeFillApplyUndoable::~ShapeFillApplyUndoable()
{
	REF_PTR_RELEASE(m_pBefore);
	REF_PTR_RELEASE(m_pAfter);
}

void ShapeFillApplyUndoable::Do()
{
	m_pDoc->SetHeightMap(m_pAfter, false); // view already rendered — just register officially
}

void ShapeFillApplyUndoable::Undo()
{
	m_pBefore->resetResources();
	m_pBefore->getTerrainTexture();
	m_pDoc->SetHeightMap(m_pBefore, true);
}

void ShapeFillApplyUndoable::Redo()
{
	m_pAfter->resetResources();
	m_pAfter->getTerrainTexture();
	m_pDoc->SetHeightMap(m_pAfter, true);
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: add shape programmatically from pipe
Int ShapeFillTool::addShape(ShapeDef def)
{
	def.id = m_nextId++;
	m_shapes.push_back(def);
	m_selectedId = def.id;
	return def.id;
}

// TheSuperHackers @feature Nemellud 10/05/2026 ShapeFillTool: apply shape to heightmap with
// inner/border textures and three independent blend groups (outer, inner, fill).
void ShapeFillTool::applySelectedShape(CWorldBuilderDoc* pDoc)
{
	if (m_selectedId < 0) return;
	ShapeDef* shape = findShape(m_selectedId);
	if (!shape) return;

	AfxGetApp()->BeginWaitCursor(); // show hourglass while apply is running

	shape->innerHeight    = m_innerHeight;
	shape->autoBlendOuter = m_autoBlend;
	shape->borderWidth    = m_borderWidth;
	shape->innerTexClass  = m_innerTexClass;
	shape->borderTexClass = m_borderTexClass;

	// Modify the live heightmap in-place so partial updateHeightMap (same pointer) works.
	WorldHeightMapEdit* pHM      = pDoc->GetHeightMap();
	WorldHeightMapEdit* htBefore = pHM->duplicate(); // pre-apply snapshot for Undo
	Int border = pHM->getBorderSize();
	Int mapW   = pHM->getXExtent();
	Int mapH   = pHM->getYExtent();
	TileSet tiles = rasterize(*shape);
	Bool needsOptimize = false;

	// Effective textures: border falls back to inner, inner falls back to border
	Int borderTex = (shape->borderTexClass >= 0) ? shape->borderTexClass : shape->innerTexClass;
	Int innerTex  = (shape->innerTexClass   >= 0) ? shape->innerTexClass  : borderTex;

	// Bounding box of modified tiles for partial render update
	Int rMinX = mapW, rMinY = mapH, rMaxX = 0, rMaxY = 0;
	for (const CPoint& pt : tiles.inner) {
		Int hx = pt.x + border, hy = pt.y + border;
		if (hx >= 0 && hy >= 0 && hx < mapW && hy < mapH) {
			if (hx < rMinX) rMinX = hx; if (hx > rMaxX) rMaxX = hx;
			if (hy < rMinY) rMinY = hy; if (hy > rMaxY) rMaxY = hy;
		}
	}
	for (const BorderTile& bt : tiles.border) {
		Int hx = bt.pt.x + border, hy = bt.pt.y + border;
		if (hx >= 0 && hy >= 0 && hx < mapW && hy < mapH) {
			if (hx < rMinX) rMinX = hx; if (hx > rMaxX) rMaxX = hx;
			if (hy < rMinY) rMinY = hy; if (hy > rMaxY) rMaxY = hy;
		}
	}

	// Apply inner zone
	for (const CPoint& pt : tiles.inner) {
		Int hx = pt.x + border, hy = pt.y + border;
		if (hx < 0 || hy < 0 || hx >= mapW || hy >= mapH) continue;
		if (pHM->getHeight(hx, hy) != (UnsignedByte)shape->innerHeight)
			pHM->setHeight(hx, hy, (UnsignedByte)shape->innerHeight);
		if (innerTex >= 0)
			if (pHM->setTileNdx(hx, hy, innerTex, false))
				needsOptimize = true;
	}

	// Apply border zone — distance-based gradient
	float bwf = (shape->borderWidth > 0) ? (float)shape->borderWidth : 1.0f;
	// When a free inner polygon is active, bt.dist is already normalized [0,1]
	bool hasInnerPoly = ((Int)shape->innerPoints.size() >= 3 &&
	                     (shape->type == SHAPE_POLYGON || shape->type == SHAPE_RECT || shape->type == SHAPE_CIRCLE));

	// For autoBlendOuter: sample terrain just OUTSIDE the shape boundary so
	// the reference height is stable across multiple Apply calls (idempotent).
	// Outer tiles are never in tiles.inner/border, so reading from pHM is safe.
	auto sampleOuterTerrain = [&](Int tx, Int ty) -> Int {
		Int ox = tx, oy = ty;
		if (shape->type == SHAPE_RECT) {
			Int minX = std::min(shape->x0, shape->x1), maxX = std::max(shape->x0, shape->x1);
			Int minY = std::min(shape->y0, shape->y1), maxY = std::max(shape->y0, shape->y1);
			float px = tx + 0.5f, py = ty + 0.5f;
			float dL = px - minX, dR = maxX - px, dT = py - minY, dB = maxY - py;
			float m = std::min({dL, dR, dT, dB});
			if      (dL <= m + 0.01f) ox = minX - 1;
			else if (dR <= m + 0.01f) ox = maxX;
			else if (dT <= m + 0.01f) oy = minY - 1;
			else                      oy = maxY;
		} else if (shape->type == SHAPE_CIRCLE) {
			float dx = tx - shape->cx + 0.5f, dy = ty - shape->cy + 0.5f;
			float d = sqrtf(dx*dx + dy*dy);
			if (d < 0.5f) { ox = shape->cx + shape->r + 1; }
			else {
				ox = (Int)((float)shape->cx + dx * ((float)(shape->r + 1)) / d);
				oy = (Int)((float)shape->cy + dy * ((float)(shape->r + 1)) / d);
			}
		} else if ((Int)shape->points.size() >= 3) {
			float px = tx + 0.5f, py = ty + 0.5f;
			float bestD = FLT_MAX, bestCx = px, bestCy = py, bestNx = 0, bestNy = 1;
			Int n = (Int)shape->points.size();
			for (Int i = 0; i < n; i++) {
				Int j = (i + 1) % n;
				float ax = (float)(shape->points[j].tx - shape->points[i].tx);
				float ay = (float)(shape->points[j].ty - shape->points[i].ty);
				float bx = px - shape->points[i].tx, by = py - shape->points[i].ty;
				float len2 = ax*ax + ay*ay;
				float t2 = (len2 > 0) ? std::max(0.0f, std::min(1.0f, (bx*ax+by*ay)/len2)) : 0.0f;
				float cX = shape->points[i].tx + t2*ax, cY = shape->points[i].ty + t2*ay;
				float ddx = px - cX, ddy = py - cY;
				float d = sqrtf(ddx*ddx + ddy*ddy);
				if (d < bestD) {
					bestD = d; bestCx = cX; bestCy = cY;
					bestNx = (d > 0.01f) ? ddx/d : 0;
					bestNy = (d > 0.01f) ? ddy/d : 1;
				}
			}
			ox = (Int)roundf(bestCx + bestNx);
			oy = (Int)roundf(bestCy + bestNy);
		}
		Int hx = std::max(0, std::min(mapW - 1, ox + border));
		Int hy = std::max(0, std::min(mapH - 1, oy + border));
		return (Int)pHM->getHeight(hx, hy);
	};

	for (const BorderTile& bt : tiles.border) {
		Int hx = bt.pt.x + border, hy = bt.pt.y + border;
		if (hx < 0 || hy < 0 || hx >= mapW || hy >= mapH) continue;
		float t = hasInnerPoly ? bt.dist : std::max(0.0f, std::min(1.0f, bt.dist / bwf));
		Int existingH = pHM->getHeight(hx, hy);
		Int outerH = sampleOuterTerrain(bt.pt.x, bt.pt.y);
		Int h = (Int)(outerH + t * (shape->innerHeight - outerH));
		h = std::max(0, std::min(80, h));
		if (existingH != h)
			pHM->setHeight(hx, hy, (UnsignedByte)h);
		if (borderTex >= 0)
			if (pHM->setTileNdx(hx, hy, borderTex, false))
				needsOptimize = true;
	}

	const Int dx4[] = {1, -1, 0,  0};
	const Int dy4[] = {0,  0, 1, -1};

	// Build shared lookups used by both blend sections
	std::vector<bool> isInnerTile(mapW * mapH, false);
	for (const CPoint& pt : tiles.inner) {
		Int hx = pt.x + border, hy = pt.y + border;
		if (hx >= 0 && hy >= 0 && hx < mapW && hy < mapH)
			isInnerTile[hy * mapW + hx] = true;
	}
	std::vector<bool> isBorderTile(mapW * mapH, false);
	for (const BorderTile& bt2 : tiles.border) {
		Int hx = bt2.pt.x + border, hy = bt2.pt.y + border;
		if (hx >= 0 && hy >= 0 && hx < mapW && hy < mapH)
			isBorderTile[hy * mapW + hx] = true;
	}

	// --- Inner boundary blend "Out" (before optimizeTiles) ---
	// autoBlendOut on inner (rocks) tiles: flood-fill stays within inner zone, no side effects.
	// Result: cliff tiles at inner edge show rocks bleeding in.
	if (m_innerAutoBlend && !m_innerBlendInward && innerTex >= 0 && !tiles.border.empty()) {
		for (const CPoint& pt : tiles.inner) {
			Int hx = pt.x + border, hy = pt.y + border;
			if (hx < 0 || hy < 0 || hx >= mapW || hy >= mapH) continue;
			bool adjToBorder = false;
			for (Int d = 0; d < 4 && !adjToBorder; d++) {
				Int nx = hx + dx4[d], ny = hy + dy4[d];
				if (nx >= 0 && ny >= 0 && nx < mapW && ny < mapH && isBorderTile[ny * mapW + nx])
					adjToBorder = true;
			}
			if (!adjToBorder) continue;
			pHM->autoBlendOut(hx, hy);
		}
		needsOptimize = true;
	}

	// Bake texture changes + inner "Out" before per-tile blend operations.
	if (needsOptimize)
		pHM->optimizeTiles();

	// --- Inner boundary blend "In" (after optimizeTiles, per-tile via blendTile, no flood-fill) ---
	// blendTile(rocks_tile, adjacent_cliff_tile): rocks show cliff bleeding in.
	// Unlike autoBlendOut(cliff), this targets only the inner boundary — no outer side effects.
	if (m_innerAutoBlend && m_innerBlendInward && innerTex >= 0 && borderTex >= 0 && !tiles.border.empty()) {
		for (const CPoint& pt : tiles.inner) {
			Int hx = pt.x + border, hy = pt.y + border;
			if (hx < 0 || hy < 0 || hx >= mapW || hy >= mapH) continue;
			Int bx = -1, by = -1;
			for (Int d = 0; d < 4 && bx < 0; d++) {
				Int nx = hx + dx4[d], ny = hy + dy4[d];
				if (nx < 0 || ny < 0 || nx >= mapW || ny >= mapH) continue;
				if (isBorderTile[ny * mapW + nx]) { bx = nx; by = ny; }
			}
			if (bx < 0) continue;
			pHM->blendTile(hx, hy, bx, by, -1, -1);
		}
	}

	// --- Outer boundary blend (after optimizeTiles, per-tile via blendTile, no flood-fill) ---
	// Out: blendTile(exterior_tile, adjacent_cliff_tile) -> exterior shows cliff bleeding in.
	// In:  blendTile(cliff_tile, adjacent_exterior_tile) -> cliff shows exterior bleeding in.
	if (shape->autoBlendOuter && borderTex >= 0) {
		float blendThresh = hasInnerPoly ? 0.3f : 2.0f;
		std::vector<bool> inShape(mapW * mapH, false);
		for (Int i = 0; i < mapW * mapH; i++)
			inShape[i] = isInnerTile[i] || isBorderTile[i];
		std::vector<bool> done(mapW * mapH, false);
		if (!m_blendInward) {
			for (const BorderTile& bt : tiles.border) {
				if (bt.dist >= blendThresh) continue;
				Int bhx = bt.pt.x + border, bhy = bt.pt.y + border;
				for (Int d = 0; d < 4; d++) {
					Int hx = bhx + dx4[d], hy = bhy + dy4[d];
					if (hx < 0 || hy < 0 || hx >= mapW || hy >= mapH) continue;
					if (inShape[hy * mapW + hx]) continue;
					if (done[hy * mapW + hx]) continue;
					done[hy * mapW + hx] = true;
					pHM->blendTile(hx, hy, bhx, bhy, -1, -1);
				}
			}
		} else {
			for (const BorderTile& bt : tiles.border) {
				if (bt.dist >= blendThresh) continue;
				Int hx = bt.pt.x + border, hy = bt.pt.y + border;
				if (hx < 0 || hy < 0 || hx >= mapW || hy >= mapH) continue;
				if (done[hy * mapW + hx]) continue;
				done[hy * mapW + hx] = true;
				Int ex = -1, ey = -1;
				for (Int d = 0; d < 4 && ex < 0; d++) {
					Int nx = hx + dx4[d], ny = hy + dy4[d];
					if (nx < 0 || ny < 0 || nx >= mapW || ny >= mapH) continue;
					if (!inShape[ny * mapW + nx]) { ex = nx; ey = ny; }
				}
				if (ex >= 0)
					pHM->blendTile(hx, hy, ex, ey, -1, -1);
			}
		}
	}

	// After optimizeTiles(), m_terrainTex is freed — partial update would corrupt tiles outside
	// the shape range. Full update required. Partial is only safe when atlas is unchanged.
	const Int BM = 4; // margin for blend effects that reach 1-2 tiles outside the shape
	IRegion2D shapeRange;
	if (rMaxX >= rMinX && rMaxY >= rMinY) {
		shapeRange = { std::max(0, rMinX - BM), std::max(0, rMinY - BM),
		               std::min(mapW, rMaxX + BM + 1), std::min(mapH, rMaxY + BM + 1) };
	} else {
		shapeRange = {0, 0, 0, 0}; // empty shape — no tiles changed
	}
	if (needsOptimize)
		pDoc->updateHeightMap(pHM, false, shapeRange); // full update — atlas was rebuilt
	else
		pDoc->updateHeightMap(pHM, true, shapeRange);  // partial update safe — atlas unchanged
	ShapeFillApplyUndoable* pUndo = new ShapeFillApplyUndoable(pDoc, htBefore, pHM, shapeRange);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo);
	REF_PTR_RELEASE(htBefore);
	AfxGetApp()->EndWaitCursor();
	invalidateBothViews();
}

// -------------------------------------------------------------------------
// Shape management
// -------------------------------------------------------------------------

void ShapeFillTool::syncSelectedFromPanel()
{
	for (auto& s : m_shapes) {
		if (s.id == m_selectedId) {
			s.borderWidth    = m_borderWidth;
			s.innerHeight    = m_innerHeight;
			s.autoBlendOuter = m_autoBlend;
			s.innerTexClass  = m_innerTexClass;
			s.borderTexClass = m_borderTexClass;
			break;
		}
	}
}

void ShapeFillTool::deleteSelectedShape()
{
	auto before = captureSnapshot();

	if (m_selectedId >= 0) {
		m_shapes.erase(
			std::remove_if(m_shapes.begin(), m_shapes.end(),
				[](const ShapeDef& s) { return s.id == m_selectedId; }),
			m_shapes.end());
		m_selectedId = -1;
	} else if (m_selectedLineId >= 0) {
		Int lineId = m_selectedLineId;
		m_lines.erase(
			std::remove_if(m_lines.begin(), m_lines.end(),
				[lineId](const LineDef& l) { return l.id == lineId; }),
			m_lines.end());
		m_selectedLineId = -1;
	} else {
		return;
	}

	ShapeFillOptions::updateFromTool();
	invalidateBothViews();

	if (CWorldBuilderDoc* pDoc = getActiveDoc())
		pushUndo(pDoc, before);
}

void ShapeFillTool::copySelectedShape()
{
	for (const auto& s : m_shapes) {
		if (s.id == m_selectedId) {
			m_clipboard    = s;
			m_hasClipboard = true;
			return;
		}
	}
}

void ShapeFillTool::pasteShape()
{
	if (!m_hasClipboard) return;

	auto before = captureSnapshot();

	ShapeDef copy    = m_clipboard;
	copy.id          = m_nextId++;
	const Int offset = 10; // tiles

	if (copy.type == SHAPE_RECT) {
		copy.x0 += offset; copy.y0 += offset;
		copy.x1 += offset; copy.y1 += offset;
	} else if (copy.type == SHAPE_CIRCLE) {
		copy.cx += offset; copy.cy += offset;
	} else {
		for (auto& pt : copy.points) { pt.tx += offset; pt.ty += offset; }
	}
	for (auto& pt : copy.innerPoints) { pt.tx += offset; pt.ty += offset; }

	m_shapes.push_back(copy);
	m_selectedId = copy.id;

	if (CWorldBuilderDoc* pDoc = getActiveDoc())
		pushUndo(pDoc, before);
}

void ShapeFillTool::flipSelectedShape(Bool horizontal)
{
	ShapeDef* shape = findShape(m_selectedId);
	if (!shape) return;

	auto before = captureSnapshot();

	if (shape->type == SHAPE_RECT) {
		if (horizontal) std::swap(shape->x0, shape->x1);
		else             std::swap(shape->y0, shape->y1);
		if (!shape->innerPoints.empty()) {
			Int minX = std::min(shape->x0, shape->x1), maxX = std::max(shape->x0, shape->x1);
			Int minY = std::min(shape->y0, shape->y1), maxY = std::max(shape->y0, shape->y1);
			Int cx2 = minX + maxX, cy2 = minY + maxY;
			for (auto& pt : shape->innerPoints) {
				if (horizontal) pt.tx = cx2 - pt.tx;
				else             pt.ty = cy2 - pt.ty;
			}
		}
	} else if (shape->type == SHAPE_CIRCLE) {
		// Circle is symmetric — no change needed
	} else {
		Int minX = INT_MAX, maxX = INT_MIN, minY = INT_MAX, maxY = INT_MIN;
		for (const auto& pt : shape->points) {
			minX = std::min(minX, pt.tx); maxX = std::max(maxX, pt.tx);
			minY = std::min(minY, pt.ty); maxY = std::max(maxY, pt.ty);
		}
		Int cx2 = minX + maxX;
		Int cy2 = minY + maxY;
		for (auto& pt : shape->points) {
			if (horizontal) pt.tx = cx2 - pt.tx;
			else             pt.ty = cy2 - pt.ty;
		}
		for (auto& pt : shape->innerPoints) {
			if (horizontal) pt.tx = cx2 - pt.tx;
			else             pt.ty = cy2 - pt.ty;
		}
	}

	if (CWorldBuilderDoc* pDoc = getActiveDoc())
		pushUndo(pDoc, before);
}

// TheSuperHackers @feature Nemellud 25/05/2026 ShapeFillTool: rotate selected shape 90° CW
void ShapeFillTool::rotateSelectedShape()
{
	ShapeDef* shape = findShape(m_selectedId);
	if (!shape) return;

	auto before = captureSnapshot();

	// 90° CW in tile coords (Y-down screen space): dx'=-dy, dy'=dx
	auto rotatePts = [](std::vector<ShapeVertex>& pts, Int cx2, Int cy2) {
		for (auto& pt : pts) {
			Int dx = 2 * pt.tx - cx2;
			Int dy = 2 * pt.ty - cy2;
			pt.tx = (cx2 - dy) / 2;
			pt.ty = (cy2 + dx) / 2;
		}
	};

	if (shape->type == SHAPE_RECT) {
		Int cx2 = shape->x0 + shape->x1;
		Int cy2 = shape->y0 + shape->y1;
		Int hw  = (shape->x1 - shape->x0);  // full width
		Int hh  = (shape->y1 - shape->y0);  // full height
		// After 90° CW: new width = old height, new height = old width
		shape->x0 = (cx2 - hh) / 2;
		shape->y0 = (cy2 - hw) / 2;
		shape->x1 = (cx2 + hh) / 2;
		shape->y1 = (cy2 + hw) / 2;
		rotatePts(shape->innerPoints, cx2, cy2);
	} else if (shape->type == SHAPE_CIRCLE) {
		// Circle is symmetric — no rotation needed
	} else {
		Int minX = INT_MAX, maxX = INT_MIN, minY = INT_MAX, maxY = INT_MIN;
		for (const auto& pt : shape->points) {
			minX = std::min(minX, pt.tx); maxX = std::max(maxX, pt.tx);
			minY = std::min(minY, pt.ty); maxY = std::max(maxY, pt.ty);
		}
		Int cx2 = minX + maxX;
		Int cy2 = minY + maxY;
		rotatePts(shape->points,      cx2, cy2);
		rotatePts(shape->innerPoints, cx2, cy2);
	}

	if (CWorldBuilderDoc* pDoc = getActiveDoc())
		pushUndo(pDoc, before);
}

// -------------------------------------------------------------------------
// Persistence — .wbsession sidecar file
// -------------------------------------------------------------------------

void ShapeFillTool::saveShapes(const CString& mapPath)
{
	CString path = shapefillPath(mapPath);
	FILE* f = fopen(path, "w");
	if (!f) return;

	fprintf(f, "SHAPEFILL_V1\n");
	fprintf(f, "NEXTID %d %d\n", m_nextId, m_nextLineId);

	for (const auto& s : m_shapes) {
		fprintf(f, "SHAPE %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			s.id, (int)s.type, s.x0, s.y0, s.x1, s.y1,
			s.cx, s.cy, s.r, s.borderWidth,
			s.innerHeight, s.autoBlendOuter ? 1 : 0,
			s.innerTexClass, s.borderTexClass);
		for (const auto& pt : s.points)
			fprintf(f, "POLY_PT %d %d %d\n", s.id, pt.tx, pt.ty);
		for (const auto& pt : s.innerPoints)
			fprintf(f, "INNER_PT %d %d %d\n", s.id, pt.tx, pt.ty);
	}

	for (const auto& l : m_lines) {
		fprintf(f, "LINE %d\n", l.id);
		for (const auto& pt : l.points)
			fprintf(f, "LINE_PT %d %d %d\n", l.id, pt.tx, pt.ty);
	}

	fclose(f);
}

void ShapeFillTool::loadShapes(const CString& mapPath)
{
	CString path = shapefillPath(mapPath);
	FILE* f = fopen(path, "r");
	if (!f) return; // no session file = fresh start

	m_shapes.clear();
	m_lines.clear();
	m_nextId     = 1;
	m_nextLineId = 1;
	m_selectedId = -1;

	char line[512];
	while (fgets(line, sizeof(line), f)) {
		if (strncmp(line, "NEXTID", 6) == 0) {
			sscanf(line, "NEXTID %d %d", &m_nextId, &m_nextLineId);
		} else if (strncmp(line, "SHAPE ", 6) == 0) {
			ShapeDef s;
			int type, autoBlend;
			sscanf(line, "SHAPE %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
				&s.id, &type, &s.x0, &s.y0, &s.x1, &s.y1,
				&s.cx, &s.cy, &s.r, &s.borderWidth,
				&s.innerHeight, &autoBlend,
				&s.innerTexClass, &s.borderTexClass);
			s.type           = (ShapeType)type;
			s.autoBlendOuter = (autoBlend != 0);
			m_shapes.push_back(s);
		} else if (strncmp(line, "POLY_PT", 7) == 0) {
			int id, tx, ty;
			sscanf(line, "POLY_PT %d %d %d", &id, &tx, &ty);
			for (auto& s : m_shapes)
				if (s.id == id) { s.points.push_back({tx, ty}); break; }
		} else if (strncmp(line, "INNER_PT", 8) == 0) {
			int id, tx, ty;
			sscanf(line, "INNER_PT %d %d %d", &id, &tx, &ty);
			for (auto& s : m_shapes)
				if (s.id == id) { s.innerPoints.push_back({tx, ty}); break; }
		} else if (strncmp(line, "LINE_PT", 7) == 0) {
			int id, tx, ty;
			sscanf(line, "LINE_PT %d %d %d", &id, &tx, &ty);
			for (auto& l : m_lines)
				if (l.id == id) { l.points.push_back({tx, ty}); break; }
		} else if (strncmp(line, "LINE ", 5) == 0) {
			LineDef l;
			sscanf(line, "LINE %d", &l.id);
			m_lines.push_back(l);
		}
	}

	fclose(f);
	ShapeFillOptions::updateFromTool();
}

// -------------------------------------------------------------------------
// Line tool
// -------------------------------------------------------------------------

void ShapeFillTool::clearLines()
{
	m_lines.clear();
	m_lineDrawing = false;
	m_lineDraft.clear();
	m_selectedLineId = -1;
	invalidateBothViews();
}

void ShapeFillTool::commitCurrentLine()
{
	if (m_lineDrawing && (Int)m_lineDraft.size() >= 2) {
		auto before = captureSnapshot();
		LineDef line;
		line.id     = m_nextLineId++;
		line.points = m_lineDraft;
		m_lines.push_back(line);
		pushUndo(getActiveDoc(), before);
	}
	m_lineDrawing = false;
	m_lineDraft.clear();
	ShapeFillOptions::updateFromTool();
	invalidateBothViews();
}
