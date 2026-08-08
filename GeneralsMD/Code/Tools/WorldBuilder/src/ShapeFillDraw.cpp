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
#include <cmath>
#include <vector>

// Fractional tile coordinate, for the smoothed preview curve.
struct CPoint2F { float x, y; };

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
	cornerToViewF(pView, (float)cx, (float)cy, sx, sy);
}

// TheSuperHackers @feature Nemellud 04/08/2026 ShapeFillTool: sub-corner precision.
// The line width preview offsets points by half a width, which lands between corners.
// Rounding those to whole corners first makes a narrow preview jitter while dragging,
// so keep the fraction all the way into screen space.
void ShapeFillTool::cornerToViewF(WbView* pView, float cx, float cy, Int& sx, Int& sy)
{
	Coord3D worldPt;
	worldPt.x = cx * 10.0f;
	worldPt.y = cy * 10.0f;
	worldPt.z = 0;
	CPoint viewPt;
	pView->docToViewCoords(worldPt, &viewPt);
	viewPt.x += pView->getScrollOffsetX();
	viewPt.y += pView->getScrollOffsetY();
	sx = viewPt.x;
	sy = viewPt.y;
}

// Smooth a clicked polyline the same way the UI does before it builds the real outline:
// two Chaikin corner-cutting passes, then centripetal Catmull-Rom.
//
// Centripetal (alpha = 0.5) rather than uniform on purpose: uniform Catmull-Rom forms
// cusps and self-intersecting loops when control points are unevenly spaced, which
// hand-clicked points always are.
//
// This is a deliberate second implementation of chaikinRelax() + catmullRomSample() in
// worldbuilder-ui-poc/js/river-geom.js. It has to stay in step with that file: if the two
// drift, the preview stops showing what you will actually get. It lives here because the
// preview has to follow a drag live, and a round trip to the UI per mouse move would mean
// dozens of synchronous pipe calls a second.
static std::vector<CPoint2F> smoothCentreLine(const std::vector<ShapeVertex>& pts)
{
	std::vector<CPoint2F> p;
	p.reserve(pts.size());
	for (const auto& v : pts) p.push_back({ (float)v.tx, (float)v.ty });
	if (p.size() < 3) return p;

	// Chaikin: relaxes tight corners before splining.
	for (Int pass = 0; pass < 2; pass++) {
		std::vector<CPoint2F> next;
		next.reserve(p.size() * 2);
		next.push_back(p.front());
		for (size_t i = 0; i + 1 < p.size(); i++) {
			const CPoint2F& a = p[i];
			const CPoint2F& b = p[i + 1];
			next.push_back({ a.x * 0.75f + b.x * 0.25f, a.y * 0.75f + b.y * 0.25f });
			next.push_back({ a.x * 0.25f + b.x * 0.75f, a.y * 0.25f + b.y * 0.75f });
		}
		next.push_back(p.back());
		p.swap(next);
	}

	// Centripetal Catmull-Rom, sampled at roughly half a tile. First and last points are
	// duplicated so the curve passes through the ends the user actually clicked.
	std::vector<CPoint2F> ctrl;
	ctrl.reserve(p.size() + 2);
	ctrl.push_back(p.front());
	for (const auto& q : p) ctrl.push_back(q);
	ctrl.push_back(p.back());

	auto dist = [](const CPoint2F& a, const CPoint2F& b) {
		return sqrtf((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
	};
	std::vector<CPoint2F> out;
	for (size_t i = 1; i + 2 < ctrl.size(); i++) {
		const CPoint2F& p0 = ctrl[i - 1]; const CPoint2F& p1 = ctrl[i];
		const CPoint2F& p2 = ctrl[i + 1]; const CPoint2F& p3 = ctrl[i + 2];
		const float t0 = 0.0f;
		const float t1 = t0 + max(sqrtf(dist(p0, p1)), 1e-4f);
		const float t2 = t1 + max(sqrtf(dist(p1, p2)), 1e-4f);
		const float t3 = t2 + max(sqrtf(dist(p2, p3)), 1e-4f);

		const Int steps = max(2, (Int)ceilf(dist(p1, p2) / 0.5f));
		for (Int s = 0; s < steps; s++) {
			const float t = t1 + (t2 - t1) * ((float)s / steps);
			auto lerp = [&](const CPoint2F& a, const CPoint2F& b, float ta, float tb) {
				const float f = (tb - ta) == 0.0f ? 0.0f : (t - ta) / (tb - ta);
				return CPoint2F{ a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f };
			};
			const CPoint2F A1 = lerp(p0, p1, t0, t1);
			const CPoint2F A2 = lerp(p1, p2, t1, t2);
			const CPoint2F A3 = lerp(p2, p3, t2, t3);
			const CPoint2F B1 = lerp(A1, A2, t0, t2);
			const CPoint2F B2 = lerp(A2, A3, t1, t3);
			out.push_back(lerp(B1, B2, t1, t2));
		}
	}
	out.push_back(p.back());
	return out;
}

// TheSuperHackers @feature Nemellud 04/08/2026 ShapeFillTool: draw the footprint a line
// would have if something were built along it at `width` tiles across.
//
// The centre line is smoothed first, so what you see while drawing and dragging is the
// shape you will actually get - offsetting the raw clicked polyline would show hard
// corners where the result has curves.
//
// Offsets each point along the normal of its LOCAL direction, taken as a central
// difference between its neighbours. That is what keeps a bend the same width instead of
// pinching it, and it mirrors waterCentreLineToRing() in the UI so the preview and the
// generated shape agree on which side is which.
//
// Deliberately plain lines rather than the staircase used for real geometry: this is a
// hint, and the final outline is a smooth curve, so a staircase would misrepresent it.
void ShapeFillTool::drawWidthPreview(CDC* pDC, WbView* pView,
                                     const std::vector<ShapeVertex>& pts, Int width)
{
	if (width <= 0 || pts.size() < 2) return;

	const std::vector<CPoint2F> c = smoothCentreLine(pts);
	const Int n = (Int)c.size();
	if (n < 2) return;

	const float half = width * 0.5f;
	std::vector<CPoint> left, right;
	// Distance from each kept sample to the marked spot, so the stretch around it can be
	// redrawn in red. Kept parallel to left/right rather than indexed off c, because the
	// duplicate-point skip below means the two do not line up.
	std::vector<float> toFold;
	left.reserve(n);
	right.reserve(n);
	toFold.reserve(n);

	const Bool hasFold = (m_lineFoldX >= 0);
	for (Int i = 0; i < n; i++) {
		const CPoint2F& prev = c[i > 0 ? i - 1 : 0];
		const CPoint2F& next = c[i < n - 1 ? i + 1 : n - 1];
		float dx = next.x - prev.x;
		float dy = next.y - prev.y;
		const float len = sqrtf(dx * dx + dy * dy);
		if (len < 1e-4f) continue;          // duplicate point: no direction to offset along
		dx /= len; dy /= len;

		Int sx, sy;
		cornerToViewF(pView, c[i].x - dy * half, c[i].y + dx * half, sx, sy);
		left.push_back(CPoint(sx, sy));
		cornerToViewF(pView, c[i].x + dy * half, c[i].y - dx * half, sx, sy);
		right.push_back(CPoint(sx, sy));

		if (hasFold) {
			const float fdx = c[i].x - (float)m_lineFoldX;
			const float fdy = c[i].y - (float)m_lineFoldY;
			toFold.push_back(sqrtf(fdx * fdx + fdy * fdy));
		} else {
			toFold.push_back(1e9f);
		}
	}
	if (left.size() < 2) return;

	// PS_DASH only works on a 1px cosmetic pen, which is what reads as "preview" anyway.
	CPen pen(PS_DASH, 1, RGB(80, 170, 255));
	CPen* oldPen = pDC->SelectObject(&pen);
	pDC->Polyline(left.data(),  (int)left.size());
	pDC->Polyline(right.data(), (int)right.size());
	// Close the ends so it reads as a footprint rather than two stray lines.
	pDC->MoveTo(left.front());  pDC->LineTo(right.front());
	pDC->MoveTo(left.back());   pDC->LineTo(right.back());
	pDC->SelectObject(oldPen);

	if (!hasFold) return;

	// Overdraw the objectionable stretch in solid red. The radius scales with the width
	// because that is the scale of the problem: where a channel folds, it does so over
	// roughly its own width. A fixed radius would vanish on a wide river and swamp a creek.
	const float R = (float)width;
	CPen redPen(PS_SOLID, 2, RGB(230, 60, 60));
	pDC->SelectObject(&redPen);

	const Int m = (Int)left.size();
	for (Int i = 0; i < m; ) {
		if (toFold[i] > R) { i++; continue; }
		Int j = i;
		while (j + 1 < m && toFold[j + 1] <= R) j++;
		if (j > i) {
			pDC->Polyline(&left[i],  j - i + 1);
			pDC->Polyline(&right[i], j - i + 1);
		}
		i = j + 1;
	}

	// A ring on the spot itself, so it is findable even when zoomed out far enough that
	// the red stretch is only a few pixels long.
	Int fsx, fsy;
	cornerToViewF(pView, (float)m_lineFoldX, (float)m_lineFoldY, fsx, fsy);
	CBrush* oldBrush = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
	pDC->Ellipse(fsx - 9, fsy - 9, fsx + 9, fsy + 9);
	pDC->SelectObject(oldBrush);
	pDC->SelectObject(oldPen);
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
	// TheSuperHackers @tweak Nemellud 03/07/2026 ShapeFillTool: only draw the shapes
	// overlay while the ShapeFill tool itself is active — other tools/tabs keep a
	// clean viewport and avoid GDI-over-D3D flicker from unrelated repaints.
	if (!m_isActive) return;
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
			// TheSuperHackers @tweak Nemellud 04/08/2026 ShapeFillTool: only label a handful.
			// A coordinate beside every handle is helpful on a 4-corner rect, but a generated
			// shape - a river channel, say - carries dozens, and the labels then cover the very
			// geometry you are trying to judge.
			const std::vector<ShapeHandle> handles = getHandles(shape);
			const Bool showCoords = ((Int)handles.size() <= 12);
			for (const auto& h : handles) {
				Int sx, sy;
				tileToView(pView, h.tx, h.ty, sx, sy);
				if (h.type == HDL_INNER_VERTEX)
					drawInnerHandle(pDC, sx, sy);
				else
					drawHandle(pDC, sx, sy, false);
				if (showCoords) drawCoordLabel(pDC, sx, sy, h.tx, h.ty);
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
				// The selected line is the one you can drag, so give it grabbable squares
				// instead of 3px dots you have to hunt for.
				if (sel) drawHandle(pDC, sx, sy, false);
				else     pDC->Ellipse(sx-3, sy-3, sx+3, sy+3);
			}
			pDC->SelectObject(oldPen);
			drawWidthPreview(pDC, pView, line.points, line.previewWidth);
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
		// The draft has no LineDef yet, so its width comes from the tool default —
		// that is what makes the footprint visible while you are still clicking.
		drawWidthPreview(pDC, pView, m_lineDraft, m_linePreviewWidth);
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
