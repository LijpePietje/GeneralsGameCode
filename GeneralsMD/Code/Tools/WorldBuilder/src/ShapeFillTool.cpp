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
// Shape-based terrain fill tool for WorldBuilder.
// Allows drawing rect/circle/polygon shapes, assigning texture+height, and applying to terrain.

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
#include <queue>

// -------------------------------------------------------------------------
// Static state
// -------------------------------------------------------------------------

std::vector<ShapeDef> ShapeFillTool::m_shapes;
Int                   ShapeFillTool::m_nextId      = 1;
Int                   ShapeFillTool::m_selectedId  = -1;
ShapeDef              ShapeFillTool::m_clipboard;
Bool                  ShapeFillTool::m_hasClipboard = false;

SFToolMode ShapeFillTool::m_mode         = SF_DRAW_RECT;
Int        ShapeFillTool::m_innerHeight  = 10;
Int        ShapeFillTool::m_outerHeight  = 0;
Bool       ShapeFillTool::m_autoBlend    = true;
Int        ShapeFillTool::m_borderWidth  = 5;
Int        ShapeFillTool::m_innerTexClass = -1;
Int        ShapeFillTool::m_borderTexClass = -1;

Bool                    ShapeFillTool::m_polyDrawing = false;
std::vector<ShapeVertex> ShapeFillTool::m_polyDraft;

std::vector<LineDef>    ShapeFillTool::m_lines;
Int                     ShapeFillTool::m_nextLineId  = 1;
Bool                    ShapeFillTool::m_lineDrawing  = false;
std::vector<ShapeVertex> ShapeFillTool::m_lineDraft;

Bool ShapeFillTool::m_hasSnapCorner = false;
Int  ShapeFillTool::m_snapCx = 0;
Int  ShapeFillTool::m_snapCy = 0;
Bool ShapeFillTool::m_isActive = false;

Bool     ShapeFillTool::m_hasDraft   = false;
ShapeDef ShapeFillTool::m_draftShape;

// Forward declarations of static helpers defined later in this file
static void viewToCorner(WbView* pView, CPoint viewPt, Int& cx, Int& cy);
static void cornerToView(WbView* pView, Int cx, Int cy, Int& sx, Int& sy);
static void rasterizeSegmentToEdges(Int cx0, Int cy0, Int cx1, Int cy1,
	std::vector<bool>& hEdge, std::vector<bool>& vEdge, Int playW, Int playH);
static void drawSegmentStaircase(CDC* pDC, WbView* pView, Int cx0, Int cy0, Int cx1, Int cy1);
static void drawCircleStaircase(CDC* pDC, WbView* pView, Int cx, Int cy, Int r);
static std::vector<ShapeVertex> insetPolygon(const std::vector<ShapeVertex>& pts, Real amount);
static std::vector<ShapeVertex> getEffectiveInner(const ShapeDef& shape);
static bool                     pointInPoly(float px, float py, const std::vector<ShapeVertex>& poly);
static std::pair<Int,Int>        clampToOuterShape(const ShapeDef& shape, Int tx, Int ty);

// -------------------------------------------------------------------------
// Helper: invalidate both 2D and 3D views so the overlay redraws everywhere
// -------------------------------------------------------------------------

static void invalidateBothViews()
{
	CWorldBuilderView* p2D = CWorldBuilderDoc::GetActive2DView();
	WbView3d*          p3D = CWorldBuilderDoc::GetActive3DView();
	if (p2D) p2D->Invalidate(false);
	if (p3D) p3D->Invalidate(false);
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
	m_activePointIdx(-1)
{
}

ShapeFillTool::~ShapeFillTool()
{
}

// -------------------------------------------------------------------------
// Tool activation
// -------------------------------------------------------------------------

void ShapeFillTool::activate()
{
	m_isActive = true;
	CMainFrame::GetMainFrame()->showOptionsDialog(IDD_SHAPE_FILL_OPTIONS);
	DrawObject::setDoBrushFeedback(false);
}

void ShapeFillTool::deactivate()
{
	m_isActive       = false;
	m_dragging       = false;
	m_movingShape    = false;
	m_resizingHandle = false;
	m_movingLinePoint = false;
	m_polyDrawing    = false;
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
// Mouse events
// -------------------------------------------------------------------------

void ShapeFillTool::mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc* pDoc)
{
	// Right-click: finish current action and switch to select (except SF_SELECT = no-op)
	if (m == TRACK_R) {
		if (m_mode == SF_DRAW_POLYGON) {
			if (m_polyDrawing && m_polyDraft.size() >= 3) {
				finishPolygon(); // already calls setMode(SF_SELECT) + updateFromTool
			} else {
				m_polyDrawing = false;
				m_polyDraft.clear();
				setMode(SF_SELECT);
				ShapeFillOptions::updateFromTool();
			}
			invalidateBothViews();
		} else if (m_mode == SF_DRAW_LINE) {
			if (m_lineDrawing && m_lineDraft.size() >= 2) {
				LineDef line;
				line.id     = m_nextLineId++;
				line.points = m_lineDraft;
				m_lines.push_back(line);
			}
			m_lineDrawing = false;
			m_lineDraft.clear();
			setMode(SF_SELECT);
			ShapeFillOptions::updateFromTool();
			invalidateBothViews();
		} else if (m_mode != SF_SELECT) {
			setMode(SF_SELECT);
			ShapeFillOptions::updateFromTool();
			invalidateBothViews();
		}
		// SF_SELECT: right-click does nothing — use Delete key instead
		return;
	}
	if (m != TRACK_L) return;

	Int tx, ty;
	viewToTile(pView, viewPt, tx, ty);

	if (m_mode == SF_SELECT) {
		// Check line endpoints first (corner coords — snap to grid intersections)
		Int cx, cy;
		viewToCorner(pView, viewPt, cx, cy);
		const Int LINE_HIT2 = 4 * 4;
		bool hitLine = false;
		for (Int li = 0; li < (Int)m_lines.size() && !hitLine; li++) {
			for (Int pi = 0; pi < (Int)m_lines[li].points.size(); pi++) {
				Int dx = cx - m_lines[li].points[pi].tx;
				Int dy = cy - m_lines[li].points[pi].ty;
				if (dx*dx + dy*dy <= LINE_HIT2) {
					m_movingLinePoint = true;
					m_activeLineIdx   = li;
					m_activePointIdx  = pi;
					hitLine = true;
					break;
				}
			}
		}
		if (hitLine) return;

		// Check shape handles
		ShapeHandle handle;
		if (m_selectedId >= 0 && hitTestHandle(tx, ty, handle)) {
			m_resizingHandle = true;
			m_activeHandle   = handle;
			m_moveStartTx    = tx;
			m_moveStartTy    = ty;
			return;
		}

		// Check shape body for move/select
		Int hitId = hitTestShape(tx, ty);
		if (hitId >= 0) {
			m_selectedId   = hitId;
			m_movingShape  = true;
			m_moveStartTx  = tx;
			m_moveStartTy  = ty;
			ShapeFillOptions::updateFromTool();
		} else {
			m_selectedId = -1;
			ShapeFillOptions::updateFromTool();
		}
		return;
	}

	if (m_mode == SF_EDIT_SHAPE) {
		// Hit-test handles first — drag vertex
		ShapeHandle handle;
		if (m_selectedId >= 0 && hitTestHandle(tx, ty, handle)) {
			m_resizingHandle = true;
			m_activeHandle   = handle;
			return;
		}

		// Click near a polygon edge → insert vertex on that edge
		if (m_selectedId >= 0) {
			ShapeDef* shape = findShape(m_selectedId);
			if (shape) {
				// Helper: find closest point on a closed polygon edge
				auto findClosestEdge = [&](const std::vector<ShapeVertex>& poly,
				                           float& bestD, Int& bestSeg, float& bestT) {
					Int n2 = (Int)poly.size();
					for (Int i = 0; i < n2; i++) {
						Int j = (i + 1) % n2;
						float ax = (float)(poly[j].tx - poly[i].tx);
						float ay = (float)(poly[j].ty - poly[i].ty);
						float bx = (float)tx - poly[i].tx;
						float by = (float)ty - poly[i].ty;
						float len2 = ax*ax + ay*ay;
						float t2 = (len2 > 0) ? std::max(0.0f, std::min(1.0f, (bx*ax+by*ay)/len2)) : 0.0f;
						float ddx = bx - t2*ax, ddy = by - t2*ay;
						float d = sqrtf(ddx*ddx + ddy*ddy);
						if (d < bestD && t2 > 0.05f && t2 < 0.95f) { bestD = d; bestSeg = i; bestT = t2; }
					}
				};

				float bestD = 2.5f; Int bestSeg = -1; float bestT = 0;

				// Check outer polygon edges first
				findClosestEdge(shape->points, bestD, bestSeg, bestT);
				if (bestSeg >= 0) {
					Int j = (bestSeg + 1) % (Int)shape->points.size();
					Int newTx = (Int)roundf(shape->points[bestSeg].tx + bestT * (shape->points[j].tx - shape->points[bestSeg].tx));
					Int newTy = (Int)roundf(shape->points[bestSeg].ty + bestT * (shape->points[j].ty - shape->points[bestSeg].ty));
					shape->points.insert(shape->points.begin() + j, {newTx, newTy});
					shape->innerPoints.clear(); // outer changed → reset inner
					m_resizingHandle = true;
					m_activeHandle   = {HDL_POLY_VERTEX, shape->id, j, newTx, newTy};
					invalidateBothViews();
					return;
				}

				// Check inner polygon edges
				if (shape->borderWidth > 0) {
					auto inner = getEffectiveInner(*shape);
					if ((Int)inner.size() >= 3) {
						bestD = 2.5f; bestSeg = -1; bestT = 0;
						findClosestEdge(inner, bestD, bestSeg, bestT);
						if (bestSeg >= 0) {
							if (shape->innerPoints.empty())
								shape->innerPoints = inner;
							Int j = (bestSeg + 1) % (Int)shape->innerPoints.size();
							Int newTx = (Int)roundf(shape->innerPoints[bestSeg].tx + bestT * (shape->innerPoints[j].tx - shape->innerPoints[bestSeg].tx));
							Int newTy = (Int)roundf(shape->innerPoints[bestSeg].ty + bestT * (shape->innerPoints[j].ty - shape->innerPoints[bestSeg].ty));
							shape->innerPoints.insert(shape->innerPoints.begin() + j, {newTx, newTy});
							m_resizingHandle = true;
							m_activeHandle   = {HDL_INNER_VERTEX, shape->id, j, newTx, newTy};
							invalidateBothViews();
							return;
						}
					}
				}
			}
		}

		// Click on shape body → select it
		Int hitId = hitTestShape(tx, ty);
		if (hitId >= 0) {
			m_selectedId = hitId;
			ShapeFillOptions::updateFromTool();
		} else {
			m_selectedId = -1;
			ShapeFillOptions::updateFromTool();
		}
		invalidateBothViews();
		return;
	}

	if (m_mode == SF_DRAW_POLYGON) {
		// Snap to grid corners (same as Line tool) so barriers align exactly with visuals
		Int cx, cy;
		viewToCorner(pView, viewPt, cx, cy);
		if (!m_polyDrawing) {
			m_polyDrawing = true;
			m_polyDraft.clear();
		}
		// Close polygon if clicking near first corner (within 3 corner-units) and have 3+ pts
		if (m_polyDrawing && m_polyDraft.size() >= 3) {
			Int dx = cx - m_polyDraft[0].tx, dy = cy - m_polyDraft[0].ty;
			if (dx*dx + dy*dy <= 9) {
				finishPolygon();
				ShapeFillOptions::updateFromTool();
				invalidateBothViews();
				return;
			}
		}
		m_polyDraft.push_back({cx, cy});
		ShapeFillOptions::updateFromTool();  // refreshes Close Polygon button visibility
		invalidateBothViews();
		return;
	}

	if (m_mode == SF_DRAW_LINE) {
		// Snap click to nearest tile-corner (grid intersection)
		Int cx, cy;
		viewToCorner(pView, viewPt, cx, cy);

		// Snap to nearest endpoint of any committed line (within 5 tile-units)
		const Int SNAP_R2 = 5 * 5;
		bool didSnap = false;
		for (const auto& line : m_lines) {
			for (const auto& pt : line.points) {
				Int dx = cx - pt.tx, dy = cy - pt.ty;
				if (dx*dx + dy*dy <= SNAP_R2) { cx = pt.tx; cy = pt.ty; didSnap = true; break; }
			}
			if (didSnap) break;
		}
		// Also snap to first point of current draft (closes the loop)
		if (!didSnap && m_lineDrawing && m_lineDraft.size() >= 2) {
			Int dx = cx - m_lineDraft[0].tx, dy = cy - m_lineDraft[0].ty;
			if (dx*dx + dy*dy <= SNAP_R2) { cx = m_lineDraft[0].tx; cy = m_lineDraft[0].ty; }
		}

		if (!m_lineDrawing) {
			m_lineDrawing = true;
			m_lineDraft.clear();
		}
		m_lineDraft.push_back({cx, cy}); // stored as corner coordinates
		invalidateBothViews();
		return;
	}

	if (m_mode == SF_BUCKET_FILL) {
		if (m != TRACK_L) return;
		bucketFill(pDoc, tx, ty);
		return;
	}

	// Rect or circle: start drag
	m_dragging     = true;
	m_dragStartView = viewPt;
	m_dragStartTx  = tx;
	m_dragStartTy  = ty;
}

void ShapeFillTool::mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc* pDoc)
{
	Int tx, ty;
	viewToTile(pView, viewPt, tx, ty);

	ShapeFillOptions::updateCoordLabel(tx, ty);

	// Line mode: compute snap corner and show rubber band preview
	if (m_mode == SF_DRAW_LINE) {
		Int cx, cy;
		viewToCorner(pView, viewPt, cx, cy);
		const Int SNAP_R2 = 5 * 5;
		bool didSnap = false;
		for (const auto& line : m_lines) {
			for (const auto& pt : line.points) {
				Int dx = cx - pt.tx, dy = cy - pt.ty;
				if (dx*dx + dy*dy <= SNAP_R2) { cx = pt.tx; cy = pt.ty; didSnap = true; break; }
			}
			if (didSnap) break;
		}
		if (!didSnap && m_lineDrawing && !m_lineDraft.empty()) {
			Int dx = cx - m_lineDraft[0].tx, dy = cy - m_lineDraft[0].ty;
			if (dx*dx + dy*dy <= SNAP_R2) { cx = m_lineDraft[0].tx; cy = m_lineDraft[0].ty; }
		}
		m_snapCx = cx; m_snapCy = cy; m_hasSnapCorner = true;
		invalidateBothViews();
		return;
	}

	// Polygon mode: same snap + rubber band as line tool
	if (m_mode == SF_DRAW_POLYGON) {
		Int cx, cy;
		viewToCorner(pView, viewPt, cx, cy);
		const Int SNAP_R2 = 5 * 5;
		// Snap to first draft point to give closing visual feedback
		if (m_polyDrawing && m_polyDraft.size() >= 3) {
			Int dx = cx - m_polyDraft[0].tx, dy = cy - m_polyDraft[0].ty;
			if (dx*dx + dy*dy <= SNAP_R2) { cx = m_polyDraft[0].tx; cy = m_polyDraft[0].ty; }
		}
		m_snapCx = cx; m_snapCy = cy; m_hasSnapCorner = true;
		invalidateBothViews();
		return;
	}

	m_hasSnapCorner = false;

	if (m_mode == SF_SELECT) {
		if (m_movingLinePoint && m == TRACK_L) {
			Int cx, cy;
			viewToCorner(pView, viewPt, cx, cy);
			if (m_activeLineIdx >= 0 && m_activeLineIdx < (Int)m_lines.size()) {
				auto& pts = m_lines[m_activeLineIdx].points;
				if (m_activePointIdx >= 0 && m_activePointIdx < (Int)pts.size()) {
					pts[m_activePointIdx].tx = cx;
					pts[m_activePointIdx].ty = cy;
				}
			}
			invalidateBothViews();
			return;
		}
	}
	if (m_mode == SF_SELECT || m_mode == SF_EDIT_SHAPE) {
		if (m_resizingHandle && m == TRACK_L) {
			ShapeDef* shape = findShape(m_activeHandle.shapeId);
			if (!shape) return;

			if (m_activeHandle.type == HDL_CORNER_NW) { shape->x0 = tx; shape->y1 = ty; }
			else if (m_activeHandle.type == HDL_CORNER_NE) { shape->x1 = tx; shape->y1 = ty; }
			else if (m_activeHandle.type == HDL_CORNER_SW) { shape->x0 = tx; shape->y0 = ty; }
			else if (m_activeHandle.type == HDL_CORNER_SE) { shape->x1 = tx; shape->y0 = ty; }
			else if (m_activeHandle.type == HDL_CIRCLE_RADIUS) {
				Int dx = tx - shape->cx, dy = ty - shape->cy;
				shape->r = std::max(1, (Int)sqrt((double)(dx*dx + dy*dy)));
			}
			else if (m_activeHandle.type == HDL_POLY_VERTEX) {
				Int vi = m_activeHandle.vertexIdx;
				if (vi >= 0 && vi < (Int)shape->points.size()) {
					shape->points[vi].tx = tx;
					shape->points[vi].ty = ty;
					// Moving an outer vertex invalidates any free inner polygon
					shape->innerPoints.clear();
				}
			}
			else if (m_activeHandle.type == HDL_INNER_VERTEX) {
				// Initialize innerPoints from computed inset on first drag
				if (shape->innerPoints.empty()) {
					auto inner = getEffectiveInner(*shape);
					if ((Int)inner.size() >= 3)
						shape->innerPoints = inner;
				}
				Int vi = m_activeHandle.vertexIdx;
				if (vi >= 0 && vi < (Int)shape->innerPoints.size()) {
					auto clamped = clampToOuterShape(*shape, tx, ty);
					shape->innerPoints[vi].tx = clamped.first;
					shape->innerPoints[vi].ty = clamped.second;
				}
			}
			invalidateBothViews();
		}
		else if (m_movingShape && m == TRACK_L) {
			ShapeDef* shape = findShape(m_selectedId);
			if (!shape) return;
			Int dx = tx - m_moveStartTx, dy = ty - m_moveStartTy;
			m_moveStartTx = tx; m_moveStartTy = ty;

			if (shape->type == SHAPE_RECT) {
				shape->x0 += dx; shape->y0 += dy;
				shape->x1 += dx; shape->y1 += dy;
			} else if (shape->type == SHAPE_CIRCLE) {
				shape->cx += dx; shape->cy += dy;
			} else {
				for (auto& pt : shape->points)      { pt.tx += dx; pt.ty += dy; }
				for (auto& pt : shape->innerPoints) { pt.tx += dx; pt.ty += dy; }
			}
			invalidateBothViews();
		}
		return;
	}

	if (m_dragging && m == TRACK_L) {
		updateDragShape(tx, ty);
		invalidateBothViews();
	}
}

void ShapeFillTool::mouseUp(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc* pDoc)
{
	if (m != TRACK_L) return;

	Int tx, ty;
	viewToTile(pView, viewPt, tx, ty);

	m_movingShape     = false;
	m_resizingHandle  = false;
	m_movingLinePoint = false;

	if (!m_dragging) return;
	m_dragging  = false;
	m_hasDraft  = false;

	if (m_mode == SF_DRAW_RECT || m_mode == SF_DRAW_CIRCLE) {
		if (tx == m_dragStartTx && ty == m_dragStartTy) return; // too small

		ShapeDef shape;
		shape.id          = m_nextId++;
		shape.type        = (m_mode == SF_DRAW_RECT) ? SHAPE_RECT : SHAPE_CIRCLE;
		shape.borderWidth = m_borderWidth;
		shape.innerHeight = m_innerHeight;
		shape.outerHeight = m_outerHeight;
		shape.autoBlendOuter = m_autoBlend;
		shape.innerTexClass  = m_innerTexClass;
		shape.borderTexClass = m_borderTexClass;

		if (shape.type == SHAPE_RECT) {
			shape.x0 = std::min(m_dragStartTx, tx);
			shape.y0 = std::min(m_dragStartTy, ty);
			shape.x1 = std::max(m_dragStartTx, tx);
			shape.y1 = std::max(m_dragStartTy, ty);
		} else {
			shape.cx = (m_dragStartTx + tx) / 2;
			shape.cy = (m_dragStartTy + ty) / 2;
			Int dx = tx - shape.cx, dy = ty - shape.cy;
			shape.r = std::max(1, (Int)sqrt((double)(dx*dx + dy*dy)));
		}

		m_shapes.push_back(shape);
		m_selectedId = shape.id;
		setMode(SF_SELECT);
		ShapeFillOptions::updateFromTool();
		invalidateBothViews();
	}
}

// -------------------------------------------------------------------------
// Polygon finish
// -------------------------------------------------------------------------

void ShapeFillTool::finishPolygon()
{
	if (!m_polyDrawing || m_polyDraft.size() < 3) return;

	ShapeDef shape;
	shape.id          = m_nextId++;
	shape.type        = SHAPE_POLYGON;
	shape.points      = m_polyDraft;
	shape.borderWidth = m_borderWidth;
	shape.innerHeight = m_innerHeight;
	shape.outerHeight = m_outerHeight;
	shape.autoBlendOuter = m_autoBlend;
	shape.innerTexClass  = m_innerTexClass;
	shape.borderTexClass = m_borderTexClass;

	m_shapes.push_back(shape);
	m_selectedId  = shape.id;
	m_polyDrawing = false;
	m_polyDraft.clear();
	setMode(SF_SELECT);
	ShapeFillOptions::updateFromTool();
}

// -------------------------------------------------------------------------
// Apply to terrain
// -------------------------------------------------------------------------

void ShapeFillTool::applySelectedShape(CWorldBuilderDoc* pDoc)
{
	if (m_selectedId < 0) return;
	ShapeDef* shape = nullptr;
	for (auto& s : m_shapes) { if (s.id == m_selectedId) { shape = &s; break; } }
	if (!shape) return;

	// Always sync panel settings into the shape before applying
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

	// Apply inner zone — only call setHeight when value actually changes to avoid
	// triggering WorldBuilder's slope-constraint propagation on already-correct tiles.
	for (const CPoint& pt : tiles.inner) {
		Int hx = pt.x + border, hy = pt.y + border;
		if (hx < 0 || hy < 0 || hx >= mapW || hy >= mapH) continue;
		if (htMapCopy->getHeight(hx, hy) != (UnsignedByte)shape->innerHeight)
			htMapCopy->setHeight(hx, hy, (UnsignedByte)shape->innerHeight);
		if (innerTex >= 0)
			if (htMapCopy->setTileNdx(hx, hy, innerTex, false))
				needsOptimize = true;
	}

	// Apply border zone — distance-based gradient (like browser editor)
	float bwf = (shape->borderWidth > 0) ? (float)shape->borderWidth : 1.0f;
	// When a free inner polygon is active, bt.dist is already normalized [0,1].
	bool hasInnerPoly = ((Int)shape->innerPoints.size() >= 3 &&
	                     (shape->type == SHAPE_POLYGON || shape->type == SHAPE_RECT));
	for (const BorderTile& bt : tiles.border) {
		Int hx = bt.pt.x + border, hy = bt.pt.y + border;
		if (hx < 0 || hy < 0 || hx >= mapW || hy >= mapH) continue;
		// t = 0 at outer edge, 1 at inner zone boundary
		float t = hasInnerPoly ? bt.dist : std::max(0.0f, std::min(1.0f, bt.dist / bwf));
		Int existingH = htMapCopy->getHeight(hx, hy);
		Int h;
		if (shape->autoBlendOuter) {
			h = (Int)(existingH + t * (shape->innerHeight - existingH));
		} else {
			h = (Int)(shape->outerHeight + t * (shape->innerHeight - shape->outerHeight));
		}
		h = std::max(0, std::min(80, h));
		// Only write height if it changed — prevents propagation into tiles outside the shape
		if (existingH != h)
			htMapCopy->setHeight(hx, hy, (UnsignedByte)h);
		if (borderTex >= 0)
			if (htMapCopy->setTileNdx(hx, hy, borderTex, false))
				needsOptimize = true;
	}

	// Auto-blend outer edge: creates engine-level texture blend tiles where the shape
	// border meets surrounding terrain, softening the diagonal staircase visually.
	// Uses the same mechanism as WorldBuilder's AutoEdgeOutTool (autoBlendOut).
	// Activated by the "Auto Blend" checkbox — only the outer 2-tile ring is blended.
	if (shape->autoBlendOuter && borderTex >= 0) {
		// Threshold: absolute 2 tiles in uniform mode; normalized 0.3 in inner-polygon mode.
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
	m_shapes.erase(
		std::remove_if(m_shapes.begin(), m_shapes.end(),
			[](const ShapeDef& s) { return s.id == m_selectedId; }),
		m_shapes.end());
	m_selectedId = -1;
	ShapeFillOptions::updateFromTool();
	invalidateBothViews();
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
	ShapeDef copy      = m_clipboard;
	copy.id            = m_nextId++;
	const Int offset   = 10; // tiles

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
}

void ShapeFillTool::flipSelectedShape(Bool horizontal)
{
	ShapeDef* shape = nullptr;
	for (auto& s : m_shapes) { if (s.id == m_selectedId) { shape = &s; break; } }
	if (!shape) return;

	if (shape->type == SHAPE_RECT) {
		if (horizontal) std::swap(shape->x0, shape->x1);
		else             std::swap(shape->y0, shape->y1);
		// Flip innerPoints around the rect center axis
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
	} else if (shape->type == SHAPE_POLYGON) {
		// Find bounding box center
		Int minX = INT_MAX, maxX = INT_MIN, minY = INT_MAX, maxY = INT_MIN;
		for (const auto& pt : shape->points) {
			minX = std::min(minX, pt.tx); maxX = std::max(maxX, pt.tx);
			minY = std::min(minY, pt.ty); maxY = std::max(maxY, pt.ty);
		}
		Int cx2 = minX + maxX; // 2*cx
		Int cy2 = minY + maxY; // 2*cy
		for (auto& pt : shape->points) {
			if (horizontal) pt.tx = cx2 - pt.tx;
			else             pt.ty = cy2 - pt.ty;
		}
		for (auto& pt : shape->innerPoints) {
			if (horizontal) pt.tx = cx2 - pt.tx;
			else             pt.ty = cy2 - pt.ty;
		}
	}
}

// -------------------------------------------------------------------------
// Line tool — clear and bucket fill
// -------------------------------------------------------------------------

void ShapeFillTool::clearLines()
{
	m_lines.clear();
	m_lineDrawing = false;
	m_lineDraft.clear();
	invalidateBothViews();
}

// Rasterize a corner-to-corner line segment into tile-edge barriers.
// hEdge[y][x] = cannot cross between tile(x,y) and tile(x,y+1)
// vEdge[y][x] = cannot cross between tile(x,y) and tile(x+1,y)
// Corners use integer coordinates; corner (cx,cy) is the top-left corner of tile (cx,cy).
static void rasterizeSegmentToEdges(
	Int cx0, Int cy0, Int cx1, Int cy1,
	std::vector<bool>& hEdge, std::vector<bool>& vEdge,
	Int playW, Int playH)
{
	if (cx0 == cx1 && cy0 == cy1) return;

	auto blockH = [&](Int y, Int x) {
		// Blocks crossing between tile row y and row y+1
		if (x >= 0 && x < playW && y >= 0 && y < playH - 1)
			hEdge[y * playW + x] = true;
	};
	auto blockV = [&](Int y, Int x) {
		// Blocks crossing between tile column x and column x+1
		if (x >= 0 && x < playW - 1 && y >= 0 && y < playH)
			vEdge[y * playW + x] = true;
	};

	Int absDx = abs(cx1 - cx0), absDy = abs(cy1 - cy0);
	Int sx = (cx1 > cx0) ? 1 : -1;
	Int sy = (cy1 > cy0) ? 1 : -1;
	Int err = absDx - absDy;
	Int cx = cx0, cy = cy0;

	while (cx != cx1 || cy != cy1) {
		Int e2 = 2 * err;
		bool stepX = (absDy == 0) || (e2 > -absDy);
		bool stepY = (absDx == 0) || (e2 < absDx);

		if (stepX && stepY) {
			// Diagonal: H step first (matches drawSegmentStaircase), then V step.
			// This ensures barriers exactly correspond to the drawn staircase segments.
			blockH(cy - 1, cx + (sx > 0 ? 0 : -1));  // H segment at y=cy
			cx += sx;
			err -= absDy;
			blockV(cy + (sy > 0 ? 0 : -1), cx - 1);  // V segment at x=cx (post-H)
			cy += sy;
			err += absDx;
		} else if (stepX) {
			blockH(cy - 1, cx + (sx > 0 ? 0 : -1));
			cx += sx;
			err -= absDy;
		} else {
			blockV(cy + (sy > 0 ? 0 : -1), cx - 1);
			cy += sy;
			err += absDx;
		}
	}
}

void ShapeFillTool::bucketFill(CWorldBuilderDoc* pDoc, Int startTx, Int startTy)
{
	if (!pDoc) return;
	// Use selected texture class, or fall back to class 0 if none selected
	Int fillTexClass = (m_innerTexClass >= 0) ? m_innerTexClass : 0;

	WorldHeightMapEdit* htMapCopy = pDoc->GetHeightMap()->duplicate();
	Int border = htMapCopy->getBorderSize();
	Int mapW   = htMapCopy->getXExtent();
	Int mapH   = htMapCopy->getYExtent();
	Int playW  = mapW - 2 * border;
	Int playH  = mapH - 2 * border;

	if (startTx < 0 || startTy < 0 || startTx >= playW || startTy >= playH) {
		REF_PTR_RELEASE(htMapCopy);
		return;
	}

	// Edge-blocked connectivity (no orphaned tiles — every tile gets filled).
	// hEdge[y*playW+x]: blocks crossing between tile(x,y) and tile(x,y+1)
	// vEdge[y*playW+x]: blocks crossing between tile(x,y) and tile(x+1,y)
	std::vector<bool> hEdge(playW * playH, false);
	std::vector<bool> vEdge(playW * playH, false);

	for (const auto& line : m_lines) {
		for (Int i = 0; i + 1 < (Int)line.points.size(); i++) {
			rasterizeSegmentToEdges(
				line.points[i].tx,   line.points[i].ty,
				line.points[i+1].tx, line.points[i+1].ty,
				hEdge, vEdge, playW, playH);
		}
	}

	// Build barriers from shape outlines — outer perimeter + inner perimeter (border/inner zone
	// boundary). Both together let the fill tool click into each zone independently.
	for (const auto& shape : m_shapes) {
		if (shape.type == SHAPE_POLYGON && shape.points.size() >= 2) {
			// Outer perimeter
			for (Int i = 0; i < (Int)shape.points.size(); i++) {
				Int j = (i + 1) % (Int)shape.points.size();
				rasterizeSegmentToEdges(
					shape.points[i].tx, shape.points[i].ty,
					shape.points[j].tx, shape.points[j].ty,
					hEdge, vEdge, playW, playH);
			}
			// Inner perimeter (border-zone / inner-zone boundary)
			if (shape.borderWidth > 0 && shape.points.size() >= 3) {
				std::vector<ShapeVertex> inner = insetPolygon(shape.points, (Real)shape.borderWidth);
				if ((Int)inner.size() >= 3) {
					for (Int i = 0; i < (Int)inner.size(); i++) {
						Int j = (i + 1) % (Int)inner.size();
						rasterizeSegmentToEdges(
							inner[i].tx, inner[i].ty,
							inner[j].tx, inner[j].ty,
							hEdge, vEdge, playW, playH);
					}
				}
			}
		} else if (shape.type == SHAPE_RECT) {
			Int x0 = std::min(shape.x0, shape.x1), x1 = std::max(shape.x0, shape.x1);
			Int y0 = std::min(shape.y0, shape.y1), y1 = std::max(shape.y0, shape.y1);
			// Outer perimeter
			rasterizeSegmentToEdges(x0, y0, x1, y0, hEdge, vEdge, playW, playH);
			rasterizeSegmentToEdges(x1, y0, x1, y1, hEdge, vEdge, playW, playH);
			rasterizeSegmentToEdges(x1, y1, x0, y1, hEdge, vEdge, playW, playH);
			rasterizeSegmentToEdges(x0, y1, x0, y0, hEdge, vEdge, playW, playH);
			// Inner perimeter
			Int bw = shape.borderWidth;
			if (bw > 0 && (x1 - x0) > 2 * bw && (y1 - y0) > 2 * bw) {
				rasterizeSegmentToEdges(x0+bw, y0+bw, x1-bw, y0+bw, hEdge, vEdge, playW, playH);
				rasterizeSegmentToEdges(x1-bw, y0+bw, x1-bw, y1-bw, hEdge, vEdge, playW, playH);
				rasterizeSegmentToEdges(x1-bw, y1-bw, x0+bw, y1-bw, hEdge, vEdge, playW, playH);
				rasterizeSegmentToEdges(x0+bw, y1-bw, x0+bw, y0+bw, hEdge, vEdge, playW, playH);
			}
		} else if (shape.type == SHAPE_CIRCLE) {
			auto insideCircle = [&](Int tx, Int ty) -> bool {
				float px = (float)(tx - shape.cx), py = (float)(ty - shape.cy);
				return px*px + py*py <= (float)shape.r * shape.r;
			};
			// Outer perimeter
			for (Int ty = shape.cy - shape.r - 1; ty <= shape.cy + shape.r + 1; ty++) {
				for (Int tx = shape.cx - shape.r - 1; tx <= shape.cx + shape.r + 1; tx++) {
					if (tx < 0 || ty < 0 || tx >= playW || ty >= playH) continue;
					if (!insideCircle(tx, ty)) continue;
					if (tx + 1 < playW  && !insideCircle(tx + 1, ty)) vEdge[ty * playW + tx]       = true;
					if (tx > 0          && !insideCircle(tx - 1, ty)) vEdge[ty * playW + (tx - 1)] = true;
					if (ty + 1 < playH  && !insideCircle(tx, ty + 1)) hEdge[ty * playW + tx]       = true;
					if (ty > 0          && !insideCircle(tx, ty - 1)) hEdge[(ty - 1) * playW + tx] = true;
				}
			}
			// Inner perimeter
			Int innerR = shape.r - shape.borderWidth;
			if (innerR > 0 && shape.borderWidth > 0) {
				auto insideInner = [&](Int tx, Int ty) -> bool {
					float px = (float)(tx - shape.cx), py = (float)(ty - shape.cy);
					return px*px + py*py <= (float)innerR * innerR;
				};
				for (Int ty = shape.cy - innerR - 1; ty <= shape.cy + innerR + 1; ty++) {
					for (Int tx = shape.cx - innerR - 1; tx <= shape.cx + innerR + 1; tx++) {
						if (tx < 0 || ty < 0 || tx >= playW || ty >= playH) continue;
						if (!insideInner(tx, ty)) continue;
						if (tx + 1 < playW  && !insideInner(tx + 1, ty)) vEdge[ty * playW + tx]       = true;
						if (tx > 0          && !insideInner(tx - 1, ty)) vEdge[ty * playW + (tx - 1)] = true;
						if (ty + 1 < playH  && !insideInner(tx, ty + 1)) hEdge[ty * playW + tx]       = true;
						if (ty > 0          && !insideInner(tx, ty - 1)) hEdge[(ty - 1) * playW + tx] = true;
					}
				}
			}
		}
	}

	// BFS flood fill using edge connectivity
	std::vector<bool> visited(playW * playH, false);
	std::queue<CPoint> q;
	q.push(CPoint(startTx, startTy));
	visited[startTy * playW + startTx] = true;

	const Int dx4[] = {1, -1, 0,  0};
	const Int dy4[] = {0,  0, 1, -1};

	Bool needsOptimize = false;
	while (!q.empty()) {
		CPoint pt = q.front(); q.pop();
		Int hx = pt.x + border, hy = pt.y + border;
		if (htMapCopy->setTileNdx(hx, hy, fillTexClass, false))
			needsOptimize = true;

		for (Int d = 0; d < 4; d++) {
			Int nx = pt.x + dx4[d], ny = pt.y + dy4[d];
			if (nx < 0 || ny < 0 || nx >= playW || ny >= playH) continue;
			if (visited[ny * playW + nx]) continue;

			// Check whether the edge between current tile and neighbor is blocked
			bool blocked = false;
			if      (dx4[d] ==  1) blocked = vEdge[pt.y * playW + pt.x];
			else if (dx4[d] == -1) blocked = vEdge[pt.y * playW + (pt.x - 1)];
			else if (dy4[d] ==  1) blocked = hEdge[pt.y * playW + pt.x];
			else                   blocked = hEdge[(pt.y - 1) * playW + pt.x];

			if (blocked) continue;
			visited[ny * playW + nx] = true;
			q.push(CPoint(nx, ny));
		}
	}

	if (needsOptimize) htMapCopy->optimizeTiles();

	IRegion2D partialRange = {0, 0, 0, 0};
	pDoc->updateHeightMap(htMapCopy, false, partialRange);
	WBDocUndoable* pUndo = new WBDocUndoable(pDoc, htMapCopy);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo);
	REF_PTR_RELEASE(htMapCopy);
	invalidateBothViews();
}

// -------------------------------------------------------------------------
// Rasterizers
// -------------------------------------------------------------------------

TileSet ShapeFillTool::rasterize(const ShapeDef& shape)
{
	if (shape.type == SHAPE_RECT) {
		if (!shape.innerPoints.empty()) {
			// Rect with free inner polygon: use normalized distance rasterization
			Int minX = std::min(shape.x0, shape.x1), maxX = std::max(shape.x0, shape.x1);
			Int minY = std::min(shape.y0, shape.y1), maxY = std::max(shape.y0, shape.y1);
			std::vector<ShapeVertex> outerPts = {{minX,minY},{maxX,minY},{maxX,maxY},{minX,maxY}};
			return rasterizePolygon(outerPts, shape.borderWidth, shape.innerPoints);
		}
		return rasterizeRect(shape.x0, shape.y0, shape.x1, shape.y1, shape.borderWidth);
	} else if (shape.type == SHAPE_CIRCLE)
		return rasterizeCircle(shape.cx, shape.cy, shape.r, shape.borderWidth);
	else
		return rasterizePolygon(shape.points, shape.borderWidth, shape.innerPoints);
}

TileSet ShapeFillTool::rasterizeRect(Int x0, Int y0, Int x1, Int y1, Int border)
{
	TileSet result;
	Int minX = std::min(x0, x1), maxX = std::max(x0, x1);
	Int minY = std::min(y0, y1), maxY = std::max(y0, y1);
	float bwf = (float)border;

	for (Int x = minX; x <= maxX; x++) {
		for (Int y = minY; y <= maxY; y++) {
			// Use tile center for sub-tile accuracy (matches browser editor)
			float cx = x + 0.5f, cy = y + 0.5f;
			float dist = std::min({cx - minX, (float)maxX - cx, cy - minY, (float)maxY - cy});
			if (dist < 0) continue;
			if (dist >= bwf)
				result.inner.push_back(CPoint(x, y));
			else
				result.border.push_back({CPoint(x, y), dist});
		}
	}
	return result;
}

TileSet ShapeFillTool::rasterizeCircle(Int cx, Int cy, Int r, Int border)
{
	TileSet result;
	float rf   = (float)r;
	float bwf  = (float)border;

	for (Int x = cx - r; x <= cx + r; x++) {
		for (Int y = cy - r; y <= cy + r; y++) {
			// Both test tile and circle center use tile-center coords — offsets cancel:
			// (x+0.5) - (cx+0.5) = x - cx.  Matches drawn circle centered at tile center.
			float px = (float)(x - cx), py = (float)(y - cy);
			float dist = rf - (float)sqrt(px*px + py*py); // positive = inside
			if (dist < 0) continue;
			if (dist >= bwf)
				result.inner.push_back(CPoint(x, y));
			else
				result.border.push_back({CPoint(x, y), dist});
		}
	}
	return result;
}

Bool ShapeFillTool::pointInPolygon(Int tx, Int ty, const std::vector<ShapeVertex>& pts)
{
	Bool inside = false;
	Int n = (Int)pts.size();
	for (Int i = 0, j = n - 1; i < n; j = i++) {
		if (((pts[i].ty > ty) != (pts[j].ty > ty)) &&
		    (tx < (pts[j].tx - pts[i].tx) * (ty - pts[i].ty) / (Real)(pts[j].ty - pts[i].ty) + pts[i].tx))
			inside = !inside;
	}
	return inside;
}

// Minimum distance from point (px,py) to the boundary of a closed polygon.
static float distToPolyEdge(float px, float py, const std::vector<ShapeVertex>& poly)
{
	float minD = FLT_MAX;
	Int n = (Int)poly.size();
	for (Int i = 0, j = n - 1; i < n; j = i++) {
		float ax = (float)poly[j].tx - poly[i].tx;
		float ay = (float)poly[j].ty - poly[i].ty;
		float bx = px - poly[i].tx;
		float by = py - poly[i].ty;
		float len2 = ax*ax + ay*ay;
		float t = (len2 > 0) ? std::max(0.0f, std::min(1.0f, (bx*ax + by*ay) / len2)) : 0.0f;
		float dx = bx - t*ax, dy = by - t*ay;
		float d = sqrtf(dx*dx + dy*dy);
		if (d < minD) minD = d;
	}
	return minD;
}

// Point-in-polygon test (ray casting).
static bool pointInPoly(float px, float py, const std::vector<ShapeVertex>& poly)
{
	bool inside = false;
	Int n = (Int)poly.size();
	for (Int i = 0, j = n - 1; i < n; j = i++) {
		float xi = (float)poly[i].tx, yi = (float)poly[i].ty;
		float xj = (float)poly[j].tx, yj = (float)poly[j].ty;
		if (((yi > py) != (yj > py)) &&
		    (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
			inside = !inside;
	}
	return inside;
}

TileSet ShapeFillTool::rasterizePolygon(const std::vector<ShapeVertex>& pts, Int border,
                                        const std::vector<ShapeVertex>& innerPts)
{
	if (pts.size() < 3) return {};

	Int minX = INT_MAX, maxX = INT_MIN, minY = INT_MAX, maxY = INT_MIN;
	for (const auto& pt : pts) {
		minX = std::min(minX, pt.tx); maxX = std::max(maxX, pt.tx);
		minY = std::min(minY, pt.ty); maxY = std::max(maxY, pt.ty);
	}

	bool useInner = ((Int)innerPts.size() >= 3);
	float bwf = (float)border;
	TileSet result;

	for (Int x = minX; x <= maxX; x++) {
		for (Int y = minY; y <= maxY; y++) {
			float px = x + 0.5f, py = y + 0.5f;

			if (!pointInPoly(px, py, pts)) continue;

			if (useInner) {
				// Free inner polygon mode: border zone is between outer and inner polygon.
				// dist is normalized [0,1]: 0 = outer boundary, 1 = inner boundary.
				if (pointInPoly(px, py, innerPts)) {
					result.inner.push_back(CPoint(x, y));
				} else {
					float dOuter = distToPolyEdge(px, py, pts);
					float dInner = distToPolyEdge(px, py, innerPts);
					float sum = dOuter + dInner;
					float normDist = (sum > 0.001f) ? (dOuter / sum) : 0.0f;
					result.border.push_back({CPoint(x, y), normDist});
				}
			} else {
				// Uniform border mode: dist = absolute distance to outer edge.
				float minDist = bwf;
				for (Int i = 0, j = (Int)pts.size() - 1; i < (Int)pts.size(); j = i++) {
					float ax = (float)pts[j].tx - pts[i].tx;
					float ay = (float)pts[j].ty - pts[i].ty;
					float bx = px - pts[i].tx;
					float by = py - pts[i].ty;
					float len2 = ax*ax + ay*ay;
					float t = (len2 > 0) ? std::max(0.0f, std::min(1.0f, (bx*ax + by*ay) / len2)) : 0.0f;
					float dx = bx - t*ax, dy = by - t*ay;
					float d = sqrtf(dx*dx + dy*dy);
					if (d < minDist) minDist = d;
				}
				if (minDist >= bwf)
					result.inner.push_back(CPoint(x, y));
				else
					result.border.push_back({CPoint(x, y), minDist});
			}
		}
	}
	return result;
}

// -------------------------------------------------------------------------
// Hit testing
// -------------------------------------------------------------------------

ShapeDef* ShapeFillTool::findShape(Int id)
{
	for (auto& s : m_shapes) { if (s.id == id) return &s; }
	return nullptr;
}

Int ShapeFillTool::hitTestShape(Int tx, Int ty)
{
	// Test in reverse order (top shape first)
	for (Int i = (Int)m_shapes.size() - 1; i >= 0; i--) {
		const ShapeDef& s = m_shapes[i];
		if (s.type == SHAPE_RECT) {
			Int minX = std::min(s.x0, s.x1), maxX = std::max(s.x0, s.x1);
			Int minY = std::min(s.y0, s.y1), maxY = std::max(s.y0, s.y1);
			if (tx >= minX && tx <= maxX && ty >= minY && ty <= maxY)
				return s.id;
		} else if (s.type == SHAPE_CIRCLE) {
			Int dx = tx - s.cx, dy = ty - s.cy;
			if (sqrt((double)(dx*dx + dy*dy)) <= s.r)
				return s.id;
		} else {
			if (pointInPolygon(tx, ty, s.points))
				return s.id;
		}
	}
	return -1;
}

// Clamps (tx,ty) so that it stays inside the outer boundary of shape.
static std::pair<Int,Int> clampToOuterShape(const ShapeDef& shape, Int tx, Int ty)
{
	if (shape.type == SHAPE_RECT) {
		Int minX = std::min(shape.x0, shape.x1), maxX = std::max(shape.x0, shape.x1);
		Int minY = std::min(shape.y0, shape.y1), maxY = std::max(shape.y0, shape.y1);
		return {std::max(minX, std::min(maxX, tx)), std::max(minY, std::min(maxY, ty))};
	}
	if (shape.type == SHAPE_POLYGON && (Int)shape.points.size() >= 3) {
		if (pointInPoly((float)tx, (float)ty, shape.points))
			return {tx, ty};
		// Outside: project to nearest point on outer boundary
		float bestD2 = FLT_MAX;
		Int bestX = tx, bestY = ty;
		Int n = (Int)shape.points.size();
		for (Int i = 0; i < n; i++) {
			Int j = (i + 1) % n;
			float ax = (float)(shape.points[j].tx - shape.points[i].tx);
			float ay = (float)(shape.points[j].ty - shape.points[i].ty);
			float bx = (float)tx - shape.points[i].tx;
			float by = (float)ty - shape.points[i].ty;
			float len2 = ax*ax + ay*ay;
			float t = (len2 > 0) ? std::max(0.0f, std::min(1.0f, (bx*ax + by*ay) / len2)) : 0.0f;
			Int cx = (Int)roundf((float)shape.points[i].tx + t * ax);
			Int cy = (Int)roundf((float)shape.points[i].ty + t * ay);
			float d2 = (float)((tx - cx)*(tx - cx) + (ty - cy)*(ty - cy));
			if (d2 < bestD2) { bestD2 = d2; bestX = cx; bestY = cy; }
		}
		return {bestX, bestY};
	}
	return {tx, ty};
}

// Returns the effective inner polygon vertices for display/hit-test purposes:
// uses shape.innerPoints if set, otherwise computes from insetPolygon.
static std::vector<ShapeVertex> getEffectiveInner(const ShapeDef& shape)
{
	if (!shape.innerPoints.empty())
		return shape.innerPoints;
	if (shape.type == SHAPE_RECT && shape.borderWidth > 0) {
		Int bw   = shape.borderWidth;
		Int minX = std::min(shape.x0, shape.x1), maxX = std::max(shape.x0, shape.x1);
		Int minY = std::min(shape.y0, shape.y1), maxY = std::max(shape.y0, shape.y1);
		Int ix0 = minX + bw, iy0 = minY + bw, ix1 = maxX - bw, iy1 = maxY - bw;
		if (ix1 > ix0 && iy1 > iy0)
			return {{ix0,iy0},{ix1,iy0},{ix1,iy1},{ix0,iy1}};
	}
	if (shape.type == SHAPE_POLYGON && shape.borderWidth > 0 && (Int)shape.points.size() >= 3) {
		auto inset = insetPolygon(shape.points, (Real)shape.borderWidth);
		if ((Int)inset.size() >= 3) return inset;
	}
	return {};
}

std::vector<ShapeHandle> ShapeFillTool::getHandles(const ShapeDef& shape)
{
	std::vector<ShapeHandle> handles;
	if (shape.type == SHAPE_RECT) {
		handles.push_back({HDL_CORNER_NW, shape.id, -1, shape.x0, shape.y1});
		handles.push_back({HDL_CORNER_NE, shape.id, -1, shape.x1, shape.y1});
		handles.push_back({HDL_CORNER_SW, shape.id, -1, shape.x0, shape.y0});
		handles.push_back({HDL_CORNER_SE, shape.id, -1, shape.x1, shape.y0});
		if (m_mode == SF_EDIT_SHAPE && shape.borderWidth > 0) {
			auto inner = getEffectiveInner(shape);
			for (Int i = 0; i < (Int)inner.size(); i++)
				handles.push_back({HDL_INNER_VERTEX, shape.id, i, inner[i].tx, inner[i].ty});
		}
	} else if (shape.type == SHAPE_CIRCLE) {
		handles.push_back({HDL_CIRCLE_RADIUS, shape.id, -1, shape.cx + shape.r, shape.cy});
	} else {
		for (Int i = 0; i < (Int)shape.points.size(); i++)
			handles.push_back({HDL_POLY_VERTEX, shape.id, i, shape.points[i].tx, shape.points[i].ty});
		// Inner polygon vertex handles (only in Edit mode — prevent blocking Select-mode move)
		if (m_mode == SF_EDIT_SHAPE && shape.borderWidth > 0) {
			auto inner = getEffectiveInner(shape);
			for (Int i = 0; i < (Int)inner.size(); i++)
				handles.push_back({HDL_INNER_VERTEX, shape.id, i, inner[i].tx, inner[i].ty});
		}
	}
	return handles;
}

Bool ShapeFillTool::hitTestHandle(Int tx, Int ty, ShapeHandle& outHandle)
{
	ShapeDef* shape = findShape(m_selectedId);
	if (!shape) return false;

	const Int HIT_RADIUS = 3; // tiles
	for (const auto& h : getHandles(*shape)) {
		Int dx = tx - h.tx, dy = ty - h.ty;
		if (abs(dx) <= HIT_RADIUS && abs(dy) <= HIT_RADIUS) {
			outHandle = h;
			return true;
		}
	}
	return false;
}

// -------------------------------------------------------------------------
// Drag shape update (preview while dragging)
// -------------------------------------------------------------------------

void ShapeFillTool::updateDragShape(Int tx, Int ty)
{
	m_draftShape = ShapeDef();
	m_draftShape.borderWidth    = m_borderWidth;
	m_draftShape.innerHeight    = m_innerHeight;
	m_draftShape.outerHeight    = m_outerHeight;
	m_draftShape.autoBlendOuter = m_autoBlend;
	m_draftShape.innerTexClass  = m_innerTexClass;
	m_draftShape.borderTexClass = m_borderTexClass;

	if (m_mode == SF_DRAW_RECT) {
		m_draftShape.type = SHAPE_RECT;
		m_draftShape.x0   = std::min(m_dragStartTx, tx);
		m_draftShape.y0   = std::min(m_dragStartTy, ty);
		m_draftShape.x1   = std::max(m_dragStartTx, tx);
		m_draftShape.y1   = std::max(m_dragStartTy, ty);
	} else {
		m_draftShape.type = SHAPE_CIRCLE;
		m_draftShape.cx   = (m_dragStartTx + tx) / 2;
		m_draftShape.cy   = (m_dragStartTy + ty) / 2;
		Int dx = tx - m_draftShape.cx, dy = ty - m_draftShape.cy;
		m_draftShape.r    = std::max(1, (Int)sqrt((double)(dx*dx + dy*dy)));
	}
	m_hasDraft = true;
}

// -------------------------------------------------------------------------
// Coordinate conversion helpers
// -------------------------------------------------------------------------

void ShapeFillTool::viewToTile(WbView* pView, CPoint viewPt, Int& tx, Int& ty)
{
	Coord3D worldPt;
	pView->viewToDocCoords(viewPt, &worldPt, false);
	tx = (Int)(worldPt.x / 10.0f);
	ty = (Int)(worldPt.y / 10.0f);
}

// Snap to nearest tile-grid corner (intersection of grid lines).
// Corner (cx,cy) is at world position (cx*10, cy*10) — i.e. tile edges, not centers.
static void viewToCorner(WbView* pView, CPoint viewPt, Int& cx, Int& cy)
{
	Coord3D worldPt;
	pView->viewToDocCoords(viewPt, &worldPt, false);
	cx = (Int)(worldPt.x / 10.0f + 0.5f);
	cy = (Int)(worldPt.y / 10.0f + 0.5f);
}

// Convert tile-corner (cx,cy) to screen coordinates.
static void cornerToView(WbView* pView, Int cx, Int cy, Int& sx, Int& sy)
{
	Coord3D worldPt;
	worldPt.x = (Real)(cx * 10);
	worldPt.y = (Real)(cy * 10);
	worldPt.z = 0;
	CPoint viewPt;
	pView->docToViewCoords(worldPt, &viewPt);
	viewPt.x += pView->getScrollOffsetX();
	viewPt.y += pView->getScrollOffsetY();
	sx = viewPt.x;
	sy = viewPt.y;
}

// Draw a line segment as a staircase along tile edges.
// Each step follows the same Bresenham path used by rasterizeSegmentToEdges,
// but diagonal steps are split into an L-shape (horizontal first, then vertical)
// so the drawn path matches the actual tile-edge barriers exactly.
static void drawSegmentStaircase(CDC* pDC, WbView* pView, Int cx0, Int cy0, Int cx1, Int cy1)
{
	if (cx0 == cx1 && cy0 == cy1) return;

	Int absDx = abs(cx1 - cx0), absDy = abs(cy1 - cy0);
	Int sx = (cx1 > cx0) ? 1 : -1;
	Int sy = (cy1 > cy0) ? 1 : -1;
	Int err = absDx - absDy;
	Int cx = cx0, cy = cy0;

	Int startSx, startSy;
	cornerToView(pView, cx, cy, startSx, startSy);
	pDC->MoveTo(startSx, startSy);

	while (cx != cx1 || cy != cy1) {
		Int e2 = 2 * err;
		bool stepX = (absDy == 0) || (e2 > -absDy);
		bool stepY = (absDx == 0) || (e2 < absDx);

		if (stepX && stepY) {
			// Diagonal step: draw as L-shape (horizontal leg first, then vertical)
			cx += sx;
			err -= absDy;
			Int midSx, midSy;
			cornerToView(pView, cx, cy, midSx, midSy);
			pDC->LineTo(midSx, midSy);
			cy += sy;
			err += absDx;
		} else if (stepX) {
			cx += sx;
			err -= absDy;
		} else {
			cy += sy;
			err += absDx;
		}

		Int nextSx, nextSy;
		cornerToView(pView, cx, cy, nextSx, nextSy);
		pDC->LineTo(nextSx, nextSy);
	}
}

// Draw the tile-aligned boundary of a circle as exposed tile edges.
// Uses the same inside test as rasterizeCircle: (tx-cx)^2+(ty-cy)^2 <= r^2.
static void drawCircleStaircase(CDC* pDC, WbView* pView, Int cx, Int cy, Int r)
{
	auto inside = [=](Int tx, Int ty) -> bool {
		float px = (float)(tx - cx), py = (float)(ty - cy);
		return px*px + py*py <= (float)r * r;
	};
	for (Int ty = cy - r - 1; ty <= cy + r + 1; ty++) {
		for (Int tx = cx - r - 1; tx <= cx + r + 1; tx++) {
			if (!inside(tx, ty)) continue;
			Int sx0, sy0, sx1, sy1;
			if (!inside(tx + 1, ty)) {         // right edge
				cornerToView(pView, tx+1, ty,   sx0, sy0);
				cornerToView(pView, tx+1, ty+1, sx1, sy1);
				pDC->MoveTo(sx0, sy0); pDC->LineTo(sx1, sy1);
			}
			if (!inside(tx - 1, ty)) {         // left edge
				cornerToView(pView, tx, ty,   sx0, sy0);
				cornerToView(pView, tx, ty+1, sx1, sy1);
				pDC->MoveTo(sx0, sy0); pDC->LineTo(sx1, sy1);
			}
			if (!inside(tx, ty + 1)) {         // bottom edge
				cornerToView(pView, tx,   ty+1, sx0, sy0);
				cornerToView(pView, tx+1, ty+1, sx1, sy1);
				pDC->MoveTo(sx0, sy0); pDC->LineTo(sx1, sy1);
			}
			if (!inside(tx, ty - 1)) {         // top edge
				cornerToView(pView, tx,   ty, sx0, sy0);
				cornerToView(pView, tx+1, ty, sx1, sy1);
				pDC->MoveTo(sx0, sy0); pDC->LineTo(sx1, sy1);
			}
		}
	}
}

void ShapeFillTool::tileToView(WbView* pView, Int tx, Int ty, Int& sx, Int& sy)
{
	Coord3D worldPt;
	worldPt.x = (Real)(tx * 10);
	worldPt.y = (Real)(ty * 10);
	worldPt.z = 0;
	CPoint viewPt;
	pView->docToViewCoords(worldPt, &viewPt);
	// docToViewCoords returns screen coords; DC viewport is offset by scroll,
	// so add scroll back — same as how object icons are drawn in OnPaint.
	viewPt.x += pView->getScrollOffsetX();
	viewPt.y += pView->getScrollOffsetY();
	sx = viewPt.x;
	sy = viewPt.y;
}

// Map tile center (tx+0.5, ty+0.5) to screen — use for shapes centered on tiles.
static void tileCenterToView(WbView* pView, Int tx, Int ty, Int& sx, Int& sy)
{
	Coord3D worldPt;
	worldPt.x = (Real)(tx * 10 + 5);
	worldPt.y = (Real)(ty * 10 + 5);
	worldPt.z = 0;
	CPoint viewPt;
	pView->docToViewCoords(worldPt, &viewPt);
	viewPt.x += pView->getScrollOffsetX();
	viewPt.y += pView->getScrollOffsetY();
	sx = viewPt.x;
	sy = viewPt.y;
}

// -------------------------------------------------------------------------
// Overlay drawing
// -------------------------------------------------------------------------

static std::vector<ShapeHandle> getHandlesStatic(const ShapeDef& shape)
{
	std::vector<ShapeHandle> handles;
	if (shape.type == SHAPE_RECT) {
		handles.push_back({HDL_CORNER_NW, shape.id, -1, shape.x0, shape.y1});
		handles.push_back({HDL_CORNER_NE, shape.id, -1, shape.x1, shape.y1});
		handles.push_back({HDL_CORNER_SW, shape.id, -1, shape.x0, shape.y0});
		handles.push_back({HDL_CORNER_SE, shape.id, -1, shape.x1, shape.y0});
		if (ShapeFillTool::getMode() == SF_EDIT_SHAPE && shape.borderWidth > 0) {
			auto inner = getEffectiveInner(shape);
			for (Int i = 0; i < (Int)inner.size(); i++)
				handles.push_back({HDL_INNER_VERTEX, shape.id, i, inner[i].tx, inner[i].ty});
		}
	} else if (shape.type == SHAPE_CIRCLE) {
		handles.push_back({HDL_CIRCLE_RADIUS, shape.id, -1, shape.cx + shape.r, shape.cy});
	} else {
		for (Int i = 0; i < (Int)shape.points.size(); i++)
			handles.push_back({HDL_POLY_VERTEX, shape.id, i, shape.points[i].tx, shape.points[i].ty});
		if (ShapeFillTool::getMode() == SF_EDIT_SHAPE && shape.borderWidth > 0) {
			auto inner = getEffectiveInner(shape);
			for (Int i = 0; i < (Int)inner.size(); i++)
				handles.push_back({HDL_INNER_VERTEX, shape.id, i, inner[i].tx, inner[i].ty});
		}
	}
	return handles;
}

void ShapeFillTool::drawOverlayStatic(CDC* /*pDC_unused*/, WbView* pView)
{
	if (m_shapes.empty() && !m_hasDraft && !m_polyDrawing && m_lines.empty() && !m_lineDrawing && !m_hasSnapCorner) return;

	// Only draw in top-down projection mode — shapes are defined on a flat plane
	// and look wrong (or flip) when the camera is angled.
	WbView3d* p3D = dynamic_cast<WbView3d*>(pView);
	if (p3D && !p3D->getTopDownProjection()) return;

	// Use CClientDC instead of the CPaintDC passed in — CPaintDC clips to the
	// dirty (update) region, which means overlay drawing outside that region is
	// silently clipped away.  CClientDC covers the full client area.
	CClientDC clientDC(pView);
	CDC* pDC = &clientDC;
	clientDC.SetViewportOrg(-pView->getScrollOffsetX(), -pView->getScrollOffsetY());

	for (const auto& shape : m_shapes) {
		Bool selected = (shape.id == m_selectedId);
		drawShape(pDC, pView, shape, selected);
		if (selected) {
			for (const auto& h : getHandlesStatic(shape)) {
				Int sx, sy;
				tileToView(pView, h.tx, h.ty, sx, sy);
				if (h.type == HDL_INNER_VERTEX)
					drawInnerHandle(pDC, sx, sy);
				else
					drawHandle(pDC, sx, sy, false);
				drawCoordLabel(pDC, sx, sy, h.tx, h.ty);
			}
		}
	}

	// Draw draft shape during rect/circle drag (live preview before mouseUp)
	if (m_hasDraft) {
		drawShape(pDC, pView, m_draftShape, true);
	}

	// Draw polygon draft as staircases along tile edges (mirrors line tool style)
	if (m_polyDrawing && m_polyDraft.size() >= 1) {
		CPen pen(PS_DOT, 1, RGB(100, 200, 255));
		CPen* oldPen = pDC->SelectObject(&pen);
		pDC->SelectStockObject(NULL_BRUSH);
		for (Int i = 1; i < (Int)m_polyDraft.size(); i++) {
			drawSegmentStaircase(pDC, pView,
				m_polyDraft[i-1].tx, m_polyDraft[i-1].ty,
				m_polyDraft[i].tx,   m_polyDraft[i].ty);
		}
		Int sx, sy;
		for (const auto& pt : m_polyDraft) {
			cornerToView(pView, pt.tx, pt.ty, sx, sy);
			drawHandle(pDC, sx, sy, true);
		}
		pDC->SelectObject(oldPen);
	}

	// Draw finished lines as staircases along tile edges
	if (!m_lines.empty()) {
		CPen linePen(PS_SOLID, 2, RGB(255, 120, 30));
		CPen* oldPen = pDC->SelectObject(&linePen);
		pDC->SelectStockObject(NULL_BRUSH);
		for (const auto& line : m_lines) {
			if (line.points.size() < 2) continue;
			for (Int i = 1; i < (Int)line.points.size(); i++) {
				drawSegmentStaircase(pDC, pView,
					line.points[i-1].tx, line.points[i-1].ty,
					line.points[i].tx,   line.points[i].ty);
			}
			// Corner dots
			Int sx, sy;
			for (const auto& pt : line.points) {
				cornerToView(pView, pt.tx, pt.ty, sx, sy);
				pDC->Ellipse(sx-3, sy-3, sx+3, sy+3);
			}
		}
		pDC->SelectObject(oldPen);
	}

	// Draw current line draft as staircase (dashed — not yet committed)
	if (m_lineDrawing && m_lineDraft.size() >= 1) {
		CPen draftPen(PS_DOT, 1, RGB(255, 180, 80));
		CPen* oldPen = pDC->SelectObject(&draftPen);
		for (Int i = 1; i < (Int)m_lineDraft.size(); i++) {
			drawSegmentStaircase(pDC, pView,
				m_lineDraft[i-1].tx, m_lineDraft[i-1].ty,
				m_lineDraft[i].tx,   m_lineDraft[i].ty);
		}
		Int sx, sy;
		for (const auto& pt : m_lineDraft) {
			cornerToView(pView, pt.tx, pt.ty, sx, sy);
			drawHandle(pDC, sx, sy, true);
		}
		pDC->SelectObject(oldPen);
	}

	// Snap cursor: yellow circle at snapped corner + staircase rubber band
	if (m_hasSnapCorner) {
		Int sx, sy;
		cornerToView(pView, m_snapCx, m_snapCy, sx, sy);

		// Rubber band staircase from last placed point to snap position
		if (m_lineDrawing && !m_lineDraft.empty()) {
			CPen rubberPen(PS_DOT, 1, RGB(255, 220, 50));
			CPen* oldPen = pDC->SelectObject(&rubberPen);
			pDC->SelectStockObject(NULL_BRUSH);
			drawSegmentStaircase(pDC, pView,
				m_lineDraft.back().tx, m_lineDraft.back().ty,
				m_snapCx, m_snapCy);
			pDC->SelectObject(oldPen);
		} else if (m_polyDrawing && !m_polyDraft.empty()) {
			CPen rubberPen(PS_DOT, 1, RGB(100, 200, 255));
			CPen* oldPen = pDC->SelectObject(&rubberPen);
			pDC->SelectStockObject(NULL_BRUSH);
			drawSegmentStaircase(pDC, pView,
				m_polyDraft.back().tx, m_polyDraft.back().ty,
				m_snapCx, m_snapCy);
			pDC->SelectObject(oldPen);
		}

		// Yellow snap indicator circle
		CPen snapPen(PS_SOLID, 1, RGB(0, 0, 0));
		CBrush snapBrush(RGB(255, 255, 0));
		CPen*   oldPen   = pDC->SelectObject(&snapPen);
		CBrush* oldBrush = pDC->SelectObject(&snapBrush);
		pDC->Ellipse(sx - 4, sy - 4, sx + 4, sy + 4);
		pDC->SelectObject(oldBrush);
		pDC->SelectObject(oldPen);
	}
}

// Proper polygon inset: offset each edge inward, intersect consecutive edges.
// Matches the browser editor's insetPolygon — correct for concave polygons.
static std::vector<ShapeVertex> insetPolygon(const std::vector<ShapeVertex>& pts, Real amount)
{
	const Int n = (Int)pts.size();
	if (n < 3) return pts;

	// Signed area (shoelace) to determine winding
	Real area2 = 0;
	for (Int i = 0; i < n; i++) {
		Int j = (i + 1) % n;
		area2 += (Real)pts[i].tx * pts[j].ty - (Real)pts[j].tx * pts[i].ty;
	}
	Real sign = (area2 >= 0) ? 1.0f : -1.0f;

	// Step 1: offset each edge inward by amount
	struct OffEdge { Real ax, ay, bx, by; };
	std::vector<OffEdge> offEdges;
	offEdges.reserve(n);
	for (Int i = 0; i < n; i++) {
		Int j = (i + 1) % n;
		Real ax = (Real)pts[i].tx, ay = (Real)pts[i].ty;
		Real bx = (Real)pts[j].tx, by = (Real)pts[j].ty;
		Real dx = bx - ax, dy = by - ay;
		Real len = (Real)sqrt(dx*dx + dy*dy);
		if (len < 0.001f) continue;
		Real nx = sign * (-dy / len);
		Real ny = sign * ( dx / len);
		offEdges.push_back({ax + nx*amount, ay + ny*amount,
		                    bx + nx*amount, by + ny*amount});
	}
	if ((Int)offEdges.size() < 3) return pts;

	// Step 2: intersect consecutive offset edges → new vertices
	Int m = (Int)offEdges.size();
	std::vector<ShapeVertex> result;
	result.reserve(m);
	for (Int i = 0; i < m; i++) {
		const OffEdge& e1 = offEdges[i];
		const OffEdge& e2 = offEdges[(i + 1) % m];
		Real d1x = e1.bx - e1.ax, d1y = e1.by - e1.ay;
		Real d2x = e2.bx - e2.ax, d2y = e2.by - e2.ay;
		Real cross = d1x*d2y - d1y*d2x;
		ShapeVertex sv;
		if ((Real)fabs(cross) < 0.0001f) {
			sv.tx = (Int)((e1.bx + e2.ax) / 2);
			sv.ty = (Int)((e1.by + e2.ay) / 2);
		} else {
			Real t = ((e2.ax - e1.ax)*d2y - (e2.ay - e1.ay)*d2x) / cross;
			sv.tx = (Int)(e1.ax + t*d1x);
			sv.ty = (Int)(e1.ay + t*d1y);
		}
		result.push_back(sv);
	}
	return result;
}

void ShapeFillTool::drawShape(CDC* pDC, WbView* pView, const ShapeDef& shape, Bool selected)
{
	COLORREF color = selected ? RGB(100, 180, 255) : RGB(150, 150, 150);
	CPen pen(PS_SOLID, selected ? 2 : 1, color);
	CPen* oldPen = pDC->SelectObject(&pen);
	CBrush* oldBrush = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);

	if (shape.type == SHAPE_RECT) {
		Int sx0, sy0, sx1, sy1;
		tileToView(pView, shape.x0, shape.y0, sx0, sy0);
		tileToView(pView, shape.x1, shape.y1, sx1, sy1);
		pDC->Rectangle(std::min(sx0,sx1), std::min(sy0,sy1),
		               std::max(sx0,sx1), std::max(sy0,sy1));
	} else if (shape.type == SHAPE_CIRCLE) {
		drawCircleStaircase(pDC, pView, shape.cx, shape.cy, shape.r);
	} else if (shape.points.size() >= 2) {
		// Draw polygon outline as staircase along tile edges
		for (Int i = 1; i < (Int)shape.points.size(); i++) {
			drawSegmentStaircase(pDC, pView,
				shape.points[i-1].tx, shape.points[i-1].ty,
				shape.points[i].tx,   shape.points[i].ty);
		}
		drawSegmentStaircase(pDC, pView,
			shape.points.back().tx, shape.points.back().ty,
			shape.points[0].tx,     shape.points[0].ty);
	}

	// Draw inner (fill) zone as dashed inset — shows where inner texture goes
	if (shape.borderWidth > 0) {
		CPen innerPen(PS_DOT, 1, RGB(255, 200, 50));
		CPen* oldInner = pDC->SelectObject(&innerPen);
		pDC->SelectStockObject(NULL_BRUSH);
		Int bw = shape.borderWidth;
		if (shape.type == SHAPE_RECT) {
			if (!shape.innerPoints.empty() && (Int)shape.innerPoints.size() >= 3) {
				// Free inner polygon — draw as staircases
				for (Int i = 1; i < (Int)shape.innerPoints.size(); i++)
					drawSegmentStaircase(pDC, pView,
						shape.innerPoints[i-1].tx, shape.innerPoints[i-1].ty,
						shape.innerPoints[i].tx,   shape.innerPoints[i].ty);
				drawSegmentStaircase(pDC, pView,
					shape.innerPoints.back().tx, shape.innerPoints.back().ty,
					shape.innerPoints[0].tx,     shape.innerPoints[0].ty);
			} else {
				Int ix0, iy0, ix1, iy1;
				Int minX = std::min(shape.x0, shape.x1), maxX = std::max(shape.x0, shape.x1);
				Int minY = std::min(shape.y0, shape.y1), maxY = std::max(shape.y0, shape.y1);
				// Only draw if inner zone has positive area
				if (maxX - minX > 2 * bw && maxY - minY > 2 * bw) {
					tileToView(pView, minX + bw, minY + bw, ix0, iy0);
					tileToView(pView, maxX - bw, maxY - bw, ix1, iy1);
					// Use min/max on screen coords — tile Y and screen Y are inverted
					pDC->Rectangle(std::min(ix0,ix1), std::min(iy0,iy1),
					                std::max(ix0,ix1), std::max(iy0,iy1));
				}
			}
		} else if (shape.type == SHAPE_CIRCLE && shape.r > bw) {
			drawCircleStaircase(pDC, pView, shape.cx, shape.cy, shape.r - bw);
		} else if (shape.type == SHAPE_POLYGON && shape.points.size() >= 3) {
			// Use free inner polygon if set, otherwise fall back to computed inset
			const std::vector<ShapeVertex>* drawInner = nullptr;
			std::vector<ShapeVertex> computed;
			if (!shape.innerPoints.empty()) {
				drawInner = &shape.innerPoints;
			} else {
				computed = insetPolygon(shape.points, (Real)bw);
				if ((Int)computed.size() >= 3) drawInner = &computed;
			}
			if (drawInner && (Int)drawInner->size() >= 3) {
				for (Int i = 1; i < (Int)drawInner->size(); i++) {
					drawSegmentStaircase(pDC, pView,
						(*drawInner)[i-1].tx, (*drawInner)[i-1].ty,
						(*drawInner)[i].tx,   (*drawInner)[i].ty);
				}
				drawSegmentStaircase(pDC, pView,
					drawInner->back().tx, drawInner->back().ty,
					(*drawInner)[0].tx,   (*drawInner)[0].ty);
			}
		}
		pDC->SelectObject(oldInner);
	}

	pDC->SelectObject(oldPen);
	pDC->SelectObject(oldBrush);
}

void ShapeFillTool::drawHandle(CDC* pDC, Int sx, Int sy, Bool active)
{
	COLORREF color = active ? RGB(255, 220, 50) : RGB(100, 200, 255);
	CBrush brush(color);
	CPen pen(PS_SOLID, 1, RGB(0, 0, 0));
	CPen* oldPen     = pDC->SelectObject(&pen);
	CBrush* oldBrush = pDC->SelectObject(&brush);
	pDC->Rectangle(sx - 4, sy - 4, sx + 4, sy + 4);
	pDC->SelectObject(oldPen);
	pDC->SelectObject(oldBrush);
}

void ShapeFillTool::drawInnerHandle(CDC* pDC, Int sx, Int sy)
{
	CPen pen(PS_SOLID, 1, RGB(0, 0, 0));
	CBrush brush(RGB(255, 140, 0));
	CPen*   oldPen   = pDC->SelectObject(&pen);
	CBrush* oldBrush = pDC->SelectObject(&brush);
	POINT pts[4] = { {sx, sy-5}, {sx+5, sy}, {sx, sy+5}, {sx-5, sy} };
	pDC->Polygon(pts, 4);
	pDC->SelectObject(oldPen);
	pDC->SelectObject(oldBrush);
}

void ShapeFillTool::drawCoordLabel(CDC* pDC, Int sx, Int sy, Int tx, Int ty)
{
	CString label;
	label.Format(_T("%d,%d"), tx, ty);
	pDC->SetBkColor(RGB(0, 0, 0));
	pDC->SetTextColor(RGB(200, 230, 255));
	pDC->TextOut(sx + 6, sy - 14, label);
}
