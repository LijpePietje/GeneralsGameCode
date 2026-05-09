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

SFToolMode ShapeFillTool::m_mode          = SF_DRAW_RECT;
Int        ShapeFillTool::m_innerHeight   = 10;
Int        ShapeFillTool::m_outerHeight   = 0;
Bool       ShapeFillTool::m_autoBlend     = true;
Int        ShapeFillTool::m_borderWidth   = 5;
Int        ShapeFillTool::m_innerTexClass  = -1;
Int        ShapeFillTool::m_borderTexClass = -1;
Bool       ShapeFillTool::m_autoSave      = true;

Bool     ShapeFillTool::m_hasDraft   = false;
ShapeDef ShapeFillTool::m_draftShape;

Bool                    ShapeFillTool::m_polyDrawing = false;
std::vector<ShapeVertex> ShapeFillTool::m_polyDraft;

std::vector<LineDef>    ShapeFillTool::m_lines;
Int                     ShapeFillTool::m_nextLineId  = 1;
Bool                    ShapeFillTool::m_lineDrawing  = false;
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
	shape.outerHeight    = m_outerHeight;
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

void ShapeFillTool::applySelectedShape(CWorldBuilderDoc* pDoc)
{
	if (m_selectedId < 0) return;
	ShapeDef* shape = findShape(m_selectedId);
	if (!shape) return;

	// Sync panel settings into the shape before applying
	shape->innerHeight    = m_innerHeight;
	shape->outerHeight    = m_outerHeight;
	shape->autoBlendOuter = m_autoBlend;
	shape->borderWidth    = m_borderWidth;
	shape->innerTexClass  = m_innerTexClass;
	shape->borderTexClass = m_borderTexClass;

	WorldHeightMapEdit* htMapCopy = pDoc->GetHeightMap()->duplicate();
	Int border = htMapCopy->getBorderSize();
	Int mapW   = htMapCopy->getXExtent();
	Int mapH   = htMapCopy->getYExtent();
	TileSet tiles = rasterize(*shape);
	Bool needsOptimize = false;

	// Effective textures: border falls back to inner, inner falls back to border
	Int borderTex = (shape->borderTexClass >= 0) ? shape->borderTexClass : shape->innerTexClass;
	Int innerTex  = (shape->innerTexClass   >= 0) ? shape->innerTexClass  : borderTex;

	// Apply inner zone
	for (const CPoint& pt : tiles.inner) {
		Int hx = pt.x + border, hy = pt.y + border;
		if (hx < 0 || hy < 0 || hx >= mapW || hy >= mapH) continue;
		if (htMapCopy->getHeight(hx, hy) != (UnsignedByte)shape->innerHeight)
			htMapCopy->setHeight(hx, hy, (UnsignedByte)shape->innerHeight);
		if (innerTex >= 0)
			if (htMapCopy->setTileNdx(hx, hy, innerTex, false))
				needsOptimize = true;
	}

	// Apply border zone — distance-based gradient
	float bwf = (shape->borderWidth > 0) ? (float)shape->borderWidth : 1.0f;
	// When a free inner polygon is active, bt.dist is already normalized [0,1]
	bool hasInnerPoly = ((Int)shape->innerPoints.size() >= 3 &&
	                     (shape->type == SHAPE_POLYGON || shape->type == SHAPE_RECT || shape->type == SHAPE_CIRCLE));

	// For autoBlendOuter: sample terrain just OUTSIDE the shape boundary so
	// the reference height is stable across multiple Apply calls (idempotent).
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
		return (Int)htMapCopy->getHeight(hx, hy);
	};

	for (const BorderTile& bt : tiles.border) {
		Int hx = bt.pt.x + border, hy = bt.pt.y + border;
		if (hx < 0 || hy < 0 || hx >= mapW || hy >= mapH) continue;
		float t = hasInnerPoly ? bt.dist : std::max(0.0f, std::min(1.0f, bt.dist / bwf));
		Int existingH = htMapCopy->getHeight(hx, hy);
		Int outerH = shape->autoBlendOuter ? sampleOuterTerrain(bt.pt.x, bt.pt.y) : shape->outerHeight;
		Int h;
		if (shape->autoBlendOuter)
			h = (Int)(outerH + t * (shape->innerHeight - outerH));
		else
			h = (Int)(shape->outerHeight + t * (shape->innerHeight - shape->outerHeight));
		h = std::max(0, std::min(80, h));
		if (existingH != h)
			htMapCopy->setHeight(hx, hy, (UnsignedByte)h);
		if (borderTex >= 0)
			if (htMapCopy->setTileNdx(hx, hy, borderTex, false))
				needsOptimize = true;
	}

	// Auto-blend outer edge: engine-level texture blend at the shape boundary
	if (shape->autoBlendOuter && borderTex >= 0) {
		float blendThresh = hasInnerPoly ? 0.3f : 2.0f;
		for (const BorderTile& bt : tiles.border) {
			Int hx = bt.pt.x + border, hy = bt.pt.y + border;
			if (hx < 0 || hy < 0 || hx >= mapW || hy >= mapH) continue;
			if (bt.dist < blendThresh)
				htMapCopy->autoBlendOut(hx, hy);
		}
	}

	if (needsOptimize)
		htMapCopy->optimizeTiles();

	IRegion2D partialRange = {0, 0, 0, 0};
	pDoc->updateHeightMap(htMapCopy, false, partialRange);
	WBDocUndoable* pUndo = new WBDocUndoable(pDoc, htMapCopy);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo);
	REF_PTR_RELEASE(htMapCopy);
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
			s.outerHeight    = m_outerHeight;
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

	m_shapes.erase(
		std::remove_if(m_shapes.begin(), m_shapes.end(),
			[](const ShapeDef& s) { return s.id == m_selectedId; }),
		m_shapes.end());
	m_selectedId = -1;
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
		fprintf(f, "SHAPE %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			s.id, (int)s.type, s.x0, s.y0, s.x1, s.y1,
			s.cx, s.cy, s.r, s.borderWidth,
			s.innerHeight, s.outerHeight, s.autoBlendOuter ? 1 : 0,
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
			sscanf(line, "SHAPE %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
				&s.id, &type, &s.x0, &s.y0, &s.x1, &s.y1,
				&s.cx, &s.cy, &s.r, &s.borderWidth,
				&s.innerHeight, &s.outerHeight, &autoBlend,
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
	invalidateBothViews();
}
