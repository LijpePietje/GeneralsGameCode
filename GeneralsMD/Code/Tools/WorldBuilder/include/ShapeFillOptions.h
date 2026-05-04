/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#pragma once

#include "WBPopupSlider.h"
#include "OptionsPanel.h"

class ShapeFillOptions : public COptionsPanel, public PopupSliderOwner
{
public:
	ShapeFillOptions(CWnd* pParent = nullptr);

	enum { IDD = IDD_SHAPE_FILL_OPTIONS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;
	virtual void OnOK() override {}
	virtual void OnCancel() override {}

	afx_msg void OnModeRect();
	afx_msg void OnModeCircle();
	afx_msg void OnModePolygon();
	afx_msg void OnModeSelect();
	afx_msg void OnClickApply();
	afx_msg void OnClickDelete();
	afx_msg void OnClickCopy();
	afx_msg void OnClickPaste();
	afx_msg void OnClickFlipH();
	afx_msg void OnClickFlipV();
	afx_msg void OnClickFinishPoly();
	afx_msg void OnChangeInnerHeight();
	afx_msg void OnChangeOuterHeight();
	afx_msg void OnChangeBorderWidth();
	afx_msg void OnAutoBlendChanged();
	afx_msg void OnSelectInnerTex();
	afx_msg void OnSelectBorderTex();
	afx_msg void OnShapeListSelChanged();
	afx_msg void OnClickTopDown();
	afx_msg void OnClick3D();
	afx_msg void OnModeLine();
	afx_msg void OnModeFill();
	afx_msg void OnModeEdit();
	afx_msg void OnClickClearLines();

	DECLARE_MESSAGE_MAP()

	virtual void GetPopSliderInfo(const long sliderID, long* pMin, long* pMax, long* pLineSize, long* pInitial) override;
	virtual void PopSliderChanged(const long sliderID, long theVal) override;
	virtual void PopSliderFinished(const long sliderID, long theVal) override;

public:
	static void updateFromTool();
	static void updateCoordLabel(Int tx, Int ty);
	static void updateTopDownState();

private:
	static ShapeFillOptions* m_staticThis;

	Bool m_updating;
	CListBox m_shapeList;
	WBPopupSliderButton m_innerHeightPopup;
	WBPopupSliderButton m_outerHeightPopup;
	WBPopupSliderButton m_borderWidthPopup;

	void refreshModeButtons();
	void refreshTexButtons();
	void refreshModeUI();
};
