/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// ShapeFillDraw.cpp
// Overlay drawing, coordinate conversion, and handle rendering for ShapeFillTool.
// TheSuperHackers @feature Nemellud 09/05/2026 ShapeFillTool drawing: overlay, staircase segments, handles, coordinate conversion

#include "StdAfx.h"
#include "ShapeFillTool.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "wbview3d.h"

#include <algorithm>

// -------------------------------------------------------------------------
// Coordinate conversion
// -------------------------------------------------------------------------

void ShapeFillTool::viewToTile(WbView* pView, CPoint viewPt, Int& tx, Int& ty)
{
	Coord3D worldPt;
	pView->viewToDocCoords(viewPt, &worldPt, false);
	tx = (Int)(worldPt.x / 10.0f);
	ty = (Int)(worldPt.y / 10.0f);
}

void ShapeFillTool::tileToView(WbView* pView, Int tx, Int ty, Int& sx, Int& sy)
{
	Coord3D worldPt;
	worldPt.x = (Real)(tx * 10);
	worldPt.y = (Real)(ty * 10);
	worldPt.z = 0;
	CPoint viewPt;
	pView->docToViewCoords(worldPt, &viewPt);
	viewPt.x += pView->getScrollOffsetX();
	viewPt.y += pView->getScrollOffsetY();
	sx = viewPt.x;
	sy = viewPt.y;
}

// Snap to nearest tile-grid corner (intersection of grid lines).
// Corner (cx,cy) is at world position (cx*10, cy*10) — i.e. tile edges, not centers.
void ShapeFillTool::viewToCorner(WbView* pView, CPoint viewPt, Int& cx, Int& cy)
{
	Coord3D worldPt;
	pView->viewToDocCoords(viewPt, &worldPt, false);
	cx = (Int)(worldPt.x / 10.0f + 0.5f);
	cy = (Int)(worldPt.y / 10.0f + 0.5f);
}

void ShapeFillTool::cornerToView(WbView* pView, Int cx, Int cy, Int& sx, Int& sy)
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

void ShapeFillTool::tileCenterToView(WbView* pView, Int tx, Int ty, Int& sx, Int& sy)
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
// Staircase drawing
// -------------------------------------------------------------------------

// Draw a line segment as a staircase along tile edges, matching rasterizeSegmentToEdges.
// Diagonal steps are split into an L-shape (horizontal first, then vertical)
// so the drawn path corresponds exactly to the tile-edge barriers.
void ShapeFillTool::drawSegmentStaircase(CDC* pDC, WbView* pView, Int cx0, Int cy0, Int cx1, Int cy1)
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
			// Diagonal: draw as L-shape (horizontal leg first, then vertical)
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
void ShapeFillTool::drawCircleStaircase(CDC* pDC, WbView* pView, Int cx, Int cy, Int r)
{
	auto inside = [=](Int tx, Int ty) -> bool {
		float px = (float)(tx - cx), py = (float)(ty - cy);
		return px*px + py*py <= (float)r * r;
	};
	for (Int ty = cy - r - 1; ty <= cy + r + 1; ty++) {
		for (Int tx = cx - r - 1; tx <= cx + r + 1; tx++) {
			if (!inside(tx, ty)) continue;
			Int sx0, sy0, sx1, sy1;
			if (!inside(tx + 1, ty)) {
				cornerToView(pView, tx+1, ty,   sx0, sy0);
				cornerToView(pView, tx+1, ty+1, sx1, sy1);
				pDC->MoveTo(sx0, sy0); pDC->LineTo(sx1, sy1);
			}
			if (!inside(tx - 1, ty)) {
				cornerToView(pView, tx, ty,   sx0, sy0);
				cornerToView(pView, tx, ty+1, sx1, sy1);
				pDC->MoveTo(sx0, sy0); pDC->LineTo(sx1, sy1);
			}
			if (!inside(tx, ty + 1)) {
				cornerToView(pView, tx,   ty+1, sx0, sy0);
				cornerToView(pView, tx+1, ty+1, sx1, sy1);
				pDC->MoveTo(sx0, sy0); pDC->LineTo(sx1, sy1);
			}
			if (!inside(tx, ty - 1)) {
				cornerToView(pView, tx,   ty, sx0, sy0);
				cornerToView(pView, tx+1, ty, sx1, sy1);
				pDC->MoveTo(sx0, sy0); pDC->LineTo(sx1, sy1);
			}
		}
	}
}

// -------------------------------------------------------------------------
// Handle computation
// -------------------------------------------------------------------------

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
		if (m_mode == SF_EDIT_SHAPE && shape.borderWidth > 0) {
			auto inner = getEffectiveInner(shape);
			for (Int i = 0; i < (Int)inner.size(); i++)
				handles.push_back({HDL_INNER_VERTEX, shape.id, i, inner[i].tx, inner[i].ty});
		}
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

// -------------------------------------------------------------------------
// Shape drawing
// -------------------------------------------------------------------------

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
				for (Int i = 1; i < (Int)shape.innerPoints.size(); i++)
					drawSegmentStaircase(pDC, pView,
						shape.innerPoints[i-1].tx, shape.innerPoints[i-1].ty,
						shape.innerPoints[i].tx,   shape.innerPoints[i].ty);
				drawSegmentStaircase(pDC, pView,
					shape.innerPoints.back().tx, shape.innerPoints.back().ty,
					shape.innerPoints[0].tx,     shape.innerPoints[0].ty);
			} else {
				Int minX = std::min(shape.x0, shape.x1), maxX = std::max(shape.x0, shape.x1);
				Int minY = std::min(shape.y0, shape.y1), maxY = std::max(shape.y0, shape.y1);
				if (maxX - minX > 2 * bw && maxY - minY > 2 * bw) {
					Int ix0, iy0, ix1, iy1;
					tileToView(pView, minX + bw, minY + bw, ix0, iy0);
					tileToView(pView, maxX - bw, maxY - bw, ix1, iy1);
					pDC->Rectangle(std::min(ix0,ix1), std::min(iy0,iy1),
					               std::max(ix0,ix1), std::max(iy0,iy1));
				}
			}
		} else if (shape.type == SHAPE_CIRCLE && shape.r > bw) {
			if (!shape.innerPoints.empty() && (Int)shape.innerPoints.size() >= 3) {
				for (Int i = 1; i < (Int)shape.innerPoints.size(); i++)
					drawSegmentStaircase(pDC, pView,
						shape.innerPoints[i-1].tx, shape.innerPoints[i-1].ty,
						shape.innerPoints[i].tx,   shape.innerPoints[i].ty);
				drawSegmentStaircase(pDC, pView,
					shape.innerPoints.back().tx, shape.innerPoints.back().ty,
					shape.innerPoints[0].tx,     shape.innerPoints[0].ty);
			} else {
				drawCircleStaircase(pDC, pView, shape.cx, shape.cy, shape.r - bw);
			}
		} else if (shape.type == SHAPE_POLYGON && shape.points.size() >= 3) {
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

// -------------------------------------------------------------------------
// Main overlay entry point
// -------------------------------------------------------------------------

void ShapeFillTool::drawOverlayStatic(CDC* /*pDC_unused*/, WbView* pView)
{
	if (m_shapes.empty() && !m_hasDraft && !m_polyDrawing && m_lines.empty() && !m_lineDrawing && !m_hasSnapCorner) return;

	// Only draw in top-down projection — shapes are defined on a flat plane
	WbView3d* p3D = dynamic_cast<WbView3d*>(pView);
	if (p3D && !p3D->getTopDownProjection()) return;

	// Use CClientDC instead of the CPaintDC passed in — CPaintDC clips to the
	// dirty region, silently cutting off overlay drawing outside the update rect.
	CClientDC clientDC(pView);
	CDC* pDC = &clientDC;
	clientDC.SetViewportOrg(-pView->getScrollOffsetX(), -pView->getScrollOffsetY());

	for (const auto& shape : m_shapes) {
		Bool selected = (shape.id == m_selectedId);
		drawShape(pDC, pView, shape, selected);
		if (selected) {
			for (const auto& h : getHandles(shape)) {
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

	// Draft shape during rect/circle drag (live preview before mouseUp)
	if (m_hasDraft)
		drawShape(pDC, pView, m_draftShape, true);

	// Polygon draft as staircases (dashed — not yet committed)
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

	// Committed lines
	if (!m_lines.empty()) {
		pDC->SelectStockObject(NULL_BRUSH);
		for (const auto& line : m_lines) {
			if (line.points.size() < 2) continue;
			Bool sel = (line.id == m_selectedLineId);
			CPen pen(PS_SOLID, sel ? 3 : 2, sel ? RGB(255, 220, 0) : RGB(255, 120, 30));
			CPen* oldPen = pDC->SelectObject(&pen);
			for (Int i = 1; i < (Int)line.points.size(); i++) {
				drawSegmentStaircase(pDC, pView,
					line.points[i-1].tx, line.points[i-1].ty,
					line.points[i].tx,   line.points[i].ty);
			}
			Int sx, sy;
			for (const auto& pt : line.points) {
				cornerToView(pView, pt.tx, pt.ty, sx, sy);
				pDC->Ellipse(sx-3, sy-3, sx+3, sy+3);
			}
			pDC->SelectObject(oldPen);
		}
	}

	// Current line draft (dashed — not yet committed)
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

	// Snap cursor: yellow circle at snapped corner + rubber-band staircase
	if (m_hasSnapCorner) {
		Int sx, sy;
		cornerToView(pView, m_snapCx, m_snapCy, sx, sy);

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

		CPen snapPen(PS_SOLID, 1, RGB(0, 0, 0));
		CBrush snapBrush(RGB(255, 255, 0));
		CPen*   oldPen   = pDC->SelectObject(&snapPen);
		CBrush* oldBrush = pDC->SelectObject(&snapBrush);
		pDC->Ellipse(sx - 4, sy - 4, sx + 4, sy + 4);
		pDC->SelectObject(oldBrush);
		pDC->SelectObject(oldPen);
	}
}
