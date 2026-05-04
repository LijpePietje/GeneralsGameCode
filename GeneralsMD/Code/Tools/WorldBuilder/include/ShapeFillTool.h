/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// ShapeFillTool.h
// Shape-based terrain fill tool for WorldBuilder.
// Draw rect/circle/polygon shapes, assign texture + height, apply to terrain.

#pragma once

#include "Tool.h"
#include <vector>

class WorldHeightMapEdit;

// -------------------------------------------------------------------------
// Shape data structures
// -------------------------------------------------------------------------

enum ShapeType { SHAPE_RECT, SHAPE_CIRCLE, SHAPE_POLYGON };

struct ShapeVertex {
	Int tx, ty; // tile coordinates
};

struct ShapeDef {
	Int        id;
	ShapeType  type;
	Int        borderWidth;   // feather zone width in tiles (1-30)

	// Geometry
	Int        x0, y0, x1, y1; // rect corners (tile coords)
	Int        cx, cy, r;       // circle center + radius
	std::vector<ShapeVertex> points; // polygon vertices

	// Terrain properties
	Int  innerHeight;     // 0-80
	Int  outerHeight;     // 0-80
	Bool autoBlendOuter;  // blend outer height from existing terrain

	// Texture
	Int  innerTexClass;   // -1 = no texture
	Int  borderTexClass;  // -1 = use inner

	// Free-form inner polygon (empty = use computed insetPolygon based on borderWidth)
	std::vector<ShapeVertex> innerPoints;

	ShapeDef() :
		id(0), type(SHAPE_RECT), borderWidth(5),
		x0(0), y0(0), x1(0), y1(0),
		cx(0), cy(0), r(10),
		innerHeight(10), outerHeight(0), autoBlendOuter(true),
		innerTexClass(-1), borderTexClass(-1)
	{}
};

// -------------------------------------------------------------------------
// Line data structure (for the Line + Bucket-fill workflow)
// -------------------------------------------------------------------------

struct LineDef {
	Int id;
	std::vector<ShapeVertex> points;
	LineDef() : id(0) {}
};

// -------------------------------------------------------------------------
// Handle for interactive editing
// -------------------------------------------------------------------------

enum HandleType { HDL_CORNER_NW, HDL_CORNER_NE, HDL_CORNER_SW, HDL_CORNER_SE,
                  HDL_CIRCLE_RADIUS, HDL_POLY_VERTEX, HDL_MOVE_CENTER,
                  HDL_INNER_VERTEX };

struct ShapeHandle {
	HandleType type;
	Int        shapeId;
	Int        vertexIdx; // for POLY_VERTEX
	Int        tx, ty;    // current tile position
};

// -------------------------------------------------------------------------
// Rasterization output
// -------------------------------------------------------------------------

struct BorderTile {
	CPoint pt;
	float  dist; // tiles from outer edge: 0 at edge, borderWidth at inner boundary
};

struct TileSet {
	std::vector<CPoint>     inner;
	std::vector<BorderTile> border;
};

// -------------------------------------------------------------------------
// ShapeFillTool
// -------------------------------------------------------------------------

enum SFToolMode { SF_DRAW_RECT, SF_DRAW_CIRCLE, SF_DRAW_POLYGON, SF_SELECT, SF_DRAW_LINE, SF_BUCKET_FILL, SF_EDIT_SHAPE };

class ShapeFillTool : public Tool {
public:
	ShapeFillTool();
	virtual ~ShapeFillTool() override;

	// Tool interface
	virtual void activate() override;
	virtual void deactivate() override;
	virtual void mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc* pDoc) override;
	virtual void mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc* pDoc) override;
	virtual void mouseUp(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc* pDoc) override;
	virtual WorldHeightMapEdit* getHeightMap() override { return nullptr; }

	// Mode
	static void setMode(SFToolMode mode);
	static SFToolMode getMode() { return m_mode; }

	// Shape management
	static void applySelectedShape(CWorldBuilderDoc* pDoc);
	static void deleteSelectedShape();
	static void copySelectedShape();
	static void pasteShape();
	static void flipSelectedShape(Bool horizontal);
	static void finishPolygon();

	// Properties (set from options dialog)
	static void setInnerHeight(Int h)    { m_innerHeight = h; }
	static void setOuterHeight(Int h)    { m_outerHeight = h; }
	static void setAutoBlend(Bool b)     { m_autoBlend = b; }
	static void setBorderWidth(Int w)    {
		m_borderWidth = w;
		// Reset any free inner polygon so the uniform inset is recomputed.
		if (m_selectedId >= 0) {
			for (auto& s : m_shapes) {
				if (s.id == m_selectedId && s.type == SHAPE_POLYGON) {
					s.innerPoints.clear();
					break;
				}
			}
		}
	}
	static void setInnerTexClass(Int t)  { m_innerTexClass = t; }
	static void setBorderTexClass(Int t) { m_borderTexClass = t; }

	static Int  getInnerHeight()   { return m_innerHeight; }
	static Int  getOuterHeight()   { return m_outerHeight; }
	static Bool getAutoBlend()     { return m_autoBlend; }
	static Int  getBorderWidth()   { return m_borderWidth; }
	static Int  getInnerTexClass() { return m_innerTexClass; }
	static Int  getBorderTexClass(){ return m_borderTexClass; }

	static Int  getSelectedId()    { return m_selectedId; }
	static void setSelectedId(Int id) { m_selectedId = id; }
	static void syncSelectedFromPanel();
	static Bool isPolyDrawing()    { return m_polyDrawing; }
	static Int  getPolyDraftSize() { return (Int)m_polyDraft.size(); }
	static const std::vector<ShapeDef>& getShapes() { return m_shapes; }
	static Bool isActive()         { return m_isActive; }

	// Line tool
	static void clearLines();
	static void bucketFill(CWorldBuilderDoc* pDoc, Int tx, Int ty);
	static const std::vector<LineDef>& getLines() { return m_lines; }

	// Drawing overlay (called from WorldBuilderView via Tool base class virtual)
	void drawOverlay(CDC* pDC, WbView* pView) override { drawOverlayStatic(pDC, pView); }
	static void drawOverlayStatic(CDC* pDC, WbView* pView);

private:
	// Shape list & selection
	static std::vector<ShapeDef> m_shapes;
	static Int                   m_nextId;
	static Int                   m_selectedId;
	static ShapeDef              m_clipboard;
	static Bool                  m_hasClipboard;
	static Bool                  m_isActive;

	// Tool properties
	static SFToolMode m_mode;
	static Int        m_innerHeight;
	static Int        m_outerHeight;
	static Bool       m_autoBlend;
	static Int        m_borderWidth;
	static Int        m_innerTexClass;
	static Int        m_borderTexClass;

	// Drag state — drawing new shape
	Bool   m_dragging;
	CPoint m_dragStartView;
	Int    m_dragStartTx, m_dragStartTy;

	// Draft shape shown during drag before mouseUp
	static Bool    m_hasDraft;
	static ShapeDef m_draftShape;

	// Polygon drawing state
	static Bool                   m_polyDrawing;
	static std::vector<ShapeVertex> m_polyDraft;

	// Line drawing state
	static std::vector<LineDef>   m_lines;
	static Int                    m_nextLineId;
	static Bool                   m_lineDrawing;
	static std::vector<ShapeVertex> m_lineDraft;

	// Snap cursor (live preview while hovering in line mode)
	static Bool m_hasSnapCorner;
	static Int  m_snapCx, m_snapCy;

	// Saved camera state — restored when tool deactivates
	Bool        m_prevTopDown;

	// Drag state — editing shape handles
	Bool        m_movingShape;
	Bool        m_resizingHandle;
	ShapeHandle m_activeHandle;
	Int         m_moveStartTx, m_moveStartTy;

	// Drag state — editing line endpoints
	Bool        m_movingLinePoint;
	Int         m_activeLineIdx;
	Int         m_activePointIdx;

	// Internal helpers
	ShapeDef*         findShape(Int id);
	Int               hitTestShape(Int tx, Int ty);
	Bool              hitTestHandle(Int tx, Int ty, ShapeHandle& outHandle);
	std::vector<ShapeHandle> getHandles(const ShapeDef& shape);

	static TileSet    rasterize(const ShapeDef& shape);
	static TileSet    rasterizeRect(Int x0, Int y0, Int x1, Int y1, Int border);
	static TileSet    rasterizeCircle(Int cx, Int cy, Int r, Int border);
	static TileSet    rasterizePolygon(const std::vector<ShapeVertex>& pts, Int border,
	                                   const std::vector<ShapeVertex>& innerPts = {});
	static Bool       pointInPolygon(Int tx, Int ty, const std::vector<ShapeVertex>& pts);

	void              updateDragShape(Int tx, Int ty);
	static void       drawShape(CDC* pDC, WbView* pView, const ShapeDef& shape, Bool selected);
	static void       drawHandle(CDC* pDC, Int sx, Int sy, Bool active);
	static void       drawInnerHandle(CDC* pDC, Int sx, Int sy);
	static void       drawCoordLabel(CDC* pDC, Int sx, Int sy, Int tx, Int ty);
	static void       viewToTile(WbView* pView, CPoint viewPt, Int& tx, Int& ty);
	static void       tileToView(WbView* pView, Int tx, Int ty, Int& sx, Int& sy);
};
