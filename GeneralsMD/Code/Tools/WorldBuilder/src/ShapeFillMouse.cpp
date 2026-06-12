/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// ShapeFillMouse.cpp
// Mouse event handling, hit testing, and drag logic for ShapeFillTool.
// TheSuperHackers @feature Nemellud 09/05/2026 ShapeFillTool mouse input: hit testing, drag, polygon/line drawing

#include "StdAfx.h"
#include "ShapeFillTool.h"
#include "ShapeFillOptions.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "wbview3d.h"

#include <algorithm>
#include <cmath>

// -------------------------------------------------------------------------
// Shape lookup
// -------------------------------------------------------------------------

ShapeDef* ShapeFillTool::findShape(Int id)
{
	for (auto& s : m_shapes) { if (s.id == id) return &s; }
	return nullptr;
}

// -------------------------------------------------------------------------
// Hit testing
// -------------------------------------------------------------------------

Int ShapeFillTool::hitTestShape(Int tx, Int ty)
{
	// Test in reverse order (topmost shape first)
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
// Drag shape preview
// -------------------------------------------------------------------------

void ShapeFillTool::updateDragShape(Int tx, Int ty)
{
	m_draftShape = ShapeDef();
	m_draftShape.borderWidth    = m_borderWidth;
	m_draftShape.innerHeight    = m_innerHeight;
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
// Mouse events
// -------------------------------------------------------------------------

void ShapeFillTool::mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc* pDoc)
{
	// Right-click: finish current action and switch to select
	if (m == TRACK_R) {
		if (m_mode == SF_DRAW_POLYGON) {
			if (m_polyDrawing && m_polyDraft.size() >= 3) {
				finishPolygon();
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
		return;
	}
	if (m != TRACK_L) return;

	Int tx, ty;
	viewToTile(pView, viewPt, tx, ty);

	if (m_mode == SF_SELECT) {
		// Check line endpoints first (corner coordinates)
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
		if (hitLine) {
			m_undoSnapshotBeforeDrag = captureSnapshot();
			m_hasDragSnapshot = true;
			return;
		}

		// TheSuperHackers @feature Nemellud 13/06/2026 ShapeFill: klik overal OP de lijn (niet alleen
		// de eindpunten) om hem te selecteren — punt-tot-segment afstand.
		const Int SEG_HIT2 = 3 * 3;
		for (Int li = 0; li < (Int)m_lines.size(); li++) {
			const std::vector<ShapeVertex>& pts = m_lines[li].points;
			for (Int pi = 0; pi + 1 < (Int)pts.size(); pi++) {
				float ax = (float)pts[pi].tx,   ay = (float)pts[pi].ty;
				float bx = (float)pts[pi+1].tx, by = (float)pts[pi+1].ty;
				float abx = bx - ax, aby = by - ay;
				float denom = abx*abx + aby*aby;
				float t = denom ? ((cx - ax)*abx + (cy - ay)*aby) / denom : 0.f;
				if (t < 0.f) t = 0.f; else if (t > 1.f) t = 1.f;
				float ddx = cx - (ax + t*abx), ddy = cy - (ay + t*aby);
				if (ddx*ddx + ddy*ddy <= (float)SEG_HIT2) {
					m_selectedLineId = m_lines[li].id;
					m_selectedId     = -1;
					ShapeFillOptions::updateFromTool();
					return;
				}
			}
		}

		// Check shape handles
		ShapeHandle handle;
		if (m_selectedId >= 0 && hitTestHandle(tx, ty, handle)) {
			m_resizingHandle = true;
			m_activeHandle   = handle;
			m_moveStartTx    = tx;
			m_moveStartTy    = ty;
			m_undoSnapshotBeforeDrag = captureSnapshot();
			m_hasDragSnapshot = true;
			return;
		}

		// Check shape body for move/select
		Int hitId = hitTestShape(tx, ty);
		if (hitId >= 0) {
			m_selectedId     = hitId;
			m_selectedLineId = -1;       // shape geselecteerd → lijn-selectie wissen
			m_movingShape = true;
			m_moveStartTx = tx;
			m_moveStartTy = ty;
			m_undoSnapshotBeforeDrag = captureSnapshot();
			m_hasDragSnapshot = true;
			ShapeFillOptions::updateFromTool();
		} else {
			m_selectedId     = -1;
			m_selectedLineId = -1;       // leeg geklikt → alles deselecteren
			ShapeFillOptions::updateFromTool();
		}
		return;
	}

	if (m_mode == SF_EDIT_SHAPE) {
		// Hit-test handles first
		ShapeHandle handle;
		if (m_selectedId >= 0 && hitTestHandle(tx, ty, handle)) {
			m_resizingHandle = true;
			m_activeHandle   = handle;
			m_undoSnapshotBeforeDrag = captureSnapshot();
			m_hasDragSnapshot = true;
			return;
		}

		// Click near a polygon edge → insert vertex on that edge
		if (m_selectedId >= 0) {
			ShapeDef* shape = findShape(m_selectedId);
			if (shape) {
				auto findClosestEdge = [&](const std::vector<ShapeVertex>& poly,
				                          float& bestD, Int& bestSeg, float& bestT) {
					Int n = (Int)poly.size();
					for (Int i = 0; i < n; i++) {
						Int j = (i + 1) % n;
						float ax = (float)(poly[j].tx - poly[i].tx);
						float ay = (float)(poly[j].ty - poly[i].ty);
						float bx = (float)tx - poly[i].tx;
						float by = (float)ty - poly[i].ty;
						float len2 = ax*ax + ay*ay;
						float t = (len2 > 0) ? std::max(0.0f, std::min(1.0f, (bx*ax+by*ay)/len2)) : 0.0f;
						float ddx = bx - t*ax, ddy = by - t*ay;
						float d = sqrtf(ddx*ddx + ddy*ddy);
						if (d < bestD && t > 0.05f && t < 0.95f) { bestD = d; bestSeg = i; bestT = t; }
					}
				};

				float bestD = 2.5f; Int bestSeg = -1; float bestT = 0;

				// Check outer polygon edges first
				findClosestEdge(shape->points, bestD, bestSeg, bestT);
				if (bestSeg >= 0) {
					m_undoSnapshotBeforeDrag = captureSnapshot();
					m_hasDragSnapshot = true;
					Int j = (bestSeg + 1) % (Int)shape->points.size();
					Int newTx = (Int)roundf(shape->points[bestSeg].tx + bestT * (shape->points[j].tx - shape->points[bestSeg].tx));
					Int newTy = (Int)roundf(shape->points[bestSeg].ty + bestT * (shape->points[j].ty - shape->points[bestSeg].ty));
					shape->points.insert(shape->points.begin() + j, {newTx, newTy});
					shape->innerPoints.clear();
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
							m_undoSnapshotBeforeDrag = captureSnapshot();
							m_hasDragSnapshot = true;
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
		Int cx, cy;
		viewToCorner(pView, viewPt, cx, cy);
		if (!m_polyDrawing) {
			m_polyDrawing = true;
			m_polyDraft.clear();
		}
		// Close polygon if clicking near first corner (within 3 units) with 3+ pts
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
		ShapeFillOptions::updateFromTool();
		invalidateBothViews();
		return;
	}

	if (m_mode == SF_DRAW_LINE) {
		Int cx, cy;
		viewToCorner(pView, viewPt, cx, cy);

		// Snap to nearest endpoint of any committed line
		const Int SNAP_R2 = 5 * 5;
		bool didSnap = false;
		for (const auto& line : m_lines) {
			for (const auto& pt : line.points) {
				Int dx = cx - pt.tx, dy = cy - pt.ty;
				if (dx*dx + dy*dy <= SNAP_R2) { cx = pt.tx; cy = pt.ty; didSnap = true; break; }
			}
			if (didSnap) break;
		}
		if (!didSnap && m_lineDrawing && m_lineDraft.size() >= 2) {
			Int dx = cx - m_lineDraft[0].tx, dy = cy - m_lineDraft[0].ty;
			if (dx*dx + dy*dy <= SNAP_R2) { cx = m_lineDraft[0].tx; cy = m_lineDraft[0].ty; }
		}

		if (!m_lineDrawing) {
			m_lineDrawing = true;
			m_lineDraft.clear();
		}
		m_lineDraft.push_back({cx, cy});
		invalidateBothViews();
		return;
	}

	if (m_mode == SF_BUCKET_FILL) {
		if (m != TRACK_L) return;
		bucketFill(pDoc, tx, ty);
		return;
	}

	// Rect or circle: start drag
	m_undoSnapshotBeforeDrag = captureSnapshot();
	m_hasDragSnapshot = true;
	m_dragging      = true;
	m_dragStartView = viewPt;
	m_dragStartTx   = tx;
	m_dragStartTy   = ty;
}

void ShapeFillTool::mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc* /*pDoc*/)
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

	// Polygon mode: snap + rubber band
	if (m_mode == SF_DRAW_POLYGON) {
		Int cx, cy;
		viewToCorner(pView, viewPt, cx, cy);
		const Int SNAP_R2 = 5 * 5;
		if (m_polyDrawing && m_polyDraft.size() >= 3) {
			Int dx = cx - m_polyDraft[0].tx, dy = cy - m_polyDraft[0].ty;
			if (dx*dx + dy*dy <= SNAP_R2) { cx = m_polyDraft[0].tx; cy = m_polyDraft[0].ty; }
		}
		m_snapCx = cx; m_snapCy = cy; m_hasSnapCorner = true;
		invalidateBothViews();
		return;
	}

	m_hasSnapCorner = false;

	// Select mode: line endpoint drag (Select-only)
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

	// Select and Edit mode: handle drag + shape move
	if (m_mode == SF_SELECT || m_mode == SF_EDIT_SHAPE) {
		if (m_resizingHandle && m == TRACK_L) {
			ShapeDef* shape = findShape(m_activeHandle.shapeId);
			if (!shape) return;

			if (m_activeHandle.type == HDL_CORNER_NW)      { shape->x0 = tx; shape->y1 = ty; }
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
					shape->innerPoints.clear(); // outer vertex moved → reset inner polygon
				}
			}
			else if (m_activeHandle.type == HDL_INNER_VERTEX) {
				// Initialize innerPoints from computed inset on first drag (works for all shape types)
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
				for (auto& pt : shape->innerPoints) { pt.tx += dx; pt.ty += dy; }
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

	// Push undo for handle/move/line gestures (m_dragging is false for these)
	bool hadSnapshot = m_hasDragSnapshot;
	m_hasDragSnapshot = false;

	if (!m_dragging) {
		if (hadSnapshot && pDoc)
			pushUndo(pDoc, m_undoSnapshotBeforeDrag);
		return;
	}
	m_dragging = false;
	m_hasDraft = false;

	if (m_mode == SF_DRAW_RECT || m_mode == SF_DRAW_CIRCLE) {
		if (tx == m_dragStartTx && ty == m_dragStartTy) return; // too small

		ShapeDef shape;
		shape.id             = m_nextId++;
		shape.type           = (m_mode == SF_DRAW_RECT) ? SHAPE_RECT : SHAPE_CIRCLE;
		shape.borderWidth    = m_borderWidth;
		shape.innerHeight    = m_innerHeight;
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

		if (hadSnapshot && pDoc)
			pushUndo(pDoc, m_undoSnapshotBeforeDrag);
	}
}
