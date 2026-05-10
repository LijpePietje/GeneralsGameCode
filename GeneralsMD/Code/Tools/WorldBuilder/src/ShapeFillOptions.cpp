/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// ShapeFillOptions.cpp
// Options dialog for the Shape Fill Tool.
// TheSuperHackers @feature Nemellud 09/05/2026 ShapeFillTool options panel: UI controls, mode switching, shape/line list

#include "StdAfx.h"
#include <shellapi.h>
#include "resource.h"
#include "Lib/BaseType.h"
#include "ShapeFillOptions.h"
#include "ShapeFillTool.h"
#include "WorldBuilderView.h"
#include "WHeightMapEdit.h"
#include "TerrainMaterial.h"
#include "TerrainModal.h"
#include "wbview3d.h"
#include "WorldBuilderDoc.h"

ShapeFillOptions* ShapeFillOptions::m_staticThis = nullptr;

ShapeFillOptions::ShapeFillOptions(CWnd* pParent) : m_updating(false)
{
}

void ShapeFillOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL ShapeFillOptions::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_updating = true;
	m_innerHeightPopup.SetupPopSliderButton(this, IDC_SF_INNER_HEIGHT_POPUP, this);
	m_outerHeightPopup.SetupPopSliderButton(this, IDC_SF_OUTER_HEIGHT_POPUP, this);
	m_borderWidthPopup.SetupPopSliderButton(this, IDC_SF_BORDER_WIDTH_POPUP, this);
	m_shapeList.SubclassDlgItem(IDC_SF_SHAPE_LIST, this);
	m_staticThis = this;
	m_updating = false;

	CheckDlgButton(IDC_SF_AUTO_SAVE, ShapeFillTool::getAutoSave() ? BST_CHECKED : BST_UNCHECKED);
	updateFromTool();
	updateTopDownState();
	return TRUE;
}

void ShapeFillOptions::OnAutoSave()
{
	ShapeFillTool::setAutoSave(IsDlgButtonChecked(IDC_SF_AUTO_SAVE) == BST_CHECKED);
}

void ShapeFillOptions::OnClickHelp()
{
	ShellExecute(NULL, "open",
		"https://github.com/LijpePietje/ProjectWorldbuilder/blob/feature/map-reader/worldbuilder-cpp/ShapeFillTool/USAGE.md",
		NULL, NULL, SW_SHOWNORMAL);
}

void ShapeFillOptions::updateFromTool()
{
	if (!m_staticThis || m_staticThis->m_updating) return;
	m_staticThis->m_updating = true;

	// Update height edits
	CString buf;
	buf.Format("%d", ShapeFillTool::getInnerHeight());
	CWnd* pEdit = m_staticThis->GetDlgItem(IDC_SF_INNER_HEIGHT_EDIT);
	if (pEdit) pEdit->SetWindowText(buf);

	buf.Format("%d", ShapeFillTool::getOuterHeight());
	pEdit = m_staticThis->GetDlgItem(IDC_SF_OUTER_HEIGHT_EDIT);
	if (pEdit) pEdit->SetWindowText(buf);

	buf.Format("%d", ShapeFillTool::getBorderWidth());
	pEdit = m_staticThis->GetDlgItem(IDC_SF_BORDER_WIDTH_EDIT);
	if (pEdit) pEdit->SetWindowText(buf);

	// Border Blend radio buttons (shapes)
	Bool autoBlend   = ShapeFillTool::getAutoBlend();
	Bool blendInward = ShapeFillTool::getBlendInward();
	CButton* pNone = (CButton*)m_staticThis->GetDlgItem(IDC_SF_AUTO_BLEND);
	CButton* pOut  = (CButton*)m_staticThis->GetDlgItem(IDC_SF_BLEND_OUT);
	CButton* pIn   = (CButton*)m_staticThis->GetDlgItem(IDC_SF_BLEND_IN);
	if (pNone) pNone->SetCheck(!autoBlend               ? BST_CHECKED : BST_UNCHECKED);
	if (pOut)  pOut ->SetCheck(autoBlend && !blendInward ? BST_CHECKED : BST_UNCHECKED);
	if (pIn)   pIn  ->SetCheck(autoBlend &&  blendInward ? BST_CHECKED : BST_UNCHECKED);
	// Border blend active → outer height driven by terrain, disable manual edit
	CWnd* pOuterEdit  = m_staticThis->GetDlgItem(IDC_SF_OUTER_HEIGHT_EDIT);
	CWnd* pOuterPopup = m_staticThis->GetDlgItem(IDC_SF_OUTER_HEIGHT_POPUP);
	if (pOuterEdit)  pOuterEdit->EnableWindow(!autoBlend);
	if (pOuterPopup) pOuterPopup->EnableWindow(!autoBlend);

	// Fill Blend radio buttons
	Bool fillAuto    = ShapeFillTool::getFillAutoBlend();
	Bool fillInward  = ShapeFillTool::getFillBlendInward();
	CButton* pFNone = (CButton*)m_staticThis->GetDlgItem(IDC_SF_FILL_BLEND_NONE);
	CButton* pFOut  = (CButton*)m_staticThis->GetDlgItem(IDC_SF_FILL_BLEND_OUT);
	CButton* pFIn   = (CButton*)m_staticThis->GetDlgItem(IDC_SF_FILL_BLEND_IN);
	if (pFNone) pFNone->SetCheck(!fillAuto              ? BST_CHECKED : BST_UNCHECKED);
	if (pFOut)  pFOut ->SetCheck(fillAuto && !fillInward ? BST_CHECKED : BST_UNCHECKED);
	if (pFIn)   pFIn  ->SetCheck(fillAuto &&  fillInward ? BST_CHECKED : BST_UNCHECKED);

	// Inner Blend radio buttons (shapes)
	Bool innerAuto    = ShapeFillTool::getInnerAutoBlend();
	Bool innerInward  = ShapeFillTool::getInnerBlendInward();
	CButton* pINone = (CButton*)m_staticThis->GetDlgItem(IDC_SF_INNER_BLEND_NONE);
	CButton* pIOut  = (CButton*)m_staticThis->GetDlgItem(IDC_SF_INNER_BLEND_OUT);
	CButton* pIIn   = (CButton*)m_staticThis->GetDlgItem(IDC_SF_INNER_BLEND_IN);
	if (pINone) pINone->SetCheck(!innerAuto                ? BST_CHECKED : BST_UNCHECKED);
	if (pIOut)  pIOut ->SetCheck(innerAuto && !innerInward ? BST_CHECKED : BST_UNCHECKED);
	if (pIIn)   pIIn  ->SetCheck(innerAuto &&  innerInward ? BST_CHECKED : BST_UNCHECKED);

	m_staticThis->refreshModeButtons();
	m_staticThis->refreshTexButtons();

	// Populate shape + line list
	CListBox* pList = (CListBox*)m_staticThis->GetDlgItem(IDC_SF_SHAPE_LIST);
	if (pList) {
		pList->ResetContent();
		const auto& shapes = ShapeFillTool::getShapes();
		for (const auto& s : shapes) {
			CString name;
			if (s.type == SHAPE_RECT)        name.Format("Rect #%d", s.id);
			else if (s.type == SHAPE_CIRCLE) name.Format("Circle #%d", s.id);
			else                             name.Format("Polygon #%d", s.id);
			Int idx = pList->AddString(name);
			pList->SetItemData(idx, (DWORD_PTR)s.id);
			if (s.id == ShapeFillTool::getSelectedId())
				pList->SetCurSel(idx);
		}
		const auto& lines = ShapeFillTool::getLines();
		for (const auto& l : lines) {
			CString name;
			name.Format("Line #%d", l.id);
			Int idx = pList->AddString(name);
			pList->SetItemData(idx, (DWORD_PTR)(-(l.id + 1)));  // negative = line
			if (l.id == ShapeFillTool::getSelectedLineId())
				pList->SetCurSel(idx);
		}
	}

	m_staticThis->m_updating = false;
}

void ShapeFillOptions::updateCoordLabel(Int tx, Int ty)
{
	if (!m_staticThis) return;
	CString buf;
	buf.Format("X:%d  Y:%d", tx, ty);
	CWnd* pLabel = m_staticThis->GetDlgItem(IDC_SF_COORDS_LABEL);
	if (pLabel) pLabel->SetWindowText(buf);
}

void ShapeFillOptions::refreshModeButtons()
{
	SFToolMode mode = ShapeFillTool::getMode();
	struct { Int id; SFToolMode mode; } buttons[] = {
		{IDC_SF_MODE_RECT,    SF_DRAW_RECT},
		{IDC_SF_MODE_CIRCLE,  SF_DRAW_CIRCLE},
		{IDC_SF_MODE_POLYGON, SF_DRAW_POLYGON},
		{IDC_SF_MODE_SELECT,  SF_SELECT},
		{IDC_SF_MODE_LINE,    SF_DRAW_LINE},
		{IDC_SF_MODE_FILL,    SF_BUCKET_FILL},
		{IDC_SF_MODE_EDIT,    SF_EDIT_SHAPE},
	};
	for (auto& b : buttons) {
		CButton* btn = (CButton*)GetDlgItem(b.id);
		if (btn) btn->SetCheck(mode == b.mode ? BST_CHECKED : BST_UNCHECKED);
	}

	// Show "Close polygon" button only when drawing polygon with 3+ points
	CWnd* pFinish = GetDlgItem(IDC_SF_FINISH_POLY);
	if (pFinish) {
		Bool showFinish = (ShapeFillTool::getMode() == SF_DRAW_POLYGON)
		               && ShapeFillTool::isPolyDrawing()
		               && ShapeFillTool::getPolyDraftSize() >= 3;
		pFinish->ShowWindow(showFinish ? SW_SHOW : SW_HIDE);
	}

	refreshModeUI();
}

void ShapeFillOptions::refreshTexButtons()
{
	// Update inner texture button label
	CWnd* pBtn = GetDlgItem(IDC_SF_INNER_TEX_BTN);
	if (pBtn) {
		Int tc = ShapeFillTool::getInnerTexClass();
		CString label = (tc >= 0) ? WorldHeightMapEdit::getTexClassName(tc).str() : "(none)";
		pBtn->SetWindowText(label);
	}

	pBtn = GetDlgItem(IDC_SF_BORDER_TEX_BTN);
	if (pBtn) {
		Int tc = ShapeFillTool::getBorderTexClass();
		CString label = (tc >= 0) ? WorldHeightMapEdit::getTexClassName(tc).str() : "(same as inner)";
		pBtn->SetWindowText(label);
	}
}

// Sync current panel settings into selected shape and redraw overlay live
static void syncAndRedraw()
{
	ShapeFillTool::syncSelectedFromPanel();
	CWorldBuilderView* p2D = CWorldBuilderDoc::GetActive2DView();
	WbView3d*          p3D = CWorldBuilderDoc::GetActive3DView();
	if (p2D) p2D->Invalidate(false);
	if (p3D) p3D->Invalidate(false);
}

// -------------------------------------------------------------------------
// Mode buttons
// -------------------------------------------------------------------------

void ShapeFillOptions::OnModeRect()    { ShapeFillTool::setMode(SF_DRAW_RECT);    refreshModeButtons(); }
void ShapeFillOptions::OnModeCircle()  { ShapeFillTool::setMode(SF_DRAW_CIRCLE);  refreshModeButtons(); }
void ShapeFillOptions::OnModePolygon() { ShapeFillTool::setMode(SF_DRAW_POLYGON); refreshModeButtons(); }
void ShapeFillOptions::OnModeSelect()  { ShapeFillTool::setMode(SF_SELECT);       refreshModeButtons(); }
void ShapeFillOptions::OnModeLine()    { ShapeFillTool::setMode(SF_DRAW_LINE);    refreshModeButtons(); }
void ShapeFillOptions::OnModeFill()    { ShapeFillTool::setMode(SF_BUCKET_FILL);  refreshModeButtons(); }
void ShapeFillOptions::OnModeEdit()    { ShapeFillTool::setMode(SF_EDIT_SHAPE);   refreshModeButtons(); }

void ShapeFillOptions::OnClickClearLines()
{
	ShapeFillTool::clearLines();
	updateFromTool();
}

void ShapeFillOptions::OnClickFinishLine()
{
	ShapeFillTool::commitCurrentLine();
}

void ShapeFillOptions::OnClickFinishPoly()
{
	ShapeFillTool::finishPolygon();
	refreshModeButtons();
}

// -------------------------------------------------------------------------
// Apply / Delete / Copy / Paste / Flip
// -------------------------------------------------------------------------

void ShapeFillOptions::OnClickApply()
{
	// Get the current document to apply terrain changes
	CWnd* pWnd = AfxGetMainWnd();
	CFrameWnd* pFrame = pWnd ? (CFrameWnd*)pWnd : nullptr;
	CWorldBuilderDoc* pDoc = pFrame ?
		(CWorldBuilderDoc*)pFrame->GetActiveDocument() : nullptr;
	if (pDoc) ShapeFillTool::applySelectedShape(pDoc);
}

void ShapeFillOptions::OnClickDelete()    { ShapeFillTool::deleteSelectedShape(); updateFromTool(); syncAndRedraw(); }
void ShapeFillOptions::OnClickDuplicate()
{
	ShapeFillTool::copySelectedShape();
	ShapeFillTool::pasteShape();
	updateFromTool();
	AfxGetMainWnd()->Invalidate();
}
void ShapeFillOptions::OnClickFlipH()   { ShapeFillTool::flipSelectedShape(true);  AfxGetMainWnd()->Invalidate(); }
void ShapeFillOptions::OnClickFlipV()   { ShapeFillTool::flipSelectedShape(false); AfxGetMainWnd()->Invalidate(); }

// -------------------------------------------------------------------------
// Height / border width edits
// -------------------------------------------------------------------------

void ShapeFillOptions::OnChangeInnerHeight()
{
	if (m_updating) return;
	CWnd* pEdit = GetDlgItem(IDC_SF_INNER_HEIGHT_EDIT);
	if (!pEdit) return;
	char buf[32]; pEdit->GetWindowText(buf, sizeof(buf));
	Int val;
	if (1 == sscanf(buf, "%d", &val)) {
		ShapeFillTool::setInnerHeight(std::max(0, std::min(80, val)));
		syncAndRedraw();
	}
}

void ShapeFillOptions::OnChangeOuterHeight()
{
	if (m_updating) return;
	CWnd* pEdit = GetDlgItem(IDC_SF_OUTER_HEIGHT_EDIT);
	if (!pEdit) return;
	char buf[32]; pEdit->GetWindowText(buf, sizeof(buf));
	Int val;
	if (1 == sscanf(buf, "%d", &val)) {
		ShapeFillTool::setOuterHeight(std::max(0, std::min(80, val)));
		syncAndRedraw();
	}
}

void ShapeFillOptions::OnChangeBorderWidth()
{
	if (m_updating) return;
	CWnd* pEdit = GetDlgItem(IDC_SF_BORDER_WIDTH_EDIT);
	if (!pEdit) return;
	char buf[32]; pEdit->GetWindowText(buf, sizeof(buf));
	Int val;
	if (1 == sscanf(buf, "%d", &val)) {
		ShapeFillTool::setBorderWidth(std::max(0, std::min(30, val)));
		syncAndRedraw();
	}
}

void ShapeFillOptions::OnBlendNone()
{
	ShapeFillTool::setAutoBlend(false);
	ShapeFillTool::setBlendInward(false);
	CWnd* pOuterEdit  = GetDlgItem(IDC_SF_OUTER_HEIGHT_EDIT);
	CWnd* pOuterPopup = GetDlgItem(IDC_SF_OUTER_HEIGHT_POPUP);
	if (pOuterEdit)  pOuterEdit->EnableWindow(true);
	if (pOuterPopup) pOuterPopup->EnableWindow(true);
	syncAndRedraw();
}

void ShapeFillOptions::OnBlendOut()
{
	ShapeFillTool::setAutoBlend(true);
	ShapeFillTool::setBlendInward(false);
	CWnd* pOuterEdit  = GetDlgItem(IDC_SF_OUTER_HEIGHT_EDIT);
	CWnd* pOuterPopup = GetDlgItem(IDC_SF_OUTER_HEIGHT_POPUP);
	if (pOuterEdit)  pOuterEdit->EnableWindow(false);
	if (pOuterPopup) pOuterPopup->EnableWindow(false);
	syncAndRedraw();
}

void ShapeFillOptions::OnBlendIn()
{
	ShapeFillTool::setAutoBlend(true);
	ShapeFillTool::setBlendInward(true);
	CWnd* pOuterEdit  = GetDlgItem(IDC_SF_OUTER_HEIGHT_EDIT);
	CWnd* pOuterPopup = GetDlgItem(IDC_SF_OUTER_HEIGHT_POPUP);
	if (pOuterEdit)  pOuterEdit->EnableWindow(false);
	if (pOuterPopup) pOuterPopup->EnableWindow(false);
	syncAndRedraw();
}

void ShapeFillOptions::OnFillBlendNone()
{
	ShapeFillTool::setFillAutoBlend(false);
	ShapeFillTool::setFillBlendInward(false);
}

void ShapeFillOptions::OnFillBlendOut()
{
	ShapeFillTool::setFillAutoBlend(true);
	ShapeFillTool::setFillBlendInward(false);
}

void ShapeFillOptions::OnFillBlendIn()
{
	ShapeFillTool::setFillAutoBlend(true);
	ShapeFillTool::setFillBlendInward(true);
}

void ShapeFillOptions::OnInnerBlendNone()
{
	ShapeFillTool::setInnerAutoBlend(false);
	ShapeFillTool::setInnerBlendInward(false);
}

void ShapeFillOptions::OnInnerBlendOut()
{
	ShapeFillTool::setInnerAutoBlend(true);
	ShapeFillTool::setInnerBlendInward(false);
}

void ShapeFillOptions::OnInnerBlendIn()
{
	ShapeFillTool::setInnerAutoBlend(true);
	ShapeFillTool::setInnerBlendInward(true);
}

// -------------------------------------------------------------------------
// Texture selectors — use the official TerrainModal picker (same as the game)
// -------------------------------------------------------------------------

static Int showOfficialTexPicker(CWnd* pParent)
{
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)
		((CFrameWnd*)AfxGetMainWnd())->GetActiveDocument();
	if (!pDoc) return -2;
	WorldHeightMapEdit* pMap = pDoc->GetHeightMap();
	if (!pMap) return -2;

	TerrainModal dlg("", pMap, pParent);
	if (dlg.DoModal() == IDOK)
		return dlg.getNewNdx();
	return -2; // cancelled
}

void ShapeFillOptions::OnSelectInnerTex()
{
	Int tc = showOfficialTexPicker(this);
	if (tc == -2) return;
	ShapeFillTool::setInnerTexClass(tc);
	refreshTexButtons();
}

void ShapeFillOptions::OnSelectBorderTex()
{
	Int tc = showOfficialTexPicker(this);
	if (tc == -2) return;
	ShapeFillTool::setBorderTexClass(tc);
	refreshTexButtons();
}

// -------------------------------------------------------------------------
// Popup sliders
// -------------------------------------------------------------------------

void ShapeFillOptions::GetPopSliderInfo(const long sliderID, long* pMin, long* pMax, long* pLineSize, long* pInitial)
{
	switch (sliderID) {
		case IDC_SF_INNER_HEIGHT_POPUP:
			*pMin = 0; *pMax = 80; *pInitial = ShapeFillTool::getInnerHeight(); *pLineSize = 1; break;
		case IDC_SF_OUTER_HEIGHT_POPUP:
			*pMin = 0; *pMax = 80; *pInitial = ShapeFillTool::getOuterHeight(); *pLineSize = 1; break;
		case IDC_SF_BORDER_WIDTH_POPUP:
			*pMin = 0; *pMax = 30; *pInitial = ShapeFillTool::getBorderWidth(); *pLineSize = 1; break;
		default:
			DEBUG_CRASH(("ShapeFillOptions: unknown slider"));
	}
}

void ShapeFillOptions::PopSliderChanged(const long sliderID, long theVal)
{
	CString str;
	CWnd* pEdit;
	switch (sliderID) {
		case IDC_SF_INNER_HEIGHT_POPUP:
			ShapeFillTool::setInnerHeight((Int)theVal);
			str.Format("%d", theVal);
			pEdit = GetDlgItem(IDC_SF_INNER_HEIGHT_EDIT);
			if (pEdit) pEdit->SetWindowText(str);
			break;
		case IDC_SF_OUTER_HEIGHT_POPUP:
			ShapeFillTool::setOuterHeight((Int)theVal);
			str.Format("%d", theVal);
			pEdit = GetDlgItem(IDC_SF_OUTER_HEIGHT_EDIT);
			if (pEdit) pEdit->SetWindowText(str);
			break;
		case IDC_SF_BORDER_WIDTH_POPUP:
			ShapeFillTool::setBorderWidth((Int)theVal);
			str.Format("%d", theVal);
			pEdit = GetDlgItem(IDC_SF_BORDER_WIDTH_EDIT);
			if (pEdit) pEdit->SetWindowText(str);
			syncAndRedraw();
			break;
		default:
			DEBUG_CRASH(("ShapeFillOptions: unknown slider"));
	}
}

void ShapeFillOptions::PopSliderFinished(const long sliderID, long theVal)
{
	(void)sliderID; (void)theVal;
}

void ShapeFillOptions::OnShapeListSelChanged()
{
	CListBox* pList = (CListBox*)GetDlgItem(IDC_SF_SHAPE_LIST);
	if (!pList) return;
	Int sel = pList->GetCurSel();
	if (sel == LB_ERR) return;
	Int data = (Int)pList->GetItemData(sel);
	if (data >= 0) {
		ShapeFillTool::setSelectedId(data);
		ShapeFillTool::setSelectedLineId(-1);
		ShapeFillTool::setMode(SF_SELECT);
		refreshModeButtons();
	} else {
		ShapeFillTool::setSelectedLineId(-(data + 1));
		ShapeFillTool::setSelectedId(-1);
	}
	CWorldBuilderView* p2D = CWorldBuilderDoc::GetActive2DView();
	WbView3d* p3D = CWorldBuilderDoc::GetActive3DView();
	if (p2D) p2D->Invalidate(false);
	if (p3D) p3D->Invalidate(false);
}

void ShapeFillOptions::OnClickTopDown()
{
	WbView3d* p3D = CWorldBuilderDoc::GetActive3DView();
	if (p3D) {
		p3D->setTopDownProjection(true);
		updateTopDownState();
	}
}

void ShapeFillOptions::OnClick3D()
{
	WbView3d* p3D = CWorldBuilderDoc::GetActive3DView();
	if (p3D) {
		p3D->setTopDownProjection(false);
		updateTopDownState();
	}
}

/* static */ void ShapeFillOptions::updateTopDownState()
{
	if (!m_staticThis) return;
	WbView3d* p3D = CWorldBuilderDoc::GetActive3DView();
	// No 3D view = still show all controls (2D-only layout is always top-down)
	bool topDown = !p3D || p3D->getTopDownProjection();

	CWnd* pChild = m_staticThis->GetWindow(GW_CHILD);
	while (pChild) {
		int id = pChild->GetDlgCtrlID();
		if (id == IDC_SF_TOPDOWN_BTN)
			pChild->ShowWindow(topDown ? SW_HIDE : SW_SHOW);
		else
			pChild->ShowWindow(topDown ? SW_SHOW : SW_HIDE);
		pChild = pChild->GetNextWindow();
	}

	// Apply mode-specific visibility on top of the topDown all-show
	if (topDown)
		m_staticThis->refreshModeButtons();
}

void ShapeFillOptions::refreshModeUI()
{
	SFToolMode mode = ShapeFillTool::getMode();
	bool isLine  = (mode == SF_DRAW_LINE);
	bool isFill  = (mode == SF_BUCKET_FILL);
	bool isEdit  = (mode == SF_EDIT_SHAPE);
	bool isShape = !isLine && !isFill;

	auto show = [&](int id, bool visible) {
		CWnd* p = GetDlgItem(id);
		if (p) p->ShowWindow(visible ? SW_SHOW : SW_HIDE);
	};
	auto enable = [&](int id, bool enabled) {
		CWnd* p = GetDlgItem(id);
		if (p) p->EnableWindow(enabled ? TRUE : FALSE);
	};

	// Heights + border width: shapes only
	show(IDC_SF_INNER_HEIGHT_EDIT,  isShape);
	show(IDC_SF_INNER_HEIGHT_POPUP, isShape);
	show(IDC_SF_OUTER_HEIGHT_EDIT,  isShape);
	show(IDC_SF_OUTER_HEIGHT_POPUP, isShape);
	show(IDC_SF_BORDER_WIDTH_EDIT,  isShape);
	show(IDC_SF_BORDER_WIDTH_POPUP, isShape);
	// Border width is overruled by free inner polygon in Edit mode — gray it out
	enable(IDC_SF_BORDER_WIDTH_EDIT,  !isEdit);
	enable(IDC_SF_BORDER_WIDTH_POPUP, !isEdit);

	// Textures: inner tex for shapes + fill; border tex for shapes only
	show(IDC_SF_INNER_TEX_BTN,   isShape || isFill);
	show(IDC_SF_BORDER_TEX_BTN,  isShape);

	// Inner Blend group: shapes only (None / Out / In)
	show(IDC_SF_INNER_BLEND_GRP,  isShape);
	show(IDC_SF_INNER_BLEND_NONE, isShape);
	show(IDC_SF_INNER_BLEND_OUT,  isShape);
	show(IDC_SF_INNER_BLEND_IN,   isShape);
	// Outer Blend group: shapes only (None / Out / In)
	show(IDC_SF_BORDER_BLEND_GRP, isShape);
	show(IDC_SF_AUTO_BLEND,       isShape);
	show(IDC_SF_BLEND_OUT,        isShape);
	show(IDC_SF_BLEND_IN,         isShape);
	// Fill Blend group: fill mode only
	show(IDC_SF_FILL_BLEND_GRP,   isFill);
	show(IDC_SF_FILL_BLEND_NONE,  isFill);
	show(IDC_SF_FILL_BLEND_OUT,   isFill);
	show(IDC_SF_FILL_BLEND_IN,    isFill);

	// Actions: shapes only (in edit mode: only Apply, no shape management)
	show(IDC_SF_APPLY_BTN,  isShape);
	show(IDC_SF_COPY_BTN,   isShape && !isEdit);
	show(IDC_SF_FLIP_H_BTN, isShape && !isEdit);
	show(IDC_SF_FLIP_V_BTN, isShape && !isEdit);

	// Line-specific buttons
	show(IDC_SF_CLEAR_LINES,  isLine);
	show(IDC_SF_FINISH_LINE,  isLine);
	// Delete works for selected lines too
	show(IDC_SF_DELETE_BTN, (isShape && !isEdit) || isLine);

	// Shape + line list: visible in shape and line modes (not in edit mode)
	show(IDC_SF_SHAPE_LIST, (isShape || isLine) && !isEdit);
}

BEGIN_MESSAGE_MAP(ShapeFillOptions, COptionsPanel)
	ON_BN_CLICKED(IDC_SF_MODE_RECT,    OnModeRect)
	ON_BN_CLICKED(IDC_SF_MODE_CIRCLE,  OnModeCircle)
	ON_BN_CLICKED(IDC_SF_MODE_POLYGON, OnModePolygon)
	ON_BN_CLICKED(IDC_SF_MODE_SELECT,  OnModeSelect)
	ON_BN_CLICKED(IDC_SF_FINISH_POLY,  OnClickFinishPoly)
	ON_BN_CLICKED(IDC_SF_APPLY_BTN,    OnClickApply)
	ON_BN_CLICKED(IDC_SF_DELETE_BTN,   OnClickDelete)
	ON_BN_CLICKED(IDC_SF_COPY_BTN,     OnClickDuplicate)
	ON_BN_CLICKED(IDC_SF_FLIP_H_BTN,   OnClickFlipH)
	ON_BN_CLICKED(IDC_SF_FLIP_V_BTN,   OnClickFlipV)
	ON_BN_CLICKED(IDC_SF_INNER_TEX_BTN, OnSelectInnerTex)
	ON_BN_CLICKED(IDC_SF_BORDER_TEX_BTN, OnSelectBorderTex)
	ON_EN_CHANGE(IDC_SF_INNER_HEIGHT_EDIT, OnChangeInnerHeight)
	ON_EN_CHANGE(IDC_SF_OUTER_HEIGHT_EDIT, OnChangeOuterHeight)
	ON_EN_CHANGE(IDC_SF_BORDER_WIDTH_EDIT, OnChangeBorderWidth)
	ON_BN_CLICKED(IDC_SF_AUTO_BLEND,      OnBlendNone)
	ON_BN_CLICKED(IDC_SF_BLEND_OUT,       OnBlendOut)
	ON_BN_CLICKED(IDC_SF_BLEND_IN,        OnBlendIn)
	ON_BN_CLICKED(IDC_SF_FILL_BLEND_NONE,  OnFillBlendNone)
	ON_BN_CLICKED(IDC_SF_FILL_BLEND_OUT,   OnFillBlendOut)
	ON_BN_CLICKED(IDC_SF_FILL_BLEND_IN,    OnFillBlendIn)
	ON_BN_CLICKED(IDC_SF_INNER_BLEND_NONE, OnInnerBlendNone)
	ON_BN_CLICKED(IDC_SF_INNER_BLEND_OUT,  OnInnerBlendOut)
	ON_BN_CLICKED(IDC_SF_INNER_BLEND_IN,   OnInnerBlendIn)
	ON_LBN_SELCHANGE(IDC_SF_SHAPE_LIST, OnShapeListSelChanged)
	ON_BN_CLICKED(IDC_SF_TOPDOWN_BTN,  OnClickTopDown)
	ON_BN_CLICKED(IDC_SF_3D_BTN,       OnClick3D)
	ON_BN_CLICKED(IDC_SF_MODE_LINE,    OnModeLine)
	ON_BN_CLICKED(IDC_SF_MODE_FILL,    OnModeFill)
	ON_BN_CLICKED(IDC_SF_MODE_EDIT,    OnModeEdit)
	ON_BN_CLICKED(IDC_SF_CLEAR_LINES,  OnClickClearLines)
	ON_BN_CLICKED(IDC_SF_FINISH_LINE,  OnClickFinishLine)
	ON_BN_CLICKED(IDC_SF_AUTO_SAVE,    OnAutoSave)
	ON_BN_CLICKED(IDC_SF_HELP_BTN,    OnClickHelp)
END_MESSAGE_MAP()
