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
// Terrain & Texture Painter — shape-based terrain painting tool for WorldBuilder.
// Draw rect/circle/polygon shapes, assign texture + height, apply to terrain.
// TheSuperHackers @feature Nemellud 09/05/2026 Adds Terrain & Texture Painter: shape-based height and texture painting tool for WorldBuilder

#pragma once

#include "CUndoable.h"
#include "Tool.h"
#include <vector>

class WorldHeightMapEdit;

// -------------------------------------------------------------------------
// Shape data structures
// -------------------------------------------------------------------------

enum ShapeType { SHAPE_RECT, SHAPE_CIRCLE, SHAPE_POLYGON };

struct ShapeVertex {
	Int tx, ty;
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
	Bool autoBlendOuter;  // outer texture blend active (Out or In)

	// Texture
	Int  innerTexClass;   // -1 = no texture
	Int  borderTexClass;  // -1 = use inner

	// Free-form inner polygon (empty = use computed inset based on borderWidth)
	std::vector<ShapeVertex> innerPoints;

	ShapeDef() :
		id(0), type(SHAPE_RECT), borderWidth(5),
		x0(0), y0(0), x1(0), y1(0),
		cx(0), cy(0), r(10),
		innerHeight(10), autoBlendOuter(true),
		innerTexClass(-1), borderTexClass(-1)
	{}
};

// -------------------------------------------------------------------------
// Line data structure (for the Line + Bucket-fill workflow)
// -------------------------------------------------------------------------

struct LineDef {
	Int id;
	std::vector<ShapeVertex> points;
	// TheSuperHackers @feature Nemellud 04/08/2026 ShapeFillTool: draw a width alongside a line.
	// 0 = plain line. Non-zero makes the overlay show two bank lines at +/- half this width,
	// so a line can preview the footprint of something that will be built along it - a river
	// channel, say - while it is being drawn and dragged. Purely an overlay hint: it changes
	// no terrain and creates no shape. In tiles, like every other ShapeFill coordinate.
	Int previewWidth;
	LineDef() : id(0), previewWidth(0) {}
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
	Int        vertexIdx;
	Int        tx, ty;
};

// -------------------------------------------------------------------------
// Rasterization output
// -------------------------------------------------------------------------

struct BorderTile {
	CPoint pt;
	float  dist; // 0 = outer edge, borderWidth (or 1.0 normalized) = inner boundary
};

struct TileSet {
	std::vector<CPoint>     inner;
	std::vector<BorderTile> border;
};

// -------------------------------------------------------------------------
// Undo/redo snapshot
// -------------------------------------------------------------------------

struct ShapeFillSnapshot {
	std::vector<ShapeDef> shapes;
	std::vector<LineDef>  lines;
	Int selectedId;
	Int nextId;
	Int nextLineId;
};

class ShapeFillUndoable : public Undoable {
	ShapeFillSnapshot m_before;
	ShapeFillSnapshot m_after;
public:
	ShapeFillUndoable(ShapeFillSnapshot before, ShapeFillSnapshot after)
		: m_before(std::move(before)), m_after(std::move(after)) {}
	virtual ~ShapeFillUndoable() override {}
	virtual void Do() override;
	virtual void Undo() override;
};

// Custom undoable for in-place heightmap apply — enables partial render update.
class ShapeFillApplyUndoable : public Undoable {
	CWorldBuilderDoc*   m_pDoc;
	WorldHeightMapEdit* m_pBefore; // pre-apply snapshot (for Undo)
	WorldHeightMapEdit* m_pAfter;  // post-apply = the live HM (for Redo)
	IRegion2D           m_range;
public:
	ShapeFillApplyUndoable(CWorldBuilderDoc* pDoc, WorldHeightMapEdit* before,
	                        WorldHeightMapEdit* after, IRegion2D range);
	virtual ~ShapeFillApplyUndoable() override;
	virtual void Do() override;
	virtual void Undo() override;
	virtual void Redo() override;
};

// -------------------------------------------------------------------------
// ShapeFillTool
// -------------------------------------------------------------------------

enum SFToolMode { SF_DRAW_RECT, SF_DRAW_CIRCLE, SF_DRAW_POLYGON, SF_SELECT,
                  SF_DRAW_LINE, SF_BUCKET_FILL, SF_EDIT_SHAPE };

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
	static void      setMode(SFToolMode mode);
	static SFToolMode getMode() { return m_mode; }

	// Shape management
	// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: programmatic shape creation for pipe
	static Int  addShape(ShapeDef def);
	static void applySelectedShape(CWorldBuilderDoc* pDoc);
	static void deleteSelectedShape();
	static void copySelectedShape();
	static void pasteShape();
	static void flipSelectedShape(Bool horizontal);
	static void rotateSelectedShape();
	static void finishPolygon();

	// Properties (set from options dialog)
	static void setInnerHeight(Int h)    { m_innerHeight = h; }
	static void setAutoBlend(Bool b)     { m_autoBlend = b; }
	static void setBlendInward(Bool b)   { m_blendInward = b; }
	static void setBorderWidth(Int w)    {
		m_borderWidth = w;
		if (m_selectedId >= 0) {
			for (auto& s : m_shapes) {
				if (s.id == m_selectedId && (s.type == SHAPE_POLYGON || s.type == SHAPE_RECT)) {
					s.innerPoints.clear();
					break;
				}
			}
		}
	}
	static void setInnerTexClass(Int t)  { m_innerTexClass = t; }
	static void setBorderTexClass(Int t) { m_borderTexClass = t; }
	// TheSuperHackers @feature Nemellud 04/08/2026 ShapeFillTool: line width preview.
	// Applies to the line being drawn (which has no LineDef yet) and, if one is selected,
	// to that line as well - so changing the width updates what is already on screen.
	static void setLinePreviewWidth(Int w) {
		m_linePreviewWidth = w;
		if (m_selectedLineId >= 0) {
			for (auto& l : m_lines) {
				if (l.id == m_selectedLineId) { l.previewWidth = w; break; }
			}
		}
	}
	static Int  getLinePreviewWidth() { return m_linePreviewWidth; }

	// TheSuperHackers @feature Nemellud 07/08/2026 ShapeFillTool: mark a spot on a line.
	// Set to a tile position to draw that stretch of the width preview in red instead of
	// blue, with a ring around it. Meant for pointing at the place a validator objects to:
	// on a long winding line a written coordinate is something you have to go hunting for.
	// Deliberately NOT part of LineDef - it is transient feedback about the current
	// settings, not a property of the line, and has no business in the saved session.
	// Pass -1 for x to clear.
	static void setLineFold(Int x, Int y) { m_lineFoldX = x; m_lineFoldY = y; }
	static Int  getLineFoldX() { return m_lineFoldX; }
	static Int  getLineFoldY() { return m_lineFoldY; }

	static Int  getInnerHeight()    { return m_innerHeight; }
	static Bool getAutoBlend()      { return m_autoBlend; }
	static Bool getBlendInward()    { return m_blendInward; }
	static Int  getBorderWidth()    { return m_borderWidth; }
	static Int  getInnerTexClass()  { return m_innerTexClass; }
	static Int  getBorderTexClass() { return m_borderTexClass; }

	static Bool getAutoSave()       { return m_autoSave; }
	static void setAutoSave(Bool b) { m_autoSave = b; }

	static Bool getFillAutoBlend()        { return m_fillAutoBlend; }
	static Bool getFillBlendInward()      { return m_fillBlendInward; }
	static void setFillAutoBlend(Bool b)  { m_fillAutoBlend = b; }
	static void setFillBlendInward(Bool b){ m_fillBlendInward = b; }

	static Bool getInnerAutoBlend()          { return m_innerAutoBlend; }
	static Bool getInnerBlendInward()        { return m_innerBlendInward; }
	static void setInnerAutoBlend(Bool b)    { m_innerAutoBlend = b; }
	static void setInnerBlendInward(Bool b)  { m_innerBlendInward = b; }

	static Int  getSelectedId()    { return m_selectedId; }
	static void setSelectedId(Int id) { m_selectedId = id; }
	static void syncSelectedFromPanel();
	static Bool isPolyDrawing()    { return m_polyDrawing; }
	static Int  getPolyDraftSize() { return (Int)m_polyDraft.size(); }
	static const std::vector<ShapeDef>& getShapes() { return m_shapes; }
	// TheSuperHackers @feature Nemellud 09/08/2026 ShapeFillTool: any shape as a vertex list.
	// A rectangle is two corners and a circle a centre and a radius, so neither could take an
	// inserted point, and nothing outside the tool could read back what a user drew.
	static std::vector<ShapeVertex> outlineOfShape(const ShapeDef& shape);
	static Bool isActive()         { return m_isActive; }

	// Line tool
	static void clearLines();
	static void commitCurrentLine();
	static void bucketFill(CWorldBuilderDoc* pDoc, Int tx, Int ty);
	static const std::vector<LineDef>& getLines() { return m_lines; }
	static Int  getSelectedLineId()       { return m_selectedLineId; }
	static void setSelectedLineId(Int id) { m_selectedLineId = id; }

	// Persistence
	static void saveShapes(const CString& mapPath);
	static void loadShapes(const CString& mapPath);

	// Undo/redo snapshot helpers (public so ShapeFillUndoable can call them)
	static ShapeFillSnapshot captureSnapshot();
	static void              restoreSnapshot(const ShapeFillSnapshot& snap);

	// Drawing overlay
	void drawOverlay(CDC* pDC, WbView* pView) override { drawOverlayStatic(pDC, pView); }
	static void drawOverlayStatic(CDC* pDC, WbView* pView);

private:
	// ---- Static state ----

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
	static Bool       m_autoBlend;
	static Bool       m_blendInward;
	static Bool       m_fillAutoBlend;
	static Bool       m_fillBlendInward;
	static Bool       m_innerAutoBlend;
	static Bool       m_innerBlendInward;
	static Int        m_borderWidth;
	static Int        m_innerTexClass;
	static Int        m_borderTexClass;
	static Bool       m_autoSave;

	// Draft shape shown during drag before mouseUp
	static Bool     m_hasDraft;
	static ShapeDef m_draftShape;

	// Polygon drawing state
	static Bool                    m_polyDrawing;
	static std::vector<ShapeVertex> m_polyDraft;

	// Line drawing state
	static std::vector<LineDef>    m_lines;
	static Int                     m_nextLineId;
	static Int                     m_selectedLineId;
	// TheSuperHackers @feature Nemellud 04/08/2026 ShapeFillTool: width for the line being
	// drawn - the draft is a bare vertex list with no LineDef to carry it.
	static Int                     m_linePreviewWidth;
	static Int                     m_lineFoldX;
	static Int                     m_lineFoldY;
	static Bool                    m_lineDrawing;
	static std::vector<ShapeVertex> m_lineDraft;

	// Snap cursor (live preview while hovering in line/polygon mode)
	static Bool m_hasSnapCorner;
	static Int  m_snapCx, m_snapCy;

	// ---- Instance state ----

	Bool        m_prevTopDown;
	Bool        m_dragging;
	CPoint      m_dragStartView;
	Int         m_dragStartTx, m_dragStartTy;

	// Snapshot captured at gesture start, pushed to undo stack at gesture end
	Bool              m_hasDragSnapshot;
	ShapeFillSnapshot m_undoSnapshotBeforeDrag;

	// Editing shape handles
	Bool        m_movingShape;
	Bool        m_resizingHandle;
	ShapeHandle m_activeHandle;
	Int         m_moveStartTx, m_moveStartTy;

	// Editing line endpoints
	Bool        m_movingLinePoint;
	Int         m_activeLineIdx;
	Int         m_activePointIdx;

	// ---- Private helpers — undo ----
	static void              pushUndo(CWorldBuilderDoc* pDoc, const ShapeFillSnapshot& before);
	static CWorldBuilderDoc* getActiveDoc();

	// ---- Private helpers — shape management ----
	static ShapeDef* findShape(Int id);
	static Int       hitTestShape(Int tx, Int ty);
	Bool             hitTestHandle(Int tx, Int ty, ShapeHandle& outHandle);
	static std::vector<ShapeHandle> getHandles(const ShapeDef& shape);

	void updateDragShape(Int tx, Int ty);

	// ---- Private helpers — rasterization ----
	static TileSet rasterize(const ShapeDef& shape);
	static TileSet rasterizeRect(Int x0, Int y0, Int x1, Int y1, Int border);
	static TileSet rasterizeCircle(Int cx, Int cy, Int r, Int border);
	static TileSet rasterizePolygon(const std::vector<ShapeVertex>& pts, Int border,
	                                const std::vector<ShapeVertex>& innerPts = {});
	static Bool    pointInPolygon(Int tx, Int ty, const std::vector<ShapeVertex>& pts);

	// ---- Private helpers — geometry ----
	static void  rasterizeSegmentToEdges(Int cx0, Int cy0, Int cx1, Int cy1,
	                                     std::vector<bool>& hEdge, std::vector<bool>& vEdge,
	                                     Int playW, Int playH);
	static float distToPolyEdge(float px, float py, const std::vector<ShapeVertex>& poly);
	static bool  pointInPoly(float px, float py, const std::vector<ShapeVertex>& poly);
	static std::pair<Int,Int>       clampToOuterShape(const ShapeDef& shape, Int tx, Int ty);
	static std::vector<ShapeVertex> getEffectiveInner(const ShapeDef& shape);
	static std::vector<ShapeVertex> insetPolygon(const std::vector<ShapeVertex>& pts, Real amount);

	// ---- Private helpers — coordinate conversion ----
	static void viewToTile(WbView* pView, CPoint viewPt, Int& tx, Int& ty);
	static void tileToView(WbView* pView, Int tx, Int ty, Int& sx, Int& sy);
	static void viewToCorner(WbView* pView, CPoint viewPt, Int& cx, Int& cy);
	static void cornerToView(WbView* pView, Int cx, Int cy, Int& sx, Int& sy);
	// TheSuperHackers @feature Nemellud 04/08/2026 ShapeFillTool: fractional corner coords
	static void cornerToViewF(WbView* pView, float cx, float cy, Int& sx, Int& sy);
	static void tileCenterToView(WbView* pView, Int tx, Int ty, Int& sx, Int& sy);

	// ---- Private helpers — drawing ----
	static void drawShape(CDC* pDC, WbView* pView, const ShapeDef& shape, Bool selected);
	static void drawHandle(CDC* pDC, Int sx, Int sy, Bool active);
	static void drawInnerHandle(CDC* pDC, Int sx, Int sy);
	// TheSuperHackers @feature Nemellud 04/08/2026 ShapeFillTool: line width footprint
	static void drawWidthPreview(CDC* pDC, WbView* pView,
	                             const std::vector<ShapeVertex>& pts, Int width);
	static void drawCoordLabel(CDC* pDC, Int sx, Int sy, Int tx, Int ty);
	static void drawSegmentStaircase(CDC* pDC, WbView* pView, Int cx0, Int cy0, Int cx1, Int cy1);
	static void drawCircleStaircase(CDC* pDC, WbView* pView, Int cx, Int cy, Int r);

	// ---- Private helpers — misc ----
	static void    invalidateBothViews();
	static CString shapefillPath(const CString& mapPath);
};
