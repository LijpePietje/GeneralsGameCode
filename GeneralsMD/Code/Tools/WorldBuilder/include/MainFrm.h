/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "MyToolbar.h"
#include "brushoptions.h"
#include "FeatherOptions.h"
#include "CellWidth.h"
#include "TerrainMaterial.h"
#include "BlendMaterial.h"
#include "MoundOptions.h"
#include "ObjectOptions.h"
#include "FenceOptions.h"
#include "RoadOptions.h"
#include "ContourOptions.h"
#include "MeshMoldOptions.h"
#include "WaypointOptions.h"
#include "WaterOptions.h"
#include "LightOptions.h"
#include "mapobjectprops.h"
#include "GroveOptions.h"
#include "RampOptions.h"
#include "GlobalLightOptions.h"
#include "CameraOptions.h"
#include "ScorchOptions.h"
#include "BuildList.h"
#include "RulerOptions.h"
#include "ShapeFillOptions.h"

#define TWO_D_WINDOW_SECTION "TwoDWindow"
#define MAIN_FRAME_SECTION "MainFrame"

class LayersList;
class ScriptDialog;

class CMainFrame : public CFrameWnd
{
  DECLARE_DYNAMIC(CMainFrame)

public:
	CMainFrame();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMainFrame() override;
#ifdef RTS_DEBUG
	virtual void AssertValid() const override;
	virtual void Dump(CDumpContext& dc) const override;
#endif

	static CMainFrame *GetMainFrame() { return TheMainFrame; }

	void showOptionsDialog(Int dialogID);
	void OnEditGloballightoptions();
	void ResetWindowPositions();
	void adjustWindowSize();
	Bool isAutoSaving() {return m_autoSaving;};
	void handleCameraChange();
	void onEditScripts();

protected:  // control bar embedded members
	CStatusBar					m_wndStatusBar;
	CToolBar						m_wndToolBar;
	CToolBar						m_floatingToolBar;
	BrushOptions				m_brushOptions;
	TerrainMaterial			m_terrainMaterial;
	BlendMaterial				m_blendMaterial;
	ObjectOptions				m_objectOptions;
	FenceOptions				m_fenceOptions;
	MapObjectProps			m_mapObjectProps;
	MoundOptions				m_moundOptions;
	RoadOptions					m_roadOptions;
	FeatherOptions			m_featherOptions;
	MeshMoldOptions			m_meshMoldOptions;
	WaypointOptions			m_waypointOptions;
	WaterOptions				m_waterOptions;
	LightOptions				m_lightOptions;
	BuildList						m_buildListOptions;
	GroveOptions				m_groveOptions;
	RampOptions					m_rampOptions;
	ScorchOptions				m_scorchOptions;
	ShapeFillOptions		m_shapeFillOptions;
	COptionsPanel				m_noOptions;
	GlobalLightOptions	m_globalLightOptions;
	CameraOptions				m_cameraOptions;
	LayersList*					m_layersList;
	ScriptDialog*				m_scriptDialog;
	RulerOptions				m_rulerOptions;

	CWnd							*m_curOptions;
	Int								m_curOptionsX;
	Int								m_curOptionsY;
	Int								m_optionsPanelWidth;
	Int								m_optionsPanelHeight;
	Int								m_globalLightOptionsWidth;
	Int								m_globalLightOptionsHeight;

	Int								m_3dViewWidth;

	Bool							m_autoSaving;  ///< True if we are autosaving.
	UINT							m_hAutoSaveTimer;  ///< Timer that triggers for autosave.
	Bool							m_autoSave;    ///< If true, then do autosaves.
	Int								m_autoSaveInterval;  ///< Time between autosaves in seconds.

	static CMainFrame *TheMainFrame;

// Generated message map functions
protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnMove(int x, int y);
	afx_msg void OnViewBrushfeedback();
	afx_msg void OnUpdateViewBrushfeedback(CCmdUI* pCmdUI);
	afx_msg void OnViewShoweffects();
	afx_msg void OnUpdateViewShoweffects(CCmdUI* pCmdUI);
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnEditCameraoptions();
	//}}AFX_MSG
	// TheSuperHackers @feature Nemellud 23/05/2026 DarkTheme: dark main frame background
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	// TheSuperHackers @feature Nemellud 24/05/2026 EmbeddedMode: ShapeFill mode via pipe
	afx_msg LRESULT OnWbSfMode(WPARAM wParam, LPARAM lParam);
	// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: set view projection via pipe
	afx_msg LRESULT OnWbSetProjection(WPARAM wParam, LPARAM lParam);
	// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: ShapeFill full panel control via pipe
	afx_msg LRESULT OnWbSfGetState(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSfSetInt(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSfAction(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSfOpenTex(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSfSelect(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSfGetTexList(WPARAM wParam, LPARAM lParam);
	// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: Tier B — brush/mound/texture tool control via pipe
	afx_msg LRESULT OnWbBrushSet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbBrushGetState(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbMoundSet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbMoundGetState(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbTexSet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbTexGetState(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbTexAction(WPARAM wParam, LPARAM lParam);
	// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: Tier C — feather/scorch/meshmold/water/ramp via pipe
	afx_msg LRESULT OnWbFeatherSet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbFeatherGet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbScorchSet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbScorchGet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbMeshmoldSet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbMeshmoldGet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbMeshmoldAction(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbMeshmoldList(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbFloodfillAt(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbWaterSet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbWaterGet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbRampSet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbRampGet(WPARAM wParam, LPARAM lParam);
	// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: Tier D — contour via pipe
	afx_msg LRESULT OnWbContourSet(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbContourGet(WPARAM wParam, LPARAM lParam);
	// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: Tier H — map data read-back via pipe
	afx_msg LRESULT OnWbGetMapInfo(WPARAM wParam, LPARAM lParam);
	// TheSuperHackers @feature Nemellud 01/08/2026 MapINI: reapply the open map's map.ini
	afx_msg LRESULT OnWbReloadMapIni(WPARAM wParam, LPARAM lParam);
	// TheSuperHackers @feature Nemellud 03/08/2026 ShapeFillTool: read a drawn line's points
	afx_msg LRESULT OnWbSfGetLine(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbFxPreview(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbDelObjByTemplate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSetPaletteObj(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSfGetShape(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbGetHeightmap(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbGetTexturemap(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbGetObjects(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbGetWaypoints(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbGetTriggers(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbGetTeams(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbGetSelected(WPARAM wParam, LPARAM lParam);
	// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: Tier I terrain write handlers
	afx_msg LRESULT OnWbSfCreatePipe(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbMapHeightSet(WPARAM wParam, LPARAM lParam);
	// TheSuperHackers @feature Nemellud 17/07/2026 EmbeddedMode: height blit handler
	afx_msg LRESULT OnWbHeightBlit(WPARAM wParam, LPARAM lParam);
	// TheSuperHackers @feature Nemellud 17/07/2026 EmbeddedMode: texture blit handler
	afx_msg LRESULT OnWbTextureBlit(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbPlaceWaypoint(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbLinkWaypoints(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbPlaceObject(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbPlantTree(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbPlantGrove(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbGetViewState(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbRotateSelected(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbObjGetProps(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbObjSetProp(WPARAM wParam, LPARAM lParam);
	// Tier J — SidesList wizard
	afx_msg LRESULT OnWbGetSideList(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSetPlayer(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSetTeam(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbDelTeam(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSetScript(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbDelScript(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSetGroup(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbAddSkirmish(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbAddPlayer(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbDelPlayer(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbDelWaypoint(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbAddTrigger(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbDelTrigger(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSetTrigger(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSelectObject(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbPipeCmd(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbNewMap(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbResizeMap(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbPlaceRoad(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbListRoads(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbDelRoad(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSelRoad(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbPlaceBridge(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbListBridges(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSetBridgeName(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSetRoadTool(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSaveToPath(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbGetLighting(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbSetLighting(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbResetLighting(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWbImpassableView(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.
