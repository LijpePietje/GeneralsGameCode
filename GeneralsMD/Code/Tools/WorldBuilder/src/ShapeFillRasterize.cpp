/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// ShapeFillRasterize.cpp
// Rasterization, geometry helpers, and bucket fill for ShapeFillTool.
// TheSuperHackers @feature Nemellud 09/05/2026 ShapeFillTool rasterization: rect/circle/polygon rasterizers, inset polygon, bucket fill

#include "StdAfx.h"
#include "ShapeFillTool.h"
#include "CUndoable.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"

#include <algorithm>
#include <cmath>
#include <queue>

// -------------------------------------------------------------------------
// Geometry helpers
// -------------------------------------------------------------------------

float ShapeFillTool::distToPolyEdge(float px, float py, const std::vector<ShapeVertex>& poly)
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

bool ShapeFillTool::pointInPoly(float px, float py, const std::vector<ShapeVertex>& poly)
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

std::pair<Int,Int> ShapeFillTool::clampToOuterShape(const ShapeDef& shape, Int tx, Int ty)
{
	if (shape.type == SHAPE_RECT) {
		Int minX = std::min(shape.x0, shape.x1), maxX = std::max(shape.x0, shape.x1);
		Int minY = std::min(shape.y0, shape.y1), maxY = std::max(shape.y0, shape.y1);
		return {std::max(minX, std::min(maxX, tx)), std::max(minY, std::min(maxY, ty))};
	}
	if (shape.type == SHAPE_CIRCLE) {
		float dx = (float)(tx - shape.cx), dy = (float)(ty - shape.cy);
		float dist = sqrtf(dx*dx + dy*dy);
		float rf = (float)shape.r;
		if (dist <= rf) return {tx, ty};
		return {shape.cx + (Int)roundf(dx * rf / dist),
		        shape.cy + (Int)roundf(dy * rf / dist)};
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

std::vector<ShapeVertex> ShapeFillTool::getEffectiveInner(const ShapeDef& shape)
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
	if (shape.type == SHAPE_CIRCLE && shape.borderWidth > 0 && shape.r > shape.borderWidth) {
		Int innerR = shape.r - shape.borderWidth;
		const Int N = 12;
		const float kPi = 3.14159265f;
		std::vector<ShapeVertex> pts;
		for (Int i = 0; i < N; i++) {
			float angle = 2.0f * kPi * i / N;
			pts.push_back({shape.cx + (Int)roundf(innerR * cosf(angle)),
			               shape.cy + (Int)roundf(innerR * sinf(angle))});
		}
		return pts;
	}
	if (shape.type == SHAPE_POLYGON && shape.borderWidth > 0 && (Int)shape.points.size() >= 3) {
		auto inset = insetPolygon(shape.points, (Real)shape.borderWidth);
		if ((Int)inset.size() >= 3) return inset;
	}
	return {};
}

// Offset each edge inward by amount and intersect consecutive edges.
// Handles concave polygons correctly (matches browser editor's insetPolygon).
std::vector<ShapeVertex> ShapeFillTool::insetPolygon(const std::vector<ShapeVertex>& pts, Real amount)
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

// -------------------------------------------------------------------------
// Line segment → tile-edge barrier rasterization (for bucket fill)
// hEdge[y*w+x]: blocks crossing between tile(x,y) and tile(x,y+1)
// vEdge[y*w+x]: blocks crossing between tile(x,y) and tile(x+1,y)
// Diagonal steps are split H-first to match drawSegmentStaircase exactly.
// -------------------------------------------------------------------------

void ShapeFillTool::rasterizeSegmentToEdges(
	Int cx0, Int cy0, Int cx1, Int cy1,
	std::vector<bool>& hEdge, std::vector<bool>& vEdge,
	Int playW, Int playH)
{
	if (cx0 == cx1 && cy0 == cy1) return;

	auto blockH = [&](Int y, Int x) {
		if (x >= 0 && x < playW && y >= 0 && y < playH - 1)
			hEdge[y * playW + x] = true;
	};
	auto blockV = [&](Int y, Int x) {
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
			blockH(cy - 1, cx + (sx > 0 ? 0 : -1));
			cx += sx;
			err -= absDy;
			blockV(cy + (sy > 0 ? 0 : -1), cx - 1);
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

// -------------------------------------------------------------------------
// Bucket fill
// -------------------------------------------------------------------------

void ShapeFillTool::bucketFill(CWorldBuilderDoc* pDoc, Int startTx, Int startTy)
{
	if (!pDoc) return;

	AfxGetApp()->BeginWaitCursor(); // show hourglass while fill is running

	Int fillTexClass = (m_innerTexClass >= 0) ? m_innerTexClass : 0;

	WorldHeightMapEdit* pHM      = pDoc->GetHeightMap();
	WorldHeightMapEdit* htBefore = pHM->duplicate(); // pre-apply snapshot for Undo
	Int border = pHM->getBorderSize();
	Int mapW   = pHM->getXExtent();
	Int mapH   = pHM->getYExtent();
	Int playW  = mapW - 2 * border;
	Int playH  = mapH - 2 * border;

	if (startTx < 0 || startTy < 0 || startTx >= playW || startTy >= playH) {
		REF_PTR_RELEASE(htBefore);
		return;
	}

	std::vector<bool> hEdge(playW * playH, false);
	std::vector<bool> vEdge(playW * playH, false);

	// Rasterize all committed lines as barriers
	for (const auto& line : m_lines) {
		for (Int i = 0; i + 1 < (Int)line.points.size(); i++) {
			rasterizeSegmentToEdges(
				line.points[i].tx,   line.points[i].ty,
				line.points[i+1].tx, line.points[i+1].ty,
				hEdge, vEdge, playW, playH);
		}
	}

	// Rasterize shape outlines (outer + inner perimeter) as barriers
	for (const auto& shape : m_shapes) {
		if (shape.type == SHAPE_POLYGON && shape.points.size() >= 2) {
			for (Int i = 0; i < (Int)shape.points.size(); i++) {
				Int j = (i + 1) % (Int)shape.points.size();
				rasterizeSegmentToEdges(
					shape.points[i].tx, shape.points[i].ty,
					shape.points[j].tx, shape.points[j].ty,
					hEdge, vEdge, playW, playH);
			}
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
			rasterizeSegmentToEdges(x0, y0, x1, y0, hEdge, vEdge, playW, playH);
			rasterizeSegmentToEdges(x1, y0, x1, y1, hEdge, vEdge, playW, playH);
			rasterizeSegmentToEdges(x1, y1, x0, y1, hEdge, vEdge, playW, playH);
			rasterizeSegmentToEdges(x0, y1, x0, y0, hEdge, vEdge, playW, playH);
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
			for (Int ty2 = shape.cy - shape.r - 1; ty2 <= shape.cy + shape.r + 1; ty2++) {
				for (Int tx2 = shape.cx - shape.r - 1; tx2 <= shape.cx + shape.r + 1; tx2++) {
					if (tx2 < 0 || ty2 < 0 || tx2 >= playW || ty2 >= playH) continue;
					if (!insideCircle(tx2, ty2)) continue;
					if (tx2 + 1 < playW  && !insideCircle(tx2 + 1, ty2)) vEdge[ty2 * playW + tx2]        = true;
					if (tx2 > 0          && !insideCircle(tx2 - 1, ty2)) vEdge[ty2 * playW + (tx2 - 1)] = true;
					if (ty2 + 1 < playH  && !insideCircle(tx2, ty2 + 1)) hEdge[ty2 * playW + tx2]        = true;
					if (ty2 > 0          && !insideCircle(tx2, ty2 - 1)) hEdge[(ty2 - 1) * playW + tx2] = true;
				}
			}
			Int innerR = shape.r - shape.borderWidth;
			if (innerR > 0 && shape.borderWidth > 0) {
				auto insideInner = [&](Int tx, Int ty) -> bool {
					float px = (float)(tx - shape.cx), py = (float)(ty - shape.cy);
					return px*px + py*py <= (float)innerR * innerR;
				};
				for (Int ty2 = shape.cy - innerR - 1; ty2 <= shape.cy + innerR + 1; ty2++) {
					for (Int tx2 = shape.cx - innerR - 1; tx2 <= shape.cx + innerR + 1; tx2++) {
						if (tx2 < 0 || ty2 < 0 || tx2 >= playW || ty2 >= playH) continue;
						if (!insideInner(tx2, ty2)) continue;
						if (tx2 + 1 < playW  && !insideInner(tx2 + 1, ty2)) vEdge[ty2 * playW + tx2]        = true;
						if (tx2 > 0          && !insideInner(tx2 - 1, ty2)) vEdge[ty2 * playW + (tx2 - 1)] = true;
						if (ty2 + 1 < playH  && !insideInner(tx2, ty2 + 1)) hEdge[ty2 * playW + tx2]        = true;
						if (ty2 > 0          && !insideInner(tx2, ty2 - 1)) hEdge[(ty2 - 1) * playW + tx2] = true;
					}
				}
			}
		}
	}

	// BFS flood fill — build visited set only (texture applied afterward)
	std::vector<bool> visited(playW * playH, false);
	std::queue<CPoint> q;
	q.push(CPoint(startTx, startTy));
	visited[startTy * playW + startTx] = true;

	const Int dx4[] = {1, -1, 0,  0};
	const Int dy4[] = {0,  0, 1, -1};

	while (!q.empty()) {
		CPoint pt = q.front(); q.pop();
		for (Int d = 0; d < 4; d++) {
			Int nx = pt.x + dx4[d], ny = pt.y + dy4[d];
			if (nx < 0 || ny < 0 || nx >= playW || ny >= playH) continue;
			if (visited[ny * playW + nx]) continue;
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

	// Identify boundary tiles (visited tiles adjacent to non-visited tiles)
	std::vector<bool> boundary(playW * playH, false);
	if (m_fillAutoBlend && fillTexClass >= 0) {
		for (Int ty = 0; ty < playH; ty++) {
			for (Int tx = 0; tx < playW; tx++) {
				if (!visited[ty * playW + tx]) continue;
				if ((tx > 0       && !visited[ ty      * playW + (tx-1)]) ||
				    (tx < playW-1 && !visited[ ty      * playW + (tx+1)]) ||
				    (ty > 0       && !visited[(ty-1)   * playW +  tx   ]) ||
				    (ty < playH-1 && !visited[(ty+1)   * playW +  tx   ]))
					boundary[ty * playW + tx] = true;
			}
		}
	}

	// TheSuperHackers @fix Nemellud 10/05/2026 ShapeFillTool: In blend calls autoBlendOut on exterior tiles so blend starts at boundary line
	// Apply fill texture to all visited tiles; track bounding box for partial render
	Bool needsOptimize = false;
	Int rMinX = mapW, rMinY = mapH, rMaxX = 0, rMaxY = 0;
	for (Int ty = 0; ty < playH; ty++) {
		for (Int tx = 0; tx < playW; tx++) {
			if (!visited[ty * playW + tx]) continue;
			Int hx = tx + border, hy = ty + border;
			if (hx < rMinX) rMinX = hx; if (hx > rMaxX) rMaxX = hx;
			if (hy < rMinY) rMinY = hy; if (hy > rMaxY) rMaxY = hy;
			if (pHM->setTileNdx(hx, hy, fillTexClass, false))
				needsOptimize = true;
		}
	}

	if (m_fillAutoBlend && fillTexClass >= 0) {
		if (!m_fillBlendInward) {
			// Out: autoBlendOut on fill boundary tiles → fill texture bleeds out into surrounding terrain
			for (Int ty = 0; ty < playH; ty++) {
				for (Int tx = 0; tx < playW; tx++) {
					if (!boundary[ty * playW + tx]) continue;
					Int hx = tx + border, hy = ty + border;
					pHM->autoBlendOut(hx, hy);
				}
			}
		} else {
			// In: autoBlendOut on exterior tiles adjacent to fill boundary → existing terrain bleeds in from the boundary line
			for (Int ty = 0; ty < playH; ty++) {
				for (Int tx = 0; tx < playW; tx++) {
					if (visited[ty * playW + tx]) continue;
					bool adjToBoundary =
						(tx > 0       && boundary[ ty      * playW + (tx-1)]) ||
						(tx < playW-1 && boundary[ ty      * playW + (tx+1)]) ||
						(ty > 0       && boundary[(ty-1)   * playW +  tx   ]) ||
						(ty < playH-1 && boundary[(ty+1)   * playW +  tx   ]);
					if (!adjToBoundary) continue;
					Int hx = tx + border, hy = ty + border;
					pHM->autoBlendOut(hx, hy);
				}
			}
		}
		needsOptimize = true;
	}

	if (needsOptimize) pHM->optimizeTiles();

	// After optimizeTiles(), m_terrainTex is freed — must use full update.
	const Int BM = 2;
	IRegion2D shapeRange;
	if (rMaxX >= rMinX && rMaxY >= rMinY) {
		shapeRange = { std::max(0, rMinX - BM), std::max(0, rMinY - BM),
		               std::min(mapW, rMaxX + BM + 1), std::min(mapH, rMaxY + BM + 1) };
	} else {
		shapeRange = {0, 0, 0, 0};
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
// Rasterizers
// -------------------------------------------------------------------------

TileSet ShapeFillTool::rasterize(const ShapeDef& shape)
{
	// Always use at least 1 tile of border so the height cliff falls inside the shape boundary,
	// not outside it. applySelectedShape already uses bwf=1.0f when borderWidth==0.
	Int rb = std::max(shape.borderWidth, 1);

	if (shape.type == SHAPE_RECT) {
		if (!shape.innerPoints.empty()) {
			Int minX = std::min(shape.x0, shape.x1), maxX = std::max(shape.x0, shape.x1);
			Int minY = std::min(shape.y0, shape.y1), maxY = std::max(shape.y0, shape.y1);
			std::vector<ShapeVertex> outerPts = {{minX,minY},{maxX,minY},{maxX,maxY},{minX,maxY}};
			return rasterizePolygon(outerPts, rb, shape.innerPoints);
		}
		return rasterizeRect(shape.x0, shape.y0, shape.x1, shape.y1, rb);
	} else if (shape.type == SHAPE_CIRCLE) {
		if (!shape.innerPoints.empty() && (Int)shape.innerPoints.size() >= 3) {
			const Int N = 24;
			const float kPi = 3.14159265f;
			std::vector<ShapeVertex> outerPoly;
			for (Int i = 0; i < N; i++) {
				float angle = 2.0f * kPi * i / N;
				outerPoly.push_back({shape.cx + (Int)roundf(shape.r * cosf(angle)),
				                     shape.cy + (Int)roundf(shape.r * sinf(angle))});
			}
			return rasterizePolygon(outerPoly, rb, shape.innerPoints);
		}
		return rasterizeCircle(shape.cx, shape.cy, shape.r, rb);
	} else {
		return rasterizePolygon(shape.points, rb, shape.innerPoints);
	}
}

TileSet ShapeFillTool::rasterizeRect(Int x0, Int y0, Int x1, Int y1, Int border)
{
	TileSet result;
	Int minX = std::min(x0, x1), maxX = std::max(x0, x1);
	Int minY = std::min(y0, y1), maxY = std::max(y0, y1);
	float bwf = (float)border;

	for (Int x = minX; x <= maxX; x++) {
		for (Int y = minY; y <= maxY; y++) {
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
	float rf  = (float)r;
	float bwf = (float)border;

	for (Int x = cx - r; x <= cx + r; x++) {
		for (Int y = cy - r; y <= cy + r; y++) {
			float px = (float)(x - cx), py = (float)(y - cy);
			float dist = rf - (float)sqrt(px*px + py*py);
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
				// Free inner polygon: dist is normalized [0,1] (0=outer, 1=inner boundary)
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
				// Uniform border: dist = absolute distance to outer edge
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
