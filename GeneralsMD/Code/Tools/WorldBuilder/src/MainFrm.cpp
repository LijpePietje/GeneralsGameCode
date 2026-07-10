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

// MainFrm.cpp : implementation of the CMainFrame class
//

#include "StdAfx.h"
#include "MainFrm.h"

// TheSuperHackers @feature Nemellud 23/05/2026 DarkTheme: dark title bar via DWM API
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include "Common/GlobalData.h"

// TheSuperHackers @feature Nemellud 10/06/2026 EmbeddedMode: global lighting via pipe
#include "Lib/BaseType.h"
#include "GlobalLightOptions.h"

#include "DrawObject.h"
#include "LayersList.h"
#include "WHeightMapEdit.h"
#include "wbview3d.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"

#include "ScriptDialog.h"
// TheSuperHackers @feature Nemellud 24/05/2026 EmbeddedMode: ShapeFill mode dispatch
#include "WbPipeServer.h"
#include "NewHeightMap.h"
#include "ShapeFillTool.h"
// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: full tool + map read-back handlers
#include "BrushTool.h"
#include "brushoptions.h"
#include "MoundOptions.h"
#include "MoundTool.h"
#include "TerrainMaterial.h"
#include "TileTool.h"
#include "FeatherOptions.h"
#include "ScorchOptions.h"
#include "MeshMoldOptions.h"
#include "MeshMoldTool.h"
#include "Common/FileSystem.h"
#include "WaterOptions.h"
#include "RampOptions.h"
#include "ContourOptions.h"
#include "Common/MapObject.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/Scripts.h"
#include "W3DDevice/GameClient/HeightMap.h"  // TheTerrainRenderObject for grove terrain checks
#include "Common/WellKnownKeys.h"
#include "GameLogic/SidesList.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "CUndoable.h"

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_WM_MOVE()
	ON_COMMAND(ID_VIEW_BRUSHFEEDBACK, OnViewBrushfeedback)
	ON_UPDATE_COMMAND_UI(ID_VIEW_BRUSHFEEDBACK, OnUpdateViewBrushfeedback)
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_WM_CANCELMODE()
	ON_COMMAND(ID_EDIT_CAMERAOPTIONS, OnEditCameraoptions)
	//}}AFX_MSG_MAP
	ON_WM_ERASEBKGND()
	ON_MESSAGE(WM_WB_SF_MODE,        OnWbSfMode)
	ON_MESSAGE(WM_WB_SET_PROJECTION, OnWbSetProjection)
	ON_MESSAGE(WM_WB_SF_GET_STATE,    OnWbSfGetState)
	ON_MESSAGE(WM_WB_SF_SETINT,       OnWbSfSetInt)
	ON_MESSAGE(WM_WB_SF_ACTION,       OnWbSfAction)
	ON_MESSAGE(WM_WB_SF_OPEN_TEX,     OnWbSfOpenTex)
	ON_MESSAGE(WM_WB_SF_SELECT,       OnWbSfSelect)
	ON_MESSAGE(WM_WB_SF_GET_TEX_LIST, OnWbSfGetTexList)
	// Tier B
	ON_MESSAGE(WM_WB_BRUSH_SET,       OnWbBrushSet)
	ON_MESSAGE(WM_WB_BRUSH_GET_STATE, OnWbBrushGetState)
	ON_MESSAGE(WM_WB_MOUND_SET,       OnWbMoundSet)
	ON_MESSAGE(WM_WB_MOUND_GET_STATE, OnWbMoundGetState)
	ON_MESSAGE(WM_WB_TEX_SET,         OnWbTexSet)
	ON_MESSAGE(WM_WB_TEX_GET_STATE,   OnWbTexGetState)
	ON_MESSAGE(WM_WB_TEX_ACTION,      OnWbTexAction)
	// Tier C
	ON_MESSAGE(WM_WB_FEATHER_SET,     OnWbFeatherSet)
	ON_MESSAGE(WM_WB_FEATHER_GET,     OnWbFeatherGet)
	ON_MESSAGE(WM_WB_SCORCH_SET,      OnWbScorchSet)
	ON_MESSAGE(WM_WB_SCORCH_GET,      OnWbScorchGet)
	ON_MESSAGE(WM_WB_MESHMOLD_SET,    OnWbMeshmoldSet)
	ON_MESSAGE(WM_WB_MESHMOLD_GET,    OnWbMeshmoldGet)
	ON_MESSAGE(WM_WB_MESHMOLD_ACTION, OnWbMeshmoldAction)
	ON_MESSAGE(WM_WB_MESHMOLD_LIST,   OnWbMeshmoldList)
	ON_MESSAGE(WM_WB_FLOODFILL_AT,    OnWbFloodfillAt)
	ON_MESSAGE(WM_WB_WATER_SET,       OnWbWaterSet)
	ON_MESSAGE(WM_WB_WATER_GET,       OnWbWaterGet)
	ON_MESSAGE(WM_WB_RAMP_SET,        OnWbRampSet)
	ON_MESSAGE(WM_WB_RAMP_GET,        OnWbRampGet)
	// Tier D
	ON_MESSAGE(WM_WB_CONTOUR_SET,     OnWbContourSet)
	ON_MESSAGE(WM_WB_CONTOUR_GET,     OnWbContourGet)
	// Tier H
	ON_MESSAGE(WM_WB_GET_MAP_INFO,    OnWbGetMapInfo)
	ON_MESSAGE(WM_WB_GET_HEIGHTMAP,   OnWbGetHeightmap)
	ON_MESSAGE(WM_WB_GET_TEXTUREMAP,  OnWbGetTexturemap)
	ON_MESSAGE(WM_WB_GET_OBJECTS,     OnWbGetObjects)
	ON_MESSAGE(WM_WB_GET_WAYPOINTS,   OnWbGetWaypoints)
	ON_MESSAGE(WM_WB_GET_TRIGGERS,    OnWbGetTriggers)
	ON_MESSAGE(WM_WB_GET_TEAMS,       OnWbGetTeams)
	ON_MESSAGE(WM_WB_GET_SELECTED,    OnWbGetSelected)
	ON_MESSAGE(WM_WB_SF_CREATE_PIPE,   OnWbSfCreatePipe)
	ON_MESSAGE(WM_WB_MAP_HEIGHT_SET,   OnWbMapHeightSet)
	ON_MESSAGE(WM_WB_PLACE_WAYPOINT,   OnWbPlaceWaypoint)
	ON_MESSAGE(WM_WB_LINK_WAYPOINTS,   OnWbLinkWaypoints)
	ON_MESSAGE(WM_WB_PLACE_OBJECT_PIPE,OnWbPlaceObject)
	ON_MESSAGE(WM_WB_PLANT_TREE,       OnWbPlantTree)
	ON_MESSAGE(WM_WB_PLANT_GROVE,      OnWbPlantGrove)
	ON_MESSAGE(WM_WB_GET_VIEW_STATE,   OnWbGetViewState)
	ON_MESSAGE(WM_WB_ROTATE_SELECTED,  OnWbRotateSelected)
	ON_MESSAGE(WM_WB_OBJ_GET_PROPS,    OnWbObjGetProps)
	ON_MESSAGE(WM_WB_OBJ_SET_PROP,     OnWbObjSetProp)
	// Tier J — SidesList wizard
	ON_MESSAGE(WM_WB_GET_SIDELIST,     OnWbGetSideList)
	ON_MESSAGE(WM_WB_SET_PLAYER,       OnWbSetPlayer)
	ON_MESSAGE(WM_WB_SET_TEAM,         OnWbSetTeam)
	ON_MESSAGE(WM_WB_DEL_TEAM,         OnWbDelTeam)
	ON_MESSAGE(WM_WB_SET_SCRIPT,       OnWbSetScript)
	ON_MESSAGE(WM_WB_DEL_SCRIPT,       OnWbDelScript)
	ON_MESSAGE(WM_WB_SET_GROUP,        OnWbSetGroup)
	ON_MESSAGE(WM_WB_ADD_SKIRMISH,     OnWbAddSkirmish)
	ON_MESSAGE(WM_WB_ADD_PLAYER,       OnWbAddPlayer)
	ON_MESSAGE(WM_WB_DEL_PLAYER,       OnWbDelPlayer)
	ON_MESSAGE(WM_WB_DEL_WAYPOINT,     OnWbDelWaypoint)
	ON_MESSAGE(WM_WB_ADD_TRIGGER,      OnWbAddTrigger)
	ON_MESSAGE(WM_WB_DEL_TRIGGER,      OnWbDelTrigger)
	ON_MESSAGE(WM_WB_SET_TRIGGER,      OnWbSetTrigger)
	ON_MESSAGE(WM_WB_SELECT_OBJECT,    OnWbSelectObject)
	ON_MESSAGE(WM_WB_PIPE_CMD,         OnWbPipeCmd)
	ON_MESSAGE(WM_WB_NEW_MAP,          OnWbNewMap)
	ON_MESSAGE(WM_WB_RESIZE_MAP,       OnWbResizeMap)
	ON_MESSAGE(WM_WB_PLACE_ROAD,       OnWbPlaceRoad)
	ON_MESSAGE(WM_WB_LIST_ROADS,       OnWbListRoads)
	ON_MESSAGE(WM_WB_DEL_ROAD,         OnWbDelRoad)
	ON_MESSAGE(WM_WB_SEL_ROAD,         OnWbSelRoad)
	ON_MESSAGE(WM_WB_PLACE_BRIDGE,     OnWbPlaceBridge)
	ON_MESSAGE(WM_WB_LIST_BRIDGES,     OnWbListBridges)
	ON_MESSAGE(WM_WB_SET_ROAD_TOOL,    OnWbSetRoadTool)
	ON_MESSAGE(WM_WB_SET_BRIDGE_NAME,  OnWbSetBridgeName)
	ON_MESSAGE(WM_WB_SAVE_TO_PATH,     OnWbSaveToPath)
	ON_MESSAGE(WM_WB_GET_LIGHTING,     OnWbGetLighting)
	ON_MESSAGE(WM_WB_SET_LIGHTING,     OnWbSetLighting)
	ON_MESSAGE(WM_WB_RESET_LIGHTING,   OnWbResetLighting)
	ON_MESSAGE(WM_WB_IMPASSABLE_VIEW,  OnWbImpassableView)
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

CMainFrame *CMainFrame::TheMainFrame = nullptr;

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	TheMainFrame = this;
	m_curOptions = nullptr;
	m_hAutoSaveTimer = 0;
	m_autoSaving = false;
	m_layersList = nullptr;
	m_scriptDialog = nullptr;
}

CMainFrame::~CMainFrame()
{
	delete m_layersList;
	m_layersList = nullptr;

	delete m_scriptDialog;
	m_scriptDialog = nullptr;

	SaveBarState("MainFrame");
	TheMainFrame = nullptr;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "AutoSave", m_autoSave);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "AutoSaveIntervalSeconds", m_autoSaveInterval);
    CoUninitialize();
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	adjustWindowSize();
	CRect frameRect;
	GetWindowRect(&frameRect);

	CWnd *pDesk = GetDesktopWindow();
	CRect top;
	pDesk->GetWindowRect(&top);
	top.left += 10;
	top.top += 10;
	top.top = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "Top", top.top);
	top.left =::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "Left", top.left);
	SetWindowPos(nullptr, top.left, top.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
	GetWindowRect(&frameRect);

	EnableDocking(CBRS_ALIGN_TOP);

#if 0 // For a floating toolbar.
#define WRAP(btn) m_floatingToolBar.SetButtonStyle( btn, m_floatingToolBar.GetButtonStyle( btn )|TBBS_WRAPPED)
	if (!m_floatingToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_LEFT
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_FIXED ) ||
		!m_floatingToolBar.LoadToolBar(IDR_TOOLBAR2))
		WRAP(1);
	WRAP(4);
	WRAP(6);
	WRAP(9);
	WRAP(11);
	WRAP(14);
	WRAP(16);
#undef WRAP
	CPoint pos(frameRect.left,frameRect.top+60);
	this->FloatControlBar(&m_floatingToolBar, pos, CBRS_ALIGN_LEFT);
	m_floatingToolBar.EnableDocking(CBRS_ALIGN_TOP);
#endif

	if (!m_wndStatusBar.Create(this) || !m_wndStatusBar.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT)))
	{
		DEBUG_CRASH(("Failed to create status bar"));
	}

	if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_FIXED ) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}
 	 m_wndToolBar.EnableDocking(CBRS_ALIGN_TOP);

	frameRect.left = frameRect.right;
	frameRect.top = ::AfxGetApp()->GetProfileInt(OPTIONS_PANEL_SECTION, "Top", frameRect.top);
	frameRect.left =::AfxGetApp()->GetProfileInt(OPTIONS_PANEL_SECTION, "Left", frameRect.left);



	m_brushOptions.Create(IDD_BRUSH_OPTIONS, this);
	m_brushOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top,	0, 0, SWP_NOZORDER|SWP_NOSIZE);
	m_brushOptions.GetWindowRect(&frameRect);
	m_optionsPanelWidth = frameRect.Width();
	m_optionsPanelHeight = frameRect.Height();

	m_featherOptions.Create(IDD_FEATHER_OPTIONS, this);
	m_featherOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top,	0, 0, SWP_NOZORDER|SWP_NOSIZE);
	m_featherOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();


	m_noOptions.Create(IDD_NO_OPTIONS, this);
	m_noOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top,	0, 0, SWP_NOZORDER|SWP_NOSIZE);
	m_noOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_terrainMaterial.Create(IDD_TERRAIN_MATERIAL, this);
	m_terrainMaterial.SetWindowPos(nullptr, frameRect.left, frameRect.top,	0, 0, SWP_NOZORDER|SWP_NOSIZE);
	m_terrainMaterial.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_blendMaterial.Create(IDD_BLEND_MATERIAL, this);
	m_blendMaterial.SetWindowPos(nullptr, frameRect.left, frameRect.top,	0, 0, SWP_NOZORDER|SWP_NOSIZE);
	m_blendMaterial.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_moundOptions.Create(IDD_MOUND_OPTIONS, this);
	m_moundOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top,	0, 0, SWP_NOZORDER|SWP_NOSIZE);
	m_moundOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_rulerOptions.Create(IDD_RULER_OPTIONS, this);
	m_rulerOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top,	0, 0, SWP_NOZORDER|SWP_NOSIZE);
	m_rulerOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_objectOptions.Create(IDD_OBJECT_OPTIONS, this);
	m_objectOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
	m_objectOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_fenceOptions.Create(IDD_FENCE_OPTIONS, this);
	m_fenceOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
	m_fenceOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_mapObjectProps.Create(IDD_MAPOBJECT_PROPS, this);
	m_mapObjectProps.makeMain();
	m_mapObjectProps.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
	m_mapObjectProps.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_roadOptions.Create(IDD_ROAD_OPTIONS, this);
	m_roadOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
 	m_roadOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_waypointOptions.Create(IDD_WAYPOINT_OPTIONS, this);
	m_waypointOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
 	m_waypointOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_waterOptions.Create(IDD_WATER_OPTIONS, this);
	m_waterOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
 	m_waterOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_lightOptions.Create(IDD_LIGHT_OPTIONS, this);
	m_lightOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
 	m_lightOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_meshMoldOptions.Create(IDD_MESHMOLD_OPTIONS, this);
	m_meshMoldOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
 	m_meshMoldOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_buildListOptions.Create(IDD_BUILD_LIST_PANEL, this);
	m_buildListOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
 	m_buildListOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_groveOptions.Create(IDD_GROVE_OPTIONS, this);
	m_groveOptions.makeMain();
	m_groveOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
 	m_groveOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_rampOptions.Create(IDD_RAMP_OPTIONS, this);
	m_rampOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
 	m_rampOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_scorchOptions.Create(IDD_SCORCH_OPTIONS, this);
	m_scorchOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
 	m_scorchOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	m_shapeFillOptions.Create(IDD_SHAPE_FILL_OPTIONS, this);
	m_shapeFillOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
	m_shapeFillOptions.GetWindowRect(&frameRect);
	if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
	if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();

	frameRect.top = ::AfxGetApp()->GetProfileInt(GLOBALLIGHT_OPTIONS_PANEL_SECTION, "Top", frameRect.top);
	frameRect.left =::AfxGetApp()->GetProfileInt(GLOBALLIGHT_OPTIONS_PANEL_SECTION, "Left", frameRect.left);

	m_globalLightOptions.Create(IDD_GLOBAL_LIGHT_OPTIONS, this);
	m_globalLightOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
 	m_globalLightOptions.GetWindowRect(&frameRect);
	m_globalLightOptionsWidth = frameRect.Width();
	m_globalLightOptionsHeight = frameRect.Height();

	frameRect.top = ::AfxGetApp()->GetProfileInt(CAMERA_OPTIONS_PANEL_SECTION, "Top", frameRect.top);
	frameRect.left =::AfxGetApp()->GetProfileInt(CAMERA_OPTIONS_PANEL_SECTION, "Left", frameRect.left);

	m_cameraOptions.Create(IDD_CAMERA_OPTIONS, this);
	m_cameraOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
 	m_cameraOptions.GetWindowRect(&frameRect);

	// now, setup the Layers Panel
	m_layersList = new LayersList(LayersList::IDD, this);
	m_layersList->Create(LayersList::IDD, this);
	m_layersList->ShowWindow(::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowLayersList", 0) ? SW_SHOW : SW_HIDE);

	CRect optionsRect;
	m_globalLightOptions.GetWindowRect(&optionsRect);
	m_layersList->SetWindowPos(nullptr, optionsRect.left, optionsRect.bottom + 100, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

	// TheSuperHackers @feature Nemellud 24/05/2026 EmbeddedMode: hide all chrome — panels exist but are invisible
	if (CWorldBuilderApp::IsEmbedded()) {
		m_wndToolBar.ShowWindow(SW_HIDE);
		m_wndStatusBar.ShowWindow(SW_HIDE);
		if (m_layersList) m_layersList->ShowWindow(SW_HIDE);
		RecalcLayout(); // force view to fill the full client area (no toolbar/statusbar gaps)
		return 0;
	}

	Int sbf = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowBrushFeedback", 1);
	if (sbf != 0) {
		DrawObject::enableFeedback();
	} else {
		DrawObject::disableFeedback();
	}

	Int autoSave = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "AutoSave", 1);
	m_autoSave = autoSave != 0;
	autoSave = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "AutoSaveIntervalSeconds", 120);
	m_autoSaveInterval = autoSave;
	m_hAutoSaveTimer = this->SetTimer(1, m_autoSaveInterval*1000, nullptr);

#if USE_STREAMING_AUDIO
	StartMusic();
#endif

	// TheSuperHackers @feature Nemellud 23/05/2026 DarkTheme: dark title bar (Win10 20H1+)
	BOOL darkMode = TRUE;
	if (FAILED(::DwmSetWindowAttribute(m_hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode)))) {
		::DwmSetWindowAttribute(m_hWnd, 19, &darkMode, sizeof(darkMode));
	}

	return 0;
}

void CMainFrame::adjustWindowSize()
{
	HWND hDesk = ::GetDesktopWindow();
	CRect top;
	::GetWindowRect(hDesk, &top);
	top.right -= 2*::GetSystemMetrics(SM_CYCAPTION);
	top.bottom -= 3*::GetSystemMetrics(SM_CYCAPTION);

	CRect client, window;
	Int borderX = ::GetSystemMetrics(SM_CXEDGE);
//	Int borderY = ::GetSystemMetrics(SM_CYEDGE);
	Int viewWidth = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "Width", THREE_D_VIEW_WIDTH);
	Int viewHeight = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "Height", THREE_D_VIEW_HEIGHT);
	WbView3d * pView = CWorldBuilderDoc::GetActive3DView();
	if (pView) {
		pView->GetClientRect(&client);
	}	else {
		GetClientRect(&client);
		client.right -= 2*borderX;
	}
		int widthDelta = client.Width() - (viewWidth);
		int heightDelta = client.Height() - (viewHeight);
		this->GetWindowRect(window);
		Int newWidth = window.Width()-widthDelta;
		Int newHeight = window.Height()-heightDelta;
	this->SetWindowPos(nullptr, 0,
	0, newWidth, newHeight,
	SWP_NOMOVE|SWP_NOZORDER); // MainFrm.cpp sets the top and left.
	if (pView) {
		pView->reset3dEngineDisplaySize(viewWidth, viewHeight);
	}
	m_3dViewWidth = viewWidth;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	return TRUE;
}

void CMainFrame::ResetWindowPositions()
{
	if (CWorldBuilderApp::IsEmbedded()) return;
	if (m_curOptions == nullptr) {
		m_curOptions = &m_brushOptions;
	}
	SetWindowPos(nullptr, 20, 20, 0, 0, SWP_NOSIZE|SWP_NOZORDER);
	ShowWindow(SW_SHOW);
	m_curOptions->SetWindowPos(nullptr, 40, 40, 0, 0,  SWP_NOSIZE|SWP_NOZORDER);
	m_curOptions->ShowWindow(SW_SHOW);
	CView *pView = CWorldBuilderDoc::GetActive2DView();
	if (pView) {
		CWnd *pParent = pView->GetParentFrame();
		if (pParent) {
			pParent->SetWindowPos(nullptr, 60, 60, 0, 0, SWP_NOSIZE|SWP_NOZORDER);
		}
	}
	CPoint pos(20,200);

	this->FloatControlBar(&m_floatingToolBar, pos, CBRS_ALIGN_LEFT);
	m_floatingToolBar.SetWindowPos(nullptr, pos.x, pos.y, 0, 0, SWP_NOSIZE|SWP_NOZORDER);
	m_floatingToolBar.ShowWindow(SW_SHOW);
}

void CMainFrame::showOptionsDialog(Int dialogID)
{
	// TheSuperHackers @feature Nemellud 24/05/2026 EmbeddedMode: no panels exist in embedded mode
	if (CWorldBuilderApp::IsEmbedded()) return;

	CWnd *newOptions = nullptr;
	switch(dialogID) {
		case IDD_BRUSH_OPTIONS : newOptions = &m_brushOptions; break;
		case IDD_TERRAIN_MATERIAL: newOptions = &m_terrainMaterial; break;
		case IDD_BLEND_MATERIAL: newOptions = &m_blendMaterial; break;
		case IDD_OBJECT_OPTIONS: newOptions = &m_objectOptions; break;
		case IDD_FENCE_OPTIONS: newOptions = &m_fenceOptions; break;
		case IDD_MAPOBJECT_PROPS: newOptions = &m_mapObjectProps; break;
		case IDD_ROAD_OPTIONS:newOptions  = &m_roadOptions; break;
		case IDD_MOUND_OPTIONS:newOptions  = &m_moundOptions; break;
		case IDD_RULER_OPTIONS:newOptions  = &m_rulerOptions; break;
		case IDD_FEATHER_OPTIONS:newOptions  = &m_featherOptions; break;
		case IDD_MESHMOLD_OPTIONS:newOptions  = &m_meshMoldOptions; break;
		case IDD_WAYPOINT_OPTIONS:newOptions  = &m_waypointOptions; break;
		case IDD_WATER_OPTIONS:newOptions  = &m_waterOptions; break;
		case IDD_LIGHT_OPTIONS:newOptions  = &m_lightOptions; break;
		case IDD_BUILD_LIST_PANEL:newOptions  = &m_buildListOptions; break;
		case IDD_GROVE_OPTIONS:newOptions = &m_groveOptions; break;
		case IDD_RAMP_OPTIONS:newOptions = &m_rampOptions; break;
		case IDD_SCORCH_OPTIONS:newOptions = &m_scorchOptions; break;
		case IDD_NO_OPTIONS:newOptions  = &m_noOptions; break;
		case IDD_SHAPE_FILL_OPTIONS:newOptions = &m_shapeFillOptions; break;
		default : break;
	}
	CRect frameRect;
	if (newOptions && newOptions != m_curOptions) {
		newOptions->GetWindowRect(&frameRect);
		if (m_curOptions) {
			m_curOptions->GetWindowRect(&frameRect);
		}
		newOptions->SetWindowPos(m_curOptions, frameRect.left, frameRect.top,
			m_optionsPanelWidth, m_optionsPanelHeight,
			SWP_NOZORDER | SWP_NOACTIVATE );
		::AfxGetApp()->WriteProfileInt(OPTIONS_PANEL_SECTION, "Top", frameRect.top);
		::AfxGetApp()->WriteProfileInt(OPTIONS_PANEL_SECTION, "Left", frameRect.left);
		newOptions->ShowWindow(SW_SHOWNA);
		if (m_curOptions) {
			m_curOptions->ShowWindow(SW_HIDE);
		}
		m_curOptions = newOptions;
	}
}

void CMainFrame::OnEditGloballightoptions()
{
	if (CWorldBuilderApp::IsEmbedded()) return;
	m_globalLightOptions.ShowWindow(SW_SHOWNA);
}

void CMainFrame::onEditScripts()
{
	if (CWorldBuilderApp::IsEmbedded()) return;
	delete m_scriptDialog;

	CRect frameRect;
	GetWindowRect(&frameRect);

	// Setup the Script Dialog.
	// This needs to be recreated each time so that it will have the current data.
	m_scriptDialog = new ScriptDialog(this);
	m_scriptDialog->Create(IDD_ScriptDialog, this);
	m_scriptDialog->SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
 	m_scriptDialog->GetWindowRect(&frameRect);
	m_scriptDialog->ShowWindow(SW_SHOWNA);
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef RTS_DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //RTS_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers


#if DEAD
	void CMainFrame::OnEditContouroptions()
	{
		ContourOptions contourOptsDialog(this);
		contourOptsDialog.DoModal();
	}
#endif

void CMainFrame::OnMove(int x, int y)
{
	CFrameWnd::OnMove(x, y);
	if (this->IsWindowVisible() && !this->IsIconic()) {
		CRect frameRect;
		GetWindowRect(&frameRect);
		::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "Top", frameRect.top);
		::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "Left", frameRect.left);
	}
}

void CMainFrame::OnViewBrushfeedback()
{
	if (DrawObject::isFeedbackEnabled()) {
		DrawObject::disableFeedback();
		::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowBrushFeedback", 0);
	} else {
		DrawObject::enableFeedback();
		::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowBrushFeedback", 1);
	}
}

void CMainFrame::OnUpdateViewBrushfeedback(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(DrawObject::isFeedbackEnabled()?1:0);
}

void CMainFrame::OnDestroy()
{
	if (m_hAutoSaveTimer) {
		KillTimer(m_hAutoSaveTimer);
	}
	m_hAutoSaveTimer = 0;
	CFrameWnd::OnDestroy();
}

void CMainFrame::OnTimer(UINT nIDEvent)
{
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc && pDoc->needAutoSave()) {
		m_autoSaving = true;
		HCURSOR old = SetCursor(::LoadCursor(nullptr, IDC_WAIT));
		SetMessageText("Auto Saving map...");
		pDoc->autoSave();
		if (old) SetCursor(old);
		SetMessageText("Auto Save Complete.");
		m_autoSaving = false;
	}
}

void CMainFrame::OnEditCameraoptions()
{
	m_cameraOptions.ShowWindow(SW_SHOWNA);
}

void CMainFrame::handleCameraChange()
{
	m_cameraOptions.update();
}

// TheSuperHackers @feature Nemellud 24/05/2026 EmbeddedMode: set ShapeFill sub-mode from pipe command
LRESULT CMainFrame::OnWbSfMode(WPARAM wParam, LPARAM /*lParam*/)
{
	ShapeFillTool::setMode(static_cast<SFToolMode>(wParam));
	ShapeFillOptions::updateFromTool();
	WbPipeServer::setSfToolActive(true);
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: set 3D view projection from pipe command
LRESULT CMainFrame::OnWbSetProjection(WPARAM wParam, LPARAM /*lParam*/)
{
	bool topDown = (wParam != 0);
	// GetActive3DView() relies on MDI focus state which breaks in embedded mode;
	// use s_instance as a direct fallback.
	WbView3d* p3D = CWorldBuilderDoc::GetActive3DView();
	if (!p3D) p3D = WbView3d::s_instance;
	if (p3D) {
		p3D->setTopDownProjection(topDown);
		ShapeFillOptions::updateTopDownState();
		WbPipeServer::setViewTopDown(topDown);
	}
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: serialize ShapeFill state to JSON for pipe client
LRESULT CMainFrame::OnWbSfGetState(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 32) return 0;

	int  innerH  = ShapeFillTool::getInnerHeight();
	int  bw      = ShapeFillTool::getBorderWidth();
	int  innerTc = ShapeFillTool::getInnerTexClass();
	int  borderTc= ShapeFillTool::getBorderTexClass();
	bool autoBlend       = ShapeFillTool::getAutoBlend();
	bool blendIn         = ShapeFillTool::getBlendInward();
	bool fillAutoBlend   = ShapeFillTool::getFillAutoBlend();
	bool fillBlendIn     = ShapeFillTool::getFillBlendInward();
	bool innerAutoBlend  = ShapeFillTool::getInnerAutoBlend();
	bool innerBlendIn    = ShapeFillTool::getInnerBlendInward();
	bool autoSave        = ShapeFillTool::getAutoSave();
	int  selId   = ShapeFillTool::getSelectedId();

	// Encode current mode
	SFToolMode curMode = ShapeFillTool::getMode();
	const char* modeStr = "select";
	if      (curMode == SF_DRAW_RECT)    modeStr = "rect";
	else if (curMode == SF_DRAW_CIRCLE)  modeStr = "circle";
	else if (curMode == SF_DRAW_POLYGON) modeStr = "polygon";
	else if (curMode == SF_DRAW_LINE)    modeStr = "line";
	else if (curMode == SF_EDIT_SHAPE)   modeStr = "edit";
	else if (curMode == SF_BUCKET_FILL)  modeStr = "fill";

	bool isDrawingPoly = (curMode == SF_DRAW_POLYGON) && ShapeFillTool::getPolyDraftSize() >= 3;
	bool isDrawingLine = (curMode == SF_DRAW_LINE);

	char innerTexBuf[64]  = "(none)";
	char borderTexBuf[64] = "(same as inner)";
	if (innerTc  >= 0) strncpy(innerTexBuf,  WorldHeightMapEdit::getTexClassName(innerTc).str(),  63);
	if (borderTc >= 0) strncpy(borderTexBuf, WorldHeightMapEdit::getTexClassName(borderTc).str(), 63);

	// Build shapes array JSON
	char shapesJson[2048] = "[";
	bool first = true;
	for (const auto& s : ShapeFillTool::getShapes()) {
		const char* typeName = (s.type == SHAPE_RECT) ? "rect" : (s.type == SHAPE_CIRCLE) ? "circle" : "polygon";
		const char* typeLabel= (s.type == SHAPE_RECT) ? "Rect" : (s.type == SHAPE_CIRCLE) ? "Circle" : "Polygon";
		char entry[128];
		_snprintf(entry, sizeof(entry), "%s{\"id\":%d,\"type\":\"%s\",\"name\":\"%s #%d\"}",
		          first ? "" : ",", s.id, typeName, typeLabel, s.id);
		strncat(shapesJson, entry, sizeof(shapesJson) - strlen(shapesJson) - 2);
		first = false;
	}
	strncat(shapesJson, "]", sizeof(shapesJson) - strlen(shapesJson) - 1);

	// TheSuperHackers @feature Nemellud 12/06/2026 ShapeFill: lijnen in de state voor de UI-lijst
	char linesJson[1024] = "[";
	bool firstL = true;
	for (const auto& l : ShapeFillTool::getLines()) {
		char entry[96];
		_snprintf(entry, sizeof(entry), "%s{\"id\":%d,\"points\":%d}",
		          firstL ? "" : ",", l.id, (int)l.points.size());
		strncat(linesJson, entry, sizeof(linesJson) - strlen(linesJson) - 2);
		firstL = false;
	}
	strncat(linesJson, "]", sizeof(linesJson) - strlen(linesJson) - 1);
	int selLineId = ShapeFillTool::getSelectedLineId();

	_snprintf(buf, len,
		"{\"ok\":true,"
		"\"innerHeight\":%d,\"borderWidth\":%d,"
		"\"innerTexClass\":%d,\"innerTexName\":\"%s\","
		"\"borderTexClass\":%d,\"borderTexName\":\"%s\","
		"\"autoBlend\":%s,\"blendInward\":%s,"
		"\"fillAutoBlend\":%s,\"fillBlendInward\":%s,"
		"\"innerAutoBlend\":%s,\"innerBlendInward\":%s,"
		"\"autoSave\":%s,"
		"\"mode\":\"%s\",\"isDrawingPoly\":%s,\"isDrawingLine\":%s,"
		"\"selectedId\":%d,\"shapes\":%s,"
		"\"selectedLineId\":%d,\"lines\":%s}",
		innerH, bw,
		innerTc, innerTexBuf,
		borderTc, borderTexBuf,
		autoBlend    ? "true" : "false",
		blendIn      ? "true" : "false",
		fillAutoBlend  ? "true" : "false",
		fillBlendIn    ? "true" : "false",
		innerAutoBlend ? "true" : "false",
		innerBlendIn   ? "true" : "false",
		autoSave       ? "true" : "false",
		modeStr,
		isDrawingPoly  ? "true" : "false",
		isDrawingLine  ? "true" : "false",
		selId, shapesJson,
		selLineId, linesJson);
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: set single ShapeFill int property from pipe
LRESULT CMainFrame::OnWbSfSetInt(WPARAM wParam, LPARAM lParam)
{
	int val = (int)lParam;
	switch ((int)wParam) {
		case SF_PROP_INNER_HEIGHT:       ShapeFillTool::setInnerHeight(val); break;
		case SF_PROP_BORDER_WIDTH:       ShapeFillTool::setBorderWidth(val); break;
		case SF_PROP_AUTO_BLEND:         ShapeFillTool::setAutoBlend(val != 0); break;
		case SF_PROP_BLEND_INWARD:       ShapeFillTool::setBlendInward(val != 0); break;
		case SF_PROP_INNER_TEX:          ShapeFillTool::setInnerTexClass(val); break;
		case SF_PROP_BORDER_TEX:         ShapeFillTool::setBorderTexClass(val); break;
		case SF_PROP_FILL_AUTO_BLEND:    ShapeFillTool::setFillAutoBlend(val != 0); break;
		case SF_PROP_FILL_BLEND_INWARD:  ShapeFillTool::setFillBlendInward(val != 0); break;
		case SF_PROP_INNER_AUTO_BLEND:   ShapeFillTool::setInnerAutoBlend(val != 0); break;
		case SF_PROP_INNER_BLEND_INWARD: ShapeFillTool::setInnerBlendInward(val != 0); break;
		case SF_PROP_AUTO_SAVE:          ShapeFillTool::setAutoSave(val != 0); break;
	}
	ShapeFillOptions::updateFromTool();
	ShapeFillTool::syncSelectedFromPanel();
	CWorldBuilderView* p2D = CWorldBuilderDoc::GetActive2DView();
	WbView3d*          p3D = CWorldBuilderDoc::GetActive3DView();
	if (p2D) p2D->Invalidate(false);
	if (p3D) p3D->Invalidate(false);
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: execute ShapeFill action from pipe
LRESULT CMainFrame::OnWbSfAction(WPARAM wParam, LPARAM /*lParam*/)
{
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	switch ((int)wParam) {
		case SF_ACT_APPLY:
			if (pDoc) ShapeFillTool::applySelectedShape(pDoc);
			break;
		case SF_ACT_DELETE:
			ShapeFillTool::deleteSelectedShape();
			ShapeFillOptions::updateFromTool();
			break;
		case SF_ACT_DUPLICATE:
			ShapeFillTool::copySelectedShape();
			ShapeFillTool::pasteShape();
			ShapeFillOptions::updateFromTool();
			break;
		case SF_ACT_COPY:
			ShapeFillTool::copySelectedShape();
			break;
		case SF_ACT_PASTE:
			ShapeFillTool::pasteShape();
			ShapeFillOptions::updateFromTool();
			break;
		case SF_ACT_FLIP_H:
			ShapeFillTool::flipSelectedShape(true);
			break;
		case SF_ACT_FLIP_V:
			ShapeFillTool::flipSelectedShape(false);
			break;
		case SF_ACT_FINISH_POLY:
			ShapeFillTool::finishPolygon();
			ShapeFillOptions::updateFromTool();
			break;
		case SF_ACT_FINISH_LINE:
			ShapeFillTool::commitCurrentLine();
			ShapeFillOptions::updateFromTool();
			break;
		case SF_ACT_CLEAR_LINES:
			ShapeFillTool::clearLines();
			ShapeFillOptions::updateFromTool();
			break;
		case SF_ACT_ROTATE:
			ShapeFillTool::rotateSelectedShape();
			ShapeFillOptions::updateFromTool();
			break;
	}
	Invalidate();
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: open native texture picker from pipe
LRESULT CMainFrame::OnWbSfOpenTex(WPARAM wParam, LPARAM /*lParam*/)
{
	ShapeFillOptions::openTexPicker(wParam == 0);
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: return all terrain texture classes to pipe client
LRESULT CMainFrame::OnWbSfGetTexList(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 32) return 0;

	int n = WorldHeightMapEdit::getNumTexClasses();
	char arr[32768] = "[";
	bool first = true;
	for (int i = 0; i < n && strlen(arr) < sizeof(arr) - 256; i++) {
		const char* name   = WorldHeightMapEdit::getTexClassName(i).str();
		const char* uiName = WorldHeightMapEdit::getTexClassUiName(i).str();
		char entry[256];
		_snprintf(entry, sizeof(entry), "%s{\"class\":%d,\"name\":\"%s\",\"uiName\":\"%s\"}",
		          first ? "" : ",", i, name ? name : "", uiName ? uiName : "");
		strncat(arr, entry, sizeof(arr) - strlen(arr) - 2);
		first = false;
	}
	strncat(arr, "]", sizeof(arr) - strlen(arr) - 1);
	_snprintf(buf, len, "{\"ok\":true,\"textures\":%s}", arr);
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: select shape by id from pipe
LRESULT CMainFrame::OnWbSfSelect(WPARAM wParam, LPARAM lParam)
{
	int id = (int)wParam;
	if (lParam == 1) {                       // lijn selecteren
		ShapeFillTool::setSelectedId(-1);
		ShapeFillTool::setSelectedLineId(id);
	} else {                                 // shape selecteren
		ShapeFillTool::setSelectedId(id);
		ShapeFillTool::setSelectedLineId(-1);
	}
	ShapeFillTool::setMode(SF_SELECT);
	ShapeFillOptions::updateFromTool();
	CWorldBuilderView* p2D = CWorldBuilderDoc::GetActive2DView();
	WbView3d*          p3D = CWorldBuilderDoc::GetActive3DView();
	if (p2D) p2D->Invalidate(false);
	if (p3D) p3D->Invalidate(false);
	return 0;
}

// TheSuperHackers @feature Nemellud 23/05/2026 DarkTheme: dark background for main frame chrome
BOOL CMainFrame::OnEraseBkgnd(CDC* pDC)
{
	CRect rect;
	GetClientRect(&rect);
	pDC->FillSolidRect(&rect, RGB(30, 30, 30));
	return TRUE;
}

// ── Tier B — BrushTool ────────────────────────────────────────────────────────

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: set brush tool property from pipe
LRESULT CMainFrame::OnWbBrushSet(WPARAM wParam, LPARAM lParam)
{
	int val = (int)lParam;
	switch ((int)wParam) {
		case BRUSH_PROP_WIDTH:   BrushOptions::setWidth(val);   BrushTool::setWidth(val);   break;
		case BRUSH_PROP_FEATHER: BrushOptions::setFeather(val); BrushTool::setFeather(val); break;
		case BRUSH_PROP_HEIGHT:  BrushOptions::setHeight(val);  BrushTool::setHeight(val);  break;
		case BRUSH_PROP_SHAPE:   BrushTool::setShape(val != 0); break;
	}
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: read brush tool state for pipe
LRESULT CMainFrame::OnWbBrushGetState(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 32) return 0;
	_snprintf(buf, len,
		"{\"ok\":true,\"width\":%d,\"feather\":%d,\"height\":%d,\"shape\":%d}",
		BrushTool::getWidth(), BrushTool::getFeather(), BrushTool::getHeight(),
		BrushTool::getShape() ? 1 : 0);
	return 0;
}

// ── Tier B — MoundTool ────────────────────────────────────────────────────────

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: set mound tool property from pipe
LRESULT CMainFrame::OnWbMoundSet(WPARAM wParam, LPARAM lParam)
{
	int val = (int)lParam;
	switch ((int)wParam) {
		case MOUND_PROP_WIDTH:   MoundOptions::setWidth(val);        break;
		case MOUND_PROP_FEATHER: MoundOptions::setFeather(val);      break;
		case MOUND_PROP_AMOUNT:  MoundOptions::setHeight(val);       break;
		case MOUND_PROP_SHAPE:   MoundTool::setShape(val != 0);      break;
	}
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: read mound tool state for pipe
LRESULT CMainFrame::OnWbMoundGetState(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 32) return 0;
	// MoundOptions shares static statics; width/feather use BrushTool equivalents
	_snprintf(buf, len,
		"{\"ok\":true,\"width\":%d,\"feather\":%d,\"amount\":%d}",
		BrushTool::getWidth(), BrushTool::getFeather(), BrushTool::getHeight());
	return 0;
}

// ── Tier B — TerrainMaterial (texture painter) ───────────────────────────────

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: set texture painter property from pipe
LRESULT CMainFrame::OnWbTexSet(WPARAM wParam, LPARAM lParam)
{
	int val = (int)lParam;
	switch ((int)wParam) {
		case TEX_PROP_FG_CLASS: TerrainMaterial::setFgTexClass(val); break;
		case TEX_PROP_BG_CLASS: TerrainMaterial::setBgTexClass(val); break;
		case TEX_PROP_WIDTH:    TerrainMaterial::setWidth(val); BigTileTool::setWidth(val); break;
		case TEX_PROP_MODE:     TerrainMaterial::setPaintingMode(val != 0, TerrainMaterial::isPaintingPassable() != FALSE); break;
		case TEX_PROP_PASSABLE: TerrainMaterial::setPaintingMode(TerrainMaterial::isPaintingPathingInfo() != FALSE, val != 0); break;
	}
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: read texture painter state for pipe
LRESULT CMainFrame::OnWbTexGetState(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 64) return 0;
	_snprintf(buf, len,
		"{\"ok\":true,\"fgClass\":%d,\"bgClass\":%d,\"width\":%d,\"pathing\":%s,\"passable\":%s}",
		TerrainMaterial::getFgTexClass(), TerrainMaterial::getBgTexClass(),
		TerrainMaterial::getWidth(),
		TerrainMaterial::isPaintingPathingInfo() ? "true" : "false",
		TerrainMaterial::isPaintingPassable()    ? "true" : "false");
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: swap FG/BG textures from pipe
LRESULT CMainFrame::OnWbTexAction(WPARAM wParam, LPARAM /*lParam*/)
{
	if ((int)wParam == TEX_ACT_SWAP) {
		Int fg = TerrainMaterial::getFgTexClass();
		Int bg = TerrainMaterial::getBgTexClass();
		TerrainMaterial::setFgTexClass(bg);
		TerrainMaterial::setBgTexClass(fg);
	}
	return 0;
}

// ── Tier C — FeatherTool ─────────────────────────────────────────────────────

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: set feather tool property from pipe
LRESULT CMainFrame::OnWbFeatherSet(WPARAM wParam, LPARAM lParam)
{
	int val = (int)lParam;
	switch ((int)wParam) {
		case FEATHER_PROP_AMOUNT: FeatherOptions::setFeather(val); break;
		case FEATHER_PROP_RADIUS: FeatherOptions::setRadius(val);  break;
		case FEATHER_PROP_RATE:   FeatherOptions::setRate(val);    break;
	}
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: read feather tool state for pipe
LRESULT CMainFrame::OnWbFeatherGet(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 64) return 0;
	_snprintf(buf, len,
		"{\"ok\":true,\"amount\":%d,\"radius\":%d,\"rate\":%d}",
		FeatherOptions::getFeather(), FeatherOptions::getRadius(), FeatherOptions::getRate());
	return 0;
}

// ── Tier C — ScorchTool ──────────────────────────────────────────────────────

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: set scorch tool property from pipe
LRESULT CMainFrame::OnWbScorchSet(WPARAM wParam, LPARAM lParam)
{
	switch ((int)wParam) {
		case SCORCH_PROP_TYPE: ScorchOptions::setScorchType((int)lParam); break;
		case SCORCH_PROP_SIZE: ScorchOptions::setScorchSize((float)((int)lParam) / 100.0f); break;
	}
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: read scorch tool state for pipe
LRESULT CMainFrame::OnWbScorchGet(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 64) return 0;
	_snprintf(buf, len,
		"{\"ok\":true,\"type\":%d,\"size\":%d}",
		(int)ScorchOptions::getScorchType(), (int)(ScorchOptions::getScorchSize() * 100.0f));
	return 0;
}

// ── Tier C — MeshMoldTool ────────────────────────────────────────────────────

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: set meshmold tool property from pipe
LRESULT CMainFrame::OnWbMeshmoldSet(WPARAM wParam, LPARAM lParam)
{
	switch ((int)wParam) {
		case MESHMOLD_PROP_SCALE:     MeshMoldOptions::setScale((float)((int)lParam) / 100.0f);  break;
		case MESHMOLD_PROP_HEIGHT:    MeshMoldOptions::setHeight((float)((int)lParam) / 100.0f); break;
		case MESHMOLD_PROP_ANGLE:     MeshMoldOptions::setAngle((int)lParam);   break;
		case MESHMOLD_PROP_RAISEONLY: MeshMoldOptions::setRaiseOnly(lParam != 0); break;
		case MESHMOLD_PROP_LOWERONLY: MeshMoldOptions::setLowerOnly(lParam != 0); break;
		// TheSuperHackers @feature Nemellud 04/07/2026 EmbeddedMode: model + world pos from pipe
		case MESHMOLD_PROP_MODEL: {
			char* name = (char*)lParam;
			if (name) { MeshMoldOptions::selectMold(name); delete[] name; }
			break;
		}
		case MESHMOLD_PROP_POS_X:   MeshMoldTool::setToolPosX((float)((int)lParam) / 100.0f); break;
		case MESHMOLD_PROP_POS_Y:
			MeshMoldTool::setToolPosY((float)((int)lParam) / 100.0f);
			// TheSuperHackers @feature Nemellud 04/07/2026 EmbeddedMode: enable preview and update position when set via pipe
			DrawObject::setDoMeshFeedback(true);
			MeshMoldTool::updateMeshLocation(false);
			break;
		case MESHMOLD_PROP_POS_Z:   MeshMoldTool::setToolPosZ((float)((int)lParam) / 100.0f); break;
		case MESHMOLD_PROP_SCALE_X: MeshMoldOptions::setScaleX((float)((int)lParam) / 100.0f); break;
		case MESHMOLD_PROP_SCALE_Y: MeshMoldOptions::setScaleY((float)((int)lParam) / 100.0f); break;
	}
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: read meshmold state for pipe
LRESULT CMainFrame::OnWbMeshmoldGet(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 128) return 0;
	char modelBuf[64] = "";
	strncpy(modelBuf, MeshMoldOptions::getModelName().str(), sizeof(modelBuf) - 1);
	_snprintf(buf, len,
		"{\"ok\":true,\"scale\":%d,\"scaleX\":%d,\"scaleY\":%d,\"height\":%d,\"angle\":%d,\"raiseOnly\":%s,\"lowerOnly\":%s,\"model\":\"%s\"}",
		(int)(MeshMoldOptions::getScale()  * 100.0f),
		(int)(MeshMoldOptions::getScaleX() * 100.0f),
		(int)(MeshMoldOptions::getScaleY() * 100.0f),
		(int)(MeshMoldOptions::getHeight() * 100.0f),
		MeshMoldOptions::getAngle(),
		MeshMoldOptions::isRaisingOnly() ? "true" : "false",
		MeshMoldOptions::isLoweringOnly() ? "true" : "false",
		modelBuf);
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: apply meshmold from pipe
// TheSuperHackers @feature Nemellud 04/07/2026 EmbeddedMode: call MeshMoldTool::apply directly so pipe works without panel open
LRESULT CMainFrame::OnWbMeshmoldAction(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc) MeshMoldTool::apply(pDoc);
	return 0;
}

// TheSuperHackers @feature Nemellud 04/07/2026 EmbeddedMode: list available .w3d molds via pipe
LRESULT CMainFrame::OnWbMeshmoldList(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   maxLen = (int)wParam;
	if (!buf || maxLen < 4) return 0;

	FilenameList filenameList;
	TheFileSystem->getFileListInDirectory(".\\data\\Editor\\Molds\\", "*.w3d", filenameList, FALSE);

	int pos = 0;
	pos += _snprintf(buf + pos, maxLen - pos, "{\"ok\":true,\"molds\":[");
	bool first = true;
	for (FilenameList::iterator it = filenameList.begin(); it != filenameList.end(); ++it) {
		AsciiString filename = *it;
		char tmp[_MAX_PATH];
		strncpy(tmp, filename.str(), sizeof(tmp) - 1);
		tmp[sizeof(tmp) - 1] = '\0';
		// strip path prefix, keep only base name
		char* nameStart = tmp;
		for (int i = 0; tmp[i]; i++)
			if (tmp[i] == '\\' || tmp[i] == '/') nameStart = tmp + i + 1;
		// strip .w3d extension
		for (int i = (int)strlen(nameStart) - 1; i > 0; i--) {
			if (nameStart[i] == '.') { nameStart[i] = '\0'; break; }
		}
		pos += _snprintf(buf + pos, maxLen - pos, "%s\"%s\"", first ? "" : ",", nameStart);
		first = false;
	}
	pos += _snprintf(buf + pos, maxLen - pos, "]}");
	return 0;
}

// TheSuperHackers @feature Nemellud 04/07/2026 EmbeddedMode: flood fill texture at world position via pipe
LRESULT CMainFrame::OnWbFloodfillAt(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) return 0;

	Coord3D cpt = {g_wbFloodfillAtReq.wx, g_wbFloodfillAtReq.wy, 0};
	CPoint ndx;
	if (!pDoc->getCellIndexFromCoord(cpt, &ndx)) return 0;

	int texClass = g_wbFloodfillAtReq.texClass;
	if (texClass < 0) texClass = TerrainMaterial::getFgTexClass();
	Bool shiftKey = (g_wbFloodfillAtReq.exact != 0);

	WorldHeightMapEdit *htMapEditCopy = pDoc->GetHeightMap()->duplicate();
	Bool didIt = htMapEditCopy->floodFill(ndx.x, ndx.y, texClass, shiftKey);
	if (didIt) {
		htMapEditCopy->optimizeTiles();
		IRegion2D partialRange = {0, 0, 0, 0};
		pDoc->updateHeightMap(htMapEditCopy, false, partialRange);
		WBDocUndoable *pUndo = new WBDocUndoable(pDoc, htMapEditCopy);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo);
	}
	REF_PTR_RELEASE(htMapEditCopy);
	return didIt ? 1 : 0;
}

// ── Tier C — WaterTool ───────────────────────────────────────────────────────

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: set water tool property from pipe
LRESULT CMainFrame::OnWbWaterSet(WPARAM wParam, LPARAM lParam)
{
	int val = (int)lParam;
	switch ((int)wParam) {
		case WATER_PROP_HEIGHT:  WaterOptions::setHeight(val);  break;
		case WATER_PROP_SPACING: WaterOptions::setSpacing(val); break;
	}
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: read water tool state for pipe
LRESULT CMainFrame::OnWbWaterGet(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 64) return 0;
	_snprintf(buf, len,
		"{\"ok\":true,\"height\":%d,\"spacing\":%d}",
		WaterOptions::getHeight(), WaterOptions::getSpacing());
	return 0;
}

// ── Tier C — RampTool ────────────────────────────────────────────────────────

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: set ramp tool property from pipe
LRESULT CMainFrame::OnWbRampSet(WPARAM wParam, LPARAM lParam)
{
	if ((int)wParam == RAMP_PROP_WIDTH && TheRampOptions)
		TheRampOptions->setRampWidth((float)((int)lParam) / 100.0f);
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: read ramp tool state for pipe
LRESULT CMainFrame::OnWbRampGet(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 64) return 0;
	float w = TheRampOptions ? TheRampOptions->getRampWidth() : 0.0f;
	_snprintf(buf, len, "{\"ok\":true,\"width\":%d}", (int)(w * 100.0f));
	return 0;
}

// ── Tier D — Contour ─────────────────────────────────────────────────────────

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: set contour option from pipe
LRESULT CMainFrame::OnWbContourSet(WPARAM wParam, LPARAM lParam)
{
	int val = (int)lParam;
	switch ((int)wParam) {
		case CONTOUR_PROP_STEP:   ContourOptions::setContourStep(val);   break;
		case CONTOUR_PROP_OFFSET: ContourOptions::setContourOffset(val); break;
		case CONTOUR_PROP_WIDTH:  ContourOptions::setContourWidth(val);  break;
	}
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: read contour options for pipe
LRESULT CMainFrame::OnWbContourGet(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 64) return 0;
	_snprintf(buf, len,
		"{\"ok\":true,\"step\":%d,\"offset\":%d,\"width\":%d}",
		ContourOptions::getContourStep(), ContourOptions::getContourOffset(), ContourOptions::getContourWidth());
	return 0;
}

// ── Tier H — Map data read-back ───────────────────────────────────────────────

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: return basic map info for pipe
LRESULT CMainFrame::OnWbGetMapInfo(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 64) return 0;

	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc || !pDoc->GetHeightMap()) {
		_snprintf(buf, len, "{\"ok\":false,\"error\":\"no map loaded\"}");
		return 0;
	}
	WorldHeightMapEdit* pMap = pDoc->GetHeightMap();
	int w = pMap->getXExtent();
	int h = pMap->getYExtent();
	int border = pMap->getBorderSize();
	bool dirty = (pDoc->IsModified() != FALSE);
	char path[MAX_PATH] = "";
	strncpy(path, (const char*)pDoc->GetPathName(), sizeof(path) - 1);
	// Escape backslashes for JSON
	char pathEsc[MAX_PATH * 2] = "";
	int pi = 0;
	for (int i = 0; path[i] && pi < (int)sizeof(pathEsc) - 2; i++) {
		if (path[i] == '\\') pathEsc[pi++] = '\\';
		pathEsc[pi++] = path[i];
	}
	_snprintf(buf, len,
		"{\"ok\":true,\"width\":%d,\"height\":%d,\"borderSize\":%d,\"cellSize\":10,\"isDirty\":%s,\"filePath\":\"%s\"}",
		w, h, border, dirty ? "true" : "false", pathEsc);
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: return full heightmap as JSON array for pipe
LRESULT CMainFrame::OnWbGetHeightmap(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   maxLen = (int)wParam;
	if (!buf || maxLen < 64) return 0;

	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc || !pDoc->GetHeightMap()) {
		_snprintf(buf, maxLen, "{\"ok\":false,\"error\":\"no map loaded\"}");
		return 0;
	}
	WorldHeightMapEdit* pMap = pDoc->GetHeightMap();
	int w = pMap->getXExtent();
	int h = pMap->getYExtent();

	int pos = _snprintf(buf, maxLen, "{\"ok\":true,\"width\":%d,\"height\":%d,\"data\":[", w, h);
	bool first = true;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			if (pos >= maxLen - 16) { pos += _snprintf(buf + pos, maxLen - pos, "]}"); return 0; }
			pos += _snprintf(buf + pos, maxLen - pos, first ? "%d" : ",%d", (int)pMap->getHeight(x, y));
			first = false;
		}
	}
	if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; buf[pos] = '\0'; }
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: return texture class map as JSON array for pipe
LRESULT CMainFrame::OnWbGetTexturemap(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   maxLen = (int)wParam;
	if (!buf || maxLen < 64) return 0;

	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc || !pDoc->GetHeightMap()) {
		_snprintf(buf, maxLen, "{\"ok\":false,\"error\":\"no map loaded\"}");
		return 0;
	}
	WorldHeightMapEdit* pMap = pDoc->GetHeightMap();
	int w = pMap->getXExtent();
	int h = pMap->getYExtent();

	int pos = _snprintf(buf, maxLen, "{\"ok\":true,\"width\":%d,\"height\":%d,\"data\":[", w, h);
	bool first = true;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			if (pos >= maxLen - 16) { pos += _snprintf(buf + pos, maxLen - pos, "]}"); return 0; }
			pos += _snprintf(buf + pos, maxLen - pos, first ? "%d" : ",%d", (int)pMap->getTextureClass(x, y));
			first = false;
		}
	}
	if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; buf[pos] = '\0'; }
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: return all map objects as JSON for pipe
LRESULT CMainFrame::OnWbGetObjects(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   maxLen = (int)wParam;
	if (!buf || maxLen < 64) return 0;

	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc) {
		_snprintf(buf, maxLen, "{\"ok\":false,\"error\":\"no map loaded\"}");
		return 0;
	}

	int pos = _snprintf(buf, maxLen, "{\"ok\":true,\"objects\":[");
	bool first = true;

	for (MapObject* pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
		if (pObj->isWaypoint()) continue;
		if (pos >= maxLen - 256) break;
		const Coord3D* loc = pObj->getLocation();
		const Dict* d = pObj->getProperties();
		Int flags = pObj->getFlags();
		bool isBridgePt1 = !!(flags & FLAG_BRIDGE_POINT1);
		bool isBridgePt2 = !!(flags & FLAG_BRIDGE_POINT2);
		if (isBridgePt2) continue; // skip POINT2 — POINT1 already represents the bridge pair
		char objName[128] = "";
		if (isBridgePt1 && d) {
			Bool sne = FALSE;
			AsciiString sn = d->getAsciiString(TheKey_objectName, &sne);
			if (sne && sn.getLength() > 0) strncpy(objName, sn.str(), sizeof(objName) - 1);
		}
		// Bridge: use objectName (the script-referenceable Name) if set, else template type name
		const char* name = (isBridgePt1 && objName[0]) ? objName : pObj->getName().str();
		// Get owner from dict
		char owner[64] = "";
		if (d) {
			Bool exists = FALSE;
			AsciiString ownerStr = d->getAsciiString(TheKey_originalOwner, &exists);
			if (exists) strncpy(owner, ownerStr.str(), sizeof(owner) - 1);
		}
		pos += _snprintf(buf + pos, maxLen - pos,
			"%s{\"name\":\"%s\",\"wx\":%.1f,\"wy\":%.1f,\"angle\":%.1f,\"team\":\"%s\",\"selected\":%s,\"flags\":%d}",
			first ? "" : ",",
			name,
			loc ? loc->x : 0.0f, loc ? loc->y : 0.0f,
			pObj->getAngle() * (180.0f / 3.14159265f),
			owner,
			pObj->isSelected() ? "true" : "false",
			flags);
		first = false;
	}
	if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; buf[pos] = '\0'; }
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: return all waypoints as JSON for pipe
LRESULT CMainFrame::OnWbGetWaypoints(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   maxLen = (int)wParam;
	if (!buf || maxLen < 64) return 0;

	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc) {
		_snprintf(buf, maxLen, "{\"ok\":false,\"error\":\"no map loaded\"}");
		return 0;
	}

	int pos = _snprintf(buf, maxLen, "{\"ok\":true,\"waypoints\":[");
	bool first = true;

	for (MapObject* pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
		if (!pObj->isWaypoint()) continue;
		if (pos >= maxLen - 512) break;
		const Coord3D* loc = pObj->getLocation();
		const char* name = pObj->getWaypointName().str();
		// Build paths array from path labels — no lambda, no Dict access on missing keys
		char pathsBuf[256];
		int ppos = _snprintf(pathsBuf, sizeof(pathsBuf), "[");
		Dict* props = pObj->getProperties();
		bool biDir = false;
		if (props) {
			Bool exists = FALSE;
			const char* labels[3] = {
				TheKey_waypointPathLabel1 ? props->getAsciiString(TheKey_waypointPathLabel1, &exists).str() : "",
				TheKey_waypointPathLabel2 ? props->getAsciiString(TheKey_waypointPathLabel2, &exists).str() : "",
				TheKey_waypointPathLabel3 ? props->getAsciiString(TheKey_waypointPathLabel3, &exists).str() : ""
			};
			bool firstLabel = true;
			for (int li = 0; li < 3; li++) {
				if (!labels[li] || !labels[li][0]) continue;
				ppos += _snprintf(pathsBuf + ppos, sizeof(pathsBuf) - ppos,
					"%s\"%s\"", firstLabel ? "" : ",", labels[li]);
				firstLabel = false;
			}
			biDir = props->getBool(TheKey_waypointPathBiDirectional, &exists) != FALSE;
		}
		if (ppos < (int)sizeof(pathsBuf) - 1) pathsBuf[ppos++] = ']';
		pathsBuf[ppos] = '\0';
		pos += _snprintf(buf + pos, maxLen - pos,
			"%s{\"name\":\"%s\",\"wx\":%.1f,\"wy\":%.1f,\"paths\":%s,\"biDir\":%s}",
			first ? "" : ",", name,
			loc ? loc->x : 0.0f, loc ? loc->y : 0.0f,
			pathsBuf, biDir ? "true" : "false");
		first = false;
	}
	if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; buf[pos] = '\0'; }
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: return all polygon triggers as JSON for pipe
LRESULT CMainFrame::OnWbGetTriggers(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   maxLen = (int)wParam;
	if (!buf || maxLen < 64) return 0;

	int pos = _snprintf(buf, maxLen, "{\"ok\":true,\"triggers\":[");
	bool first = true;

	for (PolygonTrigger* pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
		if (pos >= maxLen - 512) break;
		const char* name = pTrig->getTriggerName().str();
		int nPts = pTrig->getNumPoints();
		int tPos = pos;
		tPos += _snprintf(buf + tPos, maxLen - tPos,
			"%s{\"id\":%d,\"name\":\"%s\",\"isWater\":%s,\"isRiver\":%s,\"points\":[",
			first ? "" : ",",
			pTrig->getID(), name,
			pTrig->isWaterArea() ? "true" : "false",
			pTrig->isRiver()     ? "true" : "false");
		bool firstPt = true;
		for (int i = 0; i < nPts && tPos < maxLen - 64; i++) {
			const ICoord3D* pt = pTrig->getPoint(i);
			if (pt) {
				tPos += _snprintf(buf + tPos, maxLen - tPos,
					"%s{\"x\":%d,\"y\":%d}", firstPt ? "" : ",", pt->x, pt->y);
				firstPt = false;
			}
		}
		if (tPos < maxLen - 4) { buf[tPos++] = ']'; buf[tPos++] = '}'; buf[tPos] = '\0'; }
		pos = tPos;
		first = false;
	}
	if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; buf[pos] = '\0'; }
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: return sides and teams as JSON for pipe
LRESULT CMainFrame::OnWbGetTeams(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   maxLen = (int)wParam;
	if (!buf || maxLen < 64) return 0;

	if (!TheSidesList) {
		_snprintf(buf, maxLen, "{\"ok\":false,\"error\":\"no sides list\"}");
		return 0;
	}

	int pos = _snprintf(buf, maxLen, "{\"ok\":true,\"sides\":[");
	int nSides = TheSidesList->getNumSides();
	for (int i = 0; i < nSides && pos < maxLen - 256; i++) {
		Dict* d = TheSidesList->getSideInfo(i)->getDict();
		if (!d) continue;
		Bool exists = FALSE;
		AsciiString sName     = d->getAsciiString(TheKey_playerName, &exists);
		AsciiString sDispName = d->getAsciiString(TheKey_playerDisplayName, &exists);
		AsciiString sFaction  = d->getAsciiString(TheKey_playerFaction, &exists);
		pos += _snprintf(buf + pos, maxLen - pos,
			"%s{\"index\":%d,\"name\":\"%s\",\"displayName\":\"%s\",\"faction\":\"%s\"}",
			i ? "," : "", i,
			sName.str(), sDispName.str(), sFaction.str());
	}
	if (pos < maxLen - 16) pos += _snprintf(buf + pos, maxLen - pos, "],\"teams\":[");

	int nTeams = TheSidesList->getNumTeams();
	for (int i = 0; i < nTeams && pos < maxLen - 256; i++) {
		Dict* d = TheSidesList->getTeamInfo(i)->getDict();
		if (!d) continue;
		Bool exists = FALSE;
		AsciiString tName  = d->getAsciiString(TheKey_teamName, &exists);
		AsciiString tOwner = d->getAsciiString(TheKey_teamOwner, &exists);
		pos += _snprintf(buf + pos, maxLen - pos,
			"%s{\"index\":%d,\"name\":\"%s\",\"owner\":\"%s\"}",
			i ? "," : "", i, tName.str(), tOwner.str());
	}
	if (pos < maxLen - 4) { buf[pos++] = ']'; buf[pos++] = '}'; buf[pos] = '\0'; }
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: return selected map objects as JSON for pipe
LRESULT CMainFrame::OnWbGetSelected(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   maxLen = (int)wParam;
	if (!buf || maxLen < 64) return 0;

	int pos = _snprintf(buf, maxLen, "{\"ok\":true,\"objects\":[");
	bool first = true;

	for (MapObject* pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
		if (!pObj->isSelected()) continue;
		if (pos >= maxLen - 256) break;
		const Coord3D* loc = pObj->getLocation();
		bool  isWp  = pObj->isWaypoint() != FALSE;
		const char* name = isWp ? pObj->getWaypointName().str() : pObj->getName().str();
		pos += _snprintf(buf + pos, maxLen - pos,
			"%s{\"type\":\"%s\",\"name\":\"%s\",\"wx\":%.1f,\"wy\":%.1f,\"angle\":%.1f}",
			first ? "" : ",",
			isWp ? "waypoint" : "object",
			name,
			loc ? loc->x : 0.0f, loc ? loc->y : 0.0f,
			pObj->getAngle() * (180.0f / 3.14159265f));
		first = false;
	}
	if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; buf[pos] = '\0'; }
	return 0;
}

// TheSuperHackers @feature Nemellud 29/05/2026 EmbeddedMode: object properties read/write via pipe
static bool MF_JsonGetInt(const char* json, const char* key, int* out) {
	char pat[64]; _snprintf(pat, sizeof(pat), "\"%s\"", key);
	const char* p = strstr(json, pat); if (!p) return false;
	p += strlen(pat); while (*p == ':' || *p == ' ') p++;
	return sscanf(p, "%d", out) == 1;
}
static bool MF_JsonGetBool(const char* json, const char* key, bool* out) {
	char pat[64]; _snprintf(pat, sizeof(pat), "\"%s\"", key);
	const char* p = strstr(json, pat); if (!p) return false;
	p += strlen(pat); while (*p == ':' || *p == ' ') p++;
	if (strncmp(p, "true",  4) == 0) { *out = true;  return true; }
	if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
	return false;
}
static bool MF_JsonGetStr(const char* json, const char* key, char* out, int outLen) {
	char pat[64]; _snprintf(pat, sizeof(pat), "\"%s\"", key);
	const char* p = strstr(json, pat); if (!p) return false;
	p += strlen(pat); while (*p == ':' || *p == ' ') p++;
	if (*p != '"') return false; ++p;
	int i = 0; while (*p && *p != '"' && i < outLen-1) out[i++] = *p++;
	out[i] = '\0'; return true;
}

LRESULT CMainFrame::OnWbObjGetProps(WPARAM wParam, LPARAM lParam)
{
	char* buf    = (char*)lParam;
	int   maxLen = (int)wParam;
	if (!buf || maxLen < 64) return 0;

	MapObject* sel = nullptr;
	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext()) {
		if (p->isSelected()) { sel = p; break; }
	}

	if (!sel) {
		_snprintf(buf, maxLen, "{\"ok\":true,\"type\":\"none\"}");
		return 0;
	}

	const Coord3D* loc = sel->getLocation();
	float wx = loc ? loc->x : 0.f, wy = loc ? loc->y : 0.f;
	float angleDeg = sel->getAngle() * (180.f / 3.14159265f);

	if (sel->isWaypoint()) {
		AsciiString wpname = sel->getWaypointName();
		Dict* wpd = sel->getProperties();
		Bool wpPathExists = FALSE;
		AsciiString pathLabel;
		if (wpd) pathLabel = wpd->getAsciiString(TheKey_waypointPathLabel1, &wpPathExists);
		_snprintf(buf, maxLen,
			"{\"ok\":true,\"type\":\"waypoint\","
			"\"name\":\"%s\",\"wx\":%.2f,\"wy\":%.2f,\"pathLabel\":\"%s\"}",
			wpname.str(), wx, wy, (wpPathExists && pathLabel.str()) ? pathLabel.str() : "");
		return 0;
	}

	const Dict* d = sel->getProperties();
	const ThingTemplate* tmpl = sel->getThingTemplate();
	Bool exists;

#define DSTR(key)  (d ? d->getAsciiString(key, &exists).str() : "")
#define DINT(key)  (d ? d->getInt(key, &exists) : 0)
#define DBOOL(key) (d && d->getBool(key, &exists) ? "true" : "false")

	// TheSuperHackers @feature Nemellud 08/06/2026 EmbeddedMode: bridge POINT1/POINT2 exposed via obj_get_props
	auto writeBridgeProps = [&](const char* tplName, float px1, float py1, float px2, float py2, const Dict* pd) {
		const Dict* sd = pd;
		Bool ex2 = FALSE;
#define SD(key)  (sd ? sd->getAsciiString(key, &ex2).str() : "")
#define SI(key)  (sd ? sd->getInt(key, &ex2) : 0)
#define SB(key)  (sd && sd->getBool(key, &ex2) ? "true" : "false")
		_snprintf(buf, maxLen,
			"{\"ok\":true,\"type\":\"bridge\","
			"\"templateName\":\"%s\","
			"\"x1\":%.1f,\"y1\":%.1f,\"x2\":%.1f,\"y2\":%.1f,"
			"\"team\":\"%s\",\"name\":\"%s\",\"script\":\"%s\","
			"\"health\":%d,\"maxHP\":%d,"
			"\"weather\":%d,\"time\":%d,"
			"\"enabled\":%s,\"indestructible\":%s,"
			"\"unsellable\":%s,\"targetable\":%s,"
			"\"powered\":%s,\"selectable\":%s}",
			tplName, px1, py1, px2, py2,
			SD(TheKey_originalOwner), SD(TheKey_objectName), SD(TheKey_objectScriptAttachment),
			SI(TheKey_objectInitialHealth), SI(TheKey_objectMaxHPs),
			SI(TheKey_objectWeather), SI(TheKey_objectTime),
			SB(TheKey_objectEnabled), SB(TheKey_objectIndestructible),
			SB(TheKey_objectUnsellable), SB(TheKey_objectTargetable),
			SB(TheKey_objectPowered), SB(TheKey_objectSelectable));
#undef SD
#undef SI
#undef SB
	};

	if (sel->getFlag(FLAG_BRIDGE_POINT1)) {
		MapObject* p2 = sel->getNext();
		float x2 = wx, y2 = wy;
		if (p2 && p2->getFlag(FLAG_BRIDGE_POINT2)) {
			const Coord3D* l2 = p2->getLocation();
			if (l2) { x2 = l2->x; y2 = l2->y; }
		}
		writeBridgeProps(sel->getName().str(), wx, wy, x2, y2, d);
		return 0;
	}

	if (sel->getFlag(FLAG_BRIDGE_POINT2)) {
		for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext()) {
			if (!p->getFlag(FLAG_BRIDGE_POINT1) || p->getNext() != sel) continue;
			const Coord3D* l1 = p->getLocation();
			float px1 = l1 ? l1->x : 0.f, py1 = l1 ? l1->y : 0.f;
			writeBridgeProps(p->getName().str(), px1, py1, wx, wy, p->getProperties());
			return 0;
		}
		_snprintf(buf, maxLen, "{\"ok\":true,\"type\":\"none\"}");
		return 0;
	}

	bool isUnit = tmpl && (tmpl->isKindOf(KINDOF_INFANTRY) || tmpl->isKindOf(KINDOF_VEHICLE) || tmpl->isKindOf(KINDOF_HERO));
	bool isStructure = tmpl && tmpl->isKindOf(KINDOF_STRUCTURE);

	_snprintf(buf, maxLen,
		"{\"ok\":true,\"type\":\"object\","
		"\"templateName\":\"%s\","
		"\"wx\":%.2f,\"wy\":%.2f,\"angle\":%.2f,"
		"\"team\":\"%s\",\"name\":\"%s\",\"script\":\"%s\","
		"\"health\":%d,\"maxHP\":%d,"
		"\"aggressiveness\":%d,\"veterancy\":%d,"
		"\"weather\":%d,\"time\":%d,"
		"\"enabled\":%s,\"indestructible\":%s,"
		"\"unsellable\":%s,\"targetable\":%s,"
		"\"powered\":%s,\"selectable\":%s,\"aiRecruitable\":%s,"
		"\"isUnit\":%s,\"isStructure\":%s}",
		tmpl ? tmpl->getName().str() : "",
		wx, wy, angleDeg,
		DSTR(TheKey_originalOwner), DSTR(TheKey_objectName), DSTR(TheKey_objectScriptAttachment),
		DINT(TheKey_objectInitialHealth), DINT(TheKey_objectMaxHPs),
		DINT(TheKey_objectAggressiveness), DINT(TheKey_objectVeterancy),
		DINT(TheKey_objectWeather), DINT(TheKey_objectTime),
		DBOOL(TheKey_objectEnabled), DBOOL(TheKey_objectIndestructible),
		DBOOL(TheKey_objectUnsellable), DBOOL(TheKey_objectTargetable),
		DBOOL(TheKey_objectPowered), DBOOL(TheKey_objectSelectable), DBOOL(TheKey_objectRecruitableAI),
		isUnit ? "true" : "false", isStructure ? "true" : "false"
	);

#undef DSTR
#undef DINT
#undef DBOOL
	return 0;
}

LRESULT CMainFrame::OnWbObjSetProp(WPARAM, LPARAM lParam)
{
	char* json = (char*)lParam;
	if (!json) return 0;

	char key[64] = "";
	MF_JsonGetStr(json, "key", key, sizeof(key));

	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	WbView3d* p3View = WbView3d::s_instance;
	bool needViewRefresh = false;

	// wx/wy: move all selected objects together via ModifyObjectUndoable
	if ((strcmp(key, "wx") == 0 || strcmp(key, "wy") == 0) && pDoc) {
		int ival = 0;
		if (MF_JsonGetInt(json, "value", &ival)) {
			bool isX = (strcmp(key, "wx") == 0);
			for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext()) {
				if (!p->isSelected()) continue;
				const Coord3D* cur = p->getLocation();
				if (!cur) continue;
				float dx = isX ? ((float)ival - cur->x) : 0.f;
				float dy = isX ? 0.f : ((float)ival - cur->y);
				if (dx == 0.f && dy == 0.f) continue;
				ModifyObjectUndoable* pUndo = new ModifyObjectUndoable(pDoc);
				pDoc->AddAndDoUndoable(pUndo);
				pUndo->SetOffset(dx, dy);
				REF_PTR_RELEASE(pUndo);
			}
			needViewRefresh = true;
		}
		delete[] json;
		if (needViewRefresh && pDoc) pDoc->updateAllViews();
		return 0;
	}

	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext()) {
		if (!p->isSelected()) continue;
		Dict* d = p->getProperties();
		if (!d) continue;

		int   ival = 0;
		bool  bval = false;
		char  sval[256] = "";

		if (strcmp(key, "team") == 0) {
			if (MF_JsonGetStr(json, "value", sval, sizeof(sval))) d->setAsciiString(TheKey_originalOwner, AsciiString(sval));
		} else if (strcmp(key, "name") == 0) {
			if (MF_JsonGetStr(json, "value", sval, sizeof(sval))) {
				if (p->isWaypoint()) p->setWaypointName(AsciiString(sval));
				else d->setAsciiString(TheKey_objectName, AsciiString(sval));
			}
		} else if (strcmp(key, "pathLabel") == 0) {
			if (p->isWaypoint() && MF_JsonGetStr(json, "value", sval, sizeof(sval)))
				p->getProperties()->setAsciiString(TheKey_waypointPathLabel1, AsciiString(sval));
			needViewRefresh = true;
		} else if (strcmp(key, "script") == 0) {
			if (MF_JsonGetStr(json, "value", sval, sizeof(sval))) d->setAsciiString(TheKey_objectScriptAttachment, AsciiString(sval));
		} else if (strcmp(key, "angle") == 0) {
			if (MF_JsonGetInt(json, "value", &ival)) {
				p->setAngle(ival * (3.14159265f / 180.f));
				if (p3View) p3View->invalObjectInView(p);
				needViewRefresh = true;
			}
		} else if (strcmp(key, "health") == 0) {
			if (MF_JsonGetInt(json, "value", &ival)) d->setInt(TheKey_objectInitialHealth, ival);
		} else if (strcmp(key, "maxHP") == 0) {
			if (MF_JsonGetInt(json, "value", &ival)) d->setInt(TheKey_objectMaxHPs, ival);
		} else if (strcmp(key, "aggressiveness") == 0) {
			if (MF_JsonGetInt(json, "value", &ival)) d->setInt(TheKey_objectAggressiveness, ival);
		} else if (strcmp(key, "veterancy") == 0) {
			if (MF_JsonGetInt(json, "value", &ival)) d->setInt(TheKey_objectVeterancy, ival);
		} else if (strcmp(key, "weather") == 0) {
			if (MF_JsonGetInt(json, "value", &ival)) d->setInt(TheKey_objectWeather, ival);
		} else if (strcmp(key, "time") == 0) {
			if (MF_JsonGetInt(json, "value", &ival)) d->setInt(TheKey_objectTime, ival);
		} else if (strcmp(key, "enabled") == 0) {
			if (MF_JsonGetBool(json, "value", &bval)) d->setBool(TheKey_objectEnabled, bval);
		} else if (strcmp(key, "indestructible") == 0) {
			if (MF_JsonGetBool(json, "value", &bval)) d->setBool(TheKey_objectIndestructible, bval);
		} else if (strcmp(key, "unsellable") == 0) {
			if (MF_JsonGetBool(json, "value", &bval)) d->setBool(TheKey_objectUnsellable, bval);
		} else if (strcmp(key, "targetable") == 0) {
			if (MF_JsonGetBool(json, "value", &bval)) d->setBool(TheKey_objectTargetable, bval);
		} else if (strcmp(key, "powered") == 0) {
			if (MF_JsonGetBool(json, "value", &bval)) d->setBool(TheKey_objectPowered, bval);
		} else if (strcmp(key, "selectable") == 0) {
			if (MF_JsonGetBool(json, "value", &bval)) d->setBool(TheKey_objectSelectable, bval);
		} else if (strcmp(key, "aiRecruitable") == 0) {
			if (MF_JsonGetBool(json, "value", &bval)) d->setBool(TheKey_objectRecruitableAI, bval);
		}
	}

	if (needViewRefresh && pDoc) pDoc->updateAllViews();
	delete[] json;
	return 0;
}

// ── Tier I — Programmatic terrain write ──────────────────────────────────────

extern ShapeDef       g_wbPipeCreateShape;
extern bool           g_wbPipeCreateApply;
extern WbHeightRect   g_wbPipeHeightRect;
extern WbPlaceReq     g_wbPlaceReq;
extern WbLinkReq      g_wbLinkReq;
extern WbPlantTreeReq  g_wbPlantTreeReq;
extern WbPlantGroveReq g_wbPlantGroveReq;

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: create shape from pipe, optionally apply
LRESULT CMainFrame::OnWbSfCreatePipe(WPARAM, LPARAM)
{
	const ShapeDef& def = g_wbPipeCreateShape;
	// Sync static tool state from the shape def so applySelectedShape uses the correct values
	ShapeFillTool::setInnerHeight(def.innerHeight);
	ShapeFillTool::setBorderWidth(def.borderWidth);
	ShapeFillTool::setInnerTexClass(def.innerTexClass);
	ShapeFillTool::setBorderTexClass(def.borderTexClass);
	Int newId = ShapeFillTool::addShape(def);
	if (g_wbPipeCreateApply) {
		CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
		if (pDoc) ShapeFillTool::applySelectedShape(pDoc);
	}
	return (LRESULT)newId;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: fill rect region with height value
LRESULT CMainFrame::OnWbMapHeightSet(WPARAM, LPARAM)
{
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc || !pDoc->GetHeightMap()) return 0;
	WorldHeightMapEdit* pMap = pDoc->GetHeightMap();
	const WbHeightRect& r = g_wbPipeHeightRect;
	int mapW = pMap->getXExtent();
	int mapH = pMap->getYExtent();
	UnsignedByte val = (UnsignedByte)max(0, min(255, r.val));
	for (int y = r.y; y < r.y + r.h && y < mapH; y++)
		for (int x = r.x; x < r.x + r.w && x < mapW; x++)
			pMap->setHeight(x, y, val);
	IRegion2D range = {0, 0, 0, 0};
	pDoc->updateHeightMap(pMap, false, range);
	return 0;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: place named waypoint at world coords
LRESULT CMainFrame::OnWbPlaceWaypoint(WPARAM, LPARAM)
{
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc) return 0;
	const WbPlaceReq& r = g_wbPlaceReq;
	Coord3D loc = { r.wx, r.wy, 0.0f };
	MapObject* pNew = newInstance(MapObject)(loc, "*Waypoints/Waypoint", 0, 0, nullptr, nullptr);
	Int id = pDoc->getNextWaypointID();
	AsciiString wName;
	if (r.name[0]) wName.set(r.name);
	else           wName.format("Waypoint %d", id);
	pNew->setIsWaypoint();
	pNew->setWaypointID(id);
	pNew->setWaypointName(wName);
	pNew->getProperties()->setAsciiString(TheKey_originalOwner, "team");
	if (r.pathLabel[0])
		pNew->getProperties()->setAsciiString(TheKey_waypointPathLabel1, AsciiString(r.pathLabel));
	AddObjectUndoable* pUndo = new AddObjectUndoable(pDoc, pNew);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo);
	return 1;
}

// TheSuperHackers @feature Nemellud 05/06/2026 EmbeddedMode: link two named waypoints for 3D path rendering
LRESULT CMainFrame::OnWbLinkWaypoints(WPARAM, LPARAM)
{
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc || !g_wbLinkReq.name1[0] || !g_wbLinkReq.name2[0]) return 0;
	// Find both waypoints by name
	MapObject* pWay1 = nullptr;
	MapObject* pWay2 = nullptr;
	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext()) {
		if (!p->isWaypoint()) continue;
		if (strcmp(p->getWaypointName().str(), g_wbLinkReq.name1) == 0) pWay1 = p;
		if (strcmp(p->getWaypointName().str(), g_wbLinkReq.name2) == 0) pWay2 = p;
		if (pWay1 && pWay2) break;
	}
	if (!pWay1 || !pWay2) return 0;
	pDoc->addWaypointLink(pWay1->getWaypointID(), pWay2->getWaypointID());
	pDoc->updateAllViews();
	return 1;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: place game object at world coords
LRESULT CMainFrame::OnWbPlaceObject(WPARAM, LPARAM)
{
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc || !g_wbPlaceReq.name[0]) return 0;
	const WbPlaceReq& r = g_wbPlaceReq;
	Coord3D loc = { r.wx, r.wy, 0.0f };
	AsciiString templateName(r.name);
	const ThingTemplate* tt = TheThingFactory->findTemplate(templateName);
	MapObject* pNew = newInstance(MapObject)(loc, templateName, r.angle * (3.14159265f / 180.0f), 0, nullptr, tt);
	AsciiString teamName = r.team[0] ? AsciiString(r.team) : AsciiString("team");
	pNew->getProperties()->setAsciiString(TheKey_originalOwner, teamName);
	AddObjectUndoable* pUndo = new AddObjectUndoable(pDoc, pNew);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo);
	return 1;
}

// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: plant vegetation/tree at world position
// Uses ObjectOptions to resolve tree types (which are not in ThingFactory), falls back to ThingFactory.
// angle < 0 triggers random rotation (matching GroveTool behaviour).
LRESULT CMainFrame::OnWbPlantTree(WPARAM, LPARAM)
{
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc || !g_wbPlantTreeReq.name[0]) return 0;

	AsciiString templateName(g_wbPlantTreeReq.name);
	Coord3D loc = { g_wbPlantTreeReq.wx, g_wbPlantTreeReq.wy, 0.0f };

	// Tree types live in ObjectOptions, not ThingFactory
	const ThingTemplate* tt = nullptr;
	MapObject* pTemplate = ObjectOptions::getObjectNamed(templateName);
	if (pTemplate)
		tt = pTemplate->getThingTemplate();
	else
		tt = TheThingFactory->findTemplate(templateName);

	Real angle = (g_wbPlantTreeReq.angle < 0.0f)
		? ((rand() % 3600) / 10.0f * (3.14159265f / 180.0f))
		: (g_wbPlantTreeReq.angle * (3.14159265f / 180.0f));

	MapObject* pNew = newInstance(MapObject)(loc, templateName, angle, 0, nullptr, tt);
	pNew->getProperties()->setAsciiString(TheKey_originalOwner, AsciiString("team"));
	AddObjectUndoable* pUndo = new AddObjectUndoable(pDoc, pNew);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo);
	return 1;
}

// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: scatter vegetation over circular area (grove brush)
// All trees placed in one AddObjectUndoable for single Ctrl+Z undo.
LRESULT CMainFrame::OnWbPlantGrove(WPARAM, LPARAM)
{
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc || !g_wbPlantGroveReq.name[0]) return 0;

	const WbPlantGroveReq& req = g_wbPlantGroveReq;
	if (req.seed >= 0) srand((unsigned int)req.seed);

	// Build list of template names (base + numbered variants for mix mode)
	AsciiString namePool[10];
	int nameCount = 0;
	AsciiString baseName(req.name);
	namePool[nameCount++] = baseName;

	if (req.mix) {
		// Strip trailing digits from base name to get prefix (e.g. "TreePine01" → "TreePine")
		const char* n = req.name;
		int prefixLen = (int)strlen(n);
		while (prefixLen > 0 && n[prefixLen - 1] >= '0' && n[prefixLen - 1] <= '9') prefixLen--;
		if (prefixLen < (int)strlen(n)) {
			char prefix[64]; strncpy_s(prefix, sizeof(prefix), n, prefixLen); prefix[prefixLen] = '\0';
			for (int v = 1; v <= 9 && nameCount < 10; v++) {
				char candidate[68]; _snprintf(candidate, sizeof(candidate), "%s0%d", prefix, v);
				AsciiString cs(candidate);
				if (ObjectOptions::getObjectNamed(cs) && cs != baseName)
					namePool[nameCount++] = cs;
			}
		}
	}

	// Calculate target tree count: N = π * r² * density / 400
	float area = 3.14159265f * req.radius * req.radius;
	int   targetN = max(1, (int)(area * req.density / 400.0f));
	float minSq   = req.minSpacing * req.minSpacing;

	// Placed positions for min-spacing check (stack-allocated, capped at 200)
	float placedX[200], placedY[200];
	int   placed = 0;
	MapObject* head = nullptr;

	for (int attempt = 0; attempt < targetN * 6 && placed < targetN && placed < 200; attempt++) {
		// Uniform random point inside circle (rejection sampling)
		float a = ((rand() % 3600) / 3600.0f) * 2.0f * 3.14159265f;
		float d = sqrtf((float)(rand() % 10000) / 10000.0f) * req.radius;
		float tx = req.cx + cosf(a) * d;
		float ty = req.cy + sinf(a) * d;

		// Terrain checks via TheTerrainRenderObject
		if ((req.skipWater || req.skipSteep) && TheTerrainRenderObject) {
			float h = TheTerrainRenderObject->getHeightMapHeight(tx, ty, nullptr);
			if (req.skipWater && h <= 0.0f) continue;
			if (req.skipSteep) {
				float hx = TheTerrainRenderObject->getHeightMapHeight(tx + MAP_XY_FACTOR, ty, nullptr);
				float hy = TheTerrainRenderObject->getHeightMapHeight(tx, ty + MAP_XY_FACTOR, nullptr);
				if (h > 0.0f && ((hx > 0.0f && fabsf(hx - h) / MAP_XY_FACTOR > 1.5f) ||
				                  (hy > 0.0f && fabsf(hy - h) / MAP_XY_FACTOR > 1.5f))) continue;
			}
		}

		// Min-spacing check
		if (req.minSpacing > 0.0f) {
			bool tooClose = false;
			for (int i = 0; i < placed && !tooClose; i++) {
				float dx = placedX[i] - tx, dy = placedY[i] - ty;
				if (dx * dx + dy * dy < minSq) tooClose = true;
			}
			if (tooClose) continue;
		}

		// Pick template
		AsciiString& tmplName = namePool[rand() % nameCount];
		MapObject* pTemplate = ObjectOptions::getObjectNamed(tmplName);
		const ThingTemplate* tt = pTemplate ? pTemplate->getThingTemplate()
		                                     : TheThingFactory->findTemplate(tmplName);
		Real rot = req.randRot ? ((rand() % 3600) / 10.0f * (3.14159265f / 180.0f)) : 0.0f;
		Coord3D loc = { tx, ty, 0.0f };

		MapObject* pNew = newInstance(MapObject)(loc, tmplName, rot, 0, nullptr, tt);
		pNew->getProperties()->setAsciiString(TheKey_originalOwner, AsciiString("team"));
		pNew->setNextMap(head);
		head = pNew;
		placedX[placed] = tx;
		placedY[placed] = ty;
		placed++;
	}

	if (!head) return 0;
	AddObjectUndoable* pUndo = new AddObjectUndoable(pDoc, head);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo);
	return placed;
}

// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: convert screen pixel to world coordinate via active view
// wParam = screen X packed in high 16 bits | screen Y in low 16 bits; lParam = char* response buf (prepopulated size)
// Caller stores bufLen in g_wbScreenQueryBufLen before sending.
int g_wbScreenQuerySx = 0;
int g_wbScreenQuerySy = 0;
int g_wbScreenQueryBufLen = 256;
LRESULT CMainFrame::OnWbGetViewState(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   len = (int)wParam;
	if (!buf || len < 64) return 0;

	// Use WbView3d::s_instance — the only view active in embedded top-down mode
	WbView3d* pView = WbView3d::s_instance;
	if (!pView) {
		_snprintf(buf, len, "{\"ok\":false,\"error\":\"no view\"}");
		return 0;
	}
	CPoint viewPt(g_wbScreenQuerySx, g_wbScreenQuerySy);
	Coord3D worldPt;
	pView->viewToDocCoords(viewPt, &worldPt, false);
	_snprintf(buf, len, "{\"ok\":true,\"wx\":%.2f,\"wy\":%.2f}", worldPt.x, worldPt.y);
	return 0;
}

// TheSuperHackers @feature Nemellud 26/05/2026 EmbeddedMode: real-time rotation of selected objects via pipe
LRESULT CMainFrame::OnWbRotateSelected(WPARAM, LPARAM)
{
	extern float g_wbRotateAngleDeg;
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc) return 0;
	WbView3d* p3View = WbView3d::s_instance;
	const float pi = 3.14159265f;
	float angleRad = g_wbRotateAngleDeg * (pi / 180.0f);
	int count = 0;
	for (MapObject* pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
		if (pObj->isSelected() && !pObj->isWaypoint()) {
			pObj->setAngle(angleRad);
			if (p3View) p3View->invalObjectInView(pObj);
			count++;
		}
	}
	if (count > 0) pDoc->updateAllViews();
	return count;
}

// TheSuperHackers @feature Nemellud 03/07/2026 EmbeddedMode: absolute get/set of the
// impassable-areas overlay. wParam: -1 = query only, 0 = off, 1 = on. Returns actual
// state (0/1) so the Electron UI can resync after a renderer refresh.
LRESULT CMainFrame::OnWbImpassableView(WPARAM wParam, LPARAM)
{
	if (!TheTerrainRenderObject) return 0;
	Bool cur = TheTerrainRenderObject->getShowImpassableAreas();
	int want = (int)wParam;
	if (want >= 0 && ((want != 0) != (cur != 0))) {
		TheTerrainRenderObject->setShowImpassableAreas(want != 0);
		cur = (want != 0);
		WbView3d* p3View = WbView3d::s_instance;
		CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
		if (p3View && pDoc) {
			// Force the entire terrain mesh to be rerendered (same as the menu handler)
			IRegion2D range = {0,0,0,0};
			p3View->updateHeightMapInView(pDoc->GetHeightMap(), false, range);
		}
	}
	return cur ? 1 : 0;
}

// ── Tier J — SidesList wizard ─────────────────────────────────────────────────
// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: SidesList full JSON serialization

static const char* MF_ParamTypeName(Parameter::ParameterType t)
{
	switch (t) {
		case Parameter::INT:                     return "INT";
		case Parameter::REAL:                    return "REAL";
		case Parameter::SCRIPT:                  return "SCRIPT";
		case Parameter::TEAM:                    return "TEAM";
		case Parameter::COUNTER:                 return "COUNTER";
		case Parameter::FLAG:                    return "FLAG";
		case Parameter::COMPARISON:              return "COMPARISON";
		case Parameter::WAYPOINT:                return "WAYPOINT";
		case Parameter::BOOLEAN:                 return "BOOLEAN";
		case Parameter::TRIGGER_AREA:            return "TRIGGER_AREA";
		case Parameter::TEXT_STRING:             return "TEXT_STRING";
		case Parameter::SIDE:                    return "SIDE";
		case Parameter::SOUND:                   return "SOUND";
		case Parameter::SCRIPT_SUBROUTINE:       return "SCRIPT_SUBROUTINE";
		case Parameter::UNIT:                    return "UNIT";
		case Parameter::OBJECT_TYPE:             return "OBJECT_TYPE";
		case Parameter::COORD3D:                 return "COORD3D";
		case Parameter::ANGLE:                   return "ANGLE";
		case Parameter::TEAM_STATE:              return "TEAM_STATE";
		case Parameter::RELATION:                return "RELATION";
		case Parameter::AI_MOOD:                 return "AI_MOOD";
		case Parameter::DIALOG:                  return "DIALOG";
		case Parameter::MUSIC:                   return "MUSIC";
		case Parameter::MOVIE:                   return "MOVIE";
		case Parameter::WAYPOINT_PATH:           return "WAYPOINT_PATH";
		case Parameter::LOCALIZED_TEXT:          return "LOCALIZED_TEXT";
		case Parameter::BRIDGE:                  return "BRIDGE";
		case Parameter::KIND_OF_PARAM:           return "KIND_OF_PARAM";
		case Parameter::ATTACK_PRIORITY_SET:     return "ATTACK_PRIORITY_SET";
		case Parameter::RADAR_EVENT_TYPE:        return "RADAR_EVENT_TYPE";
		case Parameter::SPECIAL_POWER:           return "SPECIAL_POWER";
		case Parameter::SCIENCE:                 return "SCIENCE";
		case Parameter::UPGRADE:                 return "UPGRADE";
		case Parameter::COMMANDBUTTON_ABILITY:   return "COMMANDBUTTON_ABILITY";
		case Parameter::BOUNDARY:                return "BOUNDARY";
		case Parameter::BUILDABLE:               return "BUILDABLE";
		case Parameter::SURFACES_ALLOWED:        return "SURFACES_ALLOWED";
		case Parameter::SHAKE_INTENSITY:         return "SHAKE_INTENSITY";
		case Parameter::COMMAND_BUTTON:          return "COMMAND_BUTTON";
		case Parameter::FONT_NAME:               return "FONT_NAME";
		case Parameter::OBJECT_STATUS:           return "OBJECT_STATUS";
		case Parameter::COLOR:                   return "COLOR";
		case Parameter::PERCENT:                 return "PERCENT";
		default:                                 return "UNKNOWN";
	}
}

// Append a single parameter as JSON to buf+pos. Returns new pos.
static int MF_AppendParam(char* buf, int pos, int maxLen, Parameter* p)
{
	if (!p || pos >= maxLen - 128) return pos;
	const char* typeName = MF_ParamTypeName(p->getParameterType());
	char valBuf[256] = "";
	switch (p->getParameterType()) {
		case Parameter::INT:
		case Parameter::COMPARISON:
		case Parameter::BOOLEAN:
		case Parameter::RELATION:
		case Parameter::AI_MOOD:
		case Parameter::KIND_OF_PARAM:
		case Parameter::RADAR_EVENT_TYPE:
		case Parameter::COMMANDBUTTON_ABILITY:
		case Parameter::BOUNDARY:
		case Parameter::BUILDABLE:
		case Parameter::SURFACES_ALLOWED:
		case Parameter::SHAKE_INTENSITY:
		case Parameter::COLOR:
			_snprintf(valBuf, sizeof(valBuf), "%d", p->getInt());
			break;
		case Parameter::REAL:
		case Parameter::ANGLE:
		case Parameter::PERCENT:
			_snprintf(valBuf, sizeof(valBuf), "%.4f", p->getReal());
			break;
		default:
			// All string-typed params — escape quotes minimally
			{
				const char* s = p->getString().str();
				int vi = 0;
				for (int si = 0; s[si] && vi < (int)sizeof(valBuf) - 3; si++) {
					if (s[si] == '"' || s[si] == '\\') valBuf[vi++] = '\\';
					valBuf[vi++] = s[si];
				}
				valBuf[vi] = '\0';
			}
			break;
	}
	return pos + _snprintf(buf + pos, maxLen - pos,
		"{\"pt\":\"%s\",\"v\":\"%s\"}", typeName, valBuf);
}

// Append a ScriptAction list as JSON array to buf. Returns new pos.
static int MF_AppendActions(char* buf, int pos, int maxLen, ScriptAction* first)
{
	bool firstAct = true;
	for (ScriptAction* a = first; a && pos < maxLen - 256; a = a->getNext()) {
		if (!firstAct) { if (pos < maxLen - 1) buf[pos++] = ','; }
		firstAct = false;
		pos += _snprintf(buf + pos, maxLen - pos,
			"{\"type\":%d,\"params\":[", (int)a->getActionType());
		int nParms = a->getNumParameters();
		for (int pi = 0; pi < nParms && pos < maxLen - 128; pi++) {
			if (pi) { if (pos < maxLen - 1) buf[pos++] = ','; }
			pos = MF_AppendParam(buf, pos, maxLen, a->getParameter(pi));
		}
		if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; }
	}
	return pos;
}

LRESULT CMainFrame::OnWbGetSideList(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int   maxLen = (int)wParam;
	if (!buf || maxLen < 64) return 0;

	if (!TheSidesList) {
		_snprintf(buf, maxLen, "{\"ok\":false,\"error\":\"no sides list\"}");
		return 0;
	}

	int pos = _snprintf(buf, maxLen, "{\"ok\":true,\"players\":[");
	int nSides = TheSidesList->getNumSides();

	for (int si = 0; si < nSides && pos < maxLen - 512; si++) {
		if (si) { if (pos < maxLen - 1) buf[pos++] = ','; }
		SidesInfo* side = TheSidesList->getSideInfo(si);
		Dict* d = side ? side->getDict() : nullptr;
		Bool exists = FALSE;

		AsciiString sName    = d ? d->getAsciiString(TheKey_playerName,        &exists) : AsciiString("");
		AsciiString sDisp    = d ? d->getAsciiString(TheKey_playerDisplayName,  &exists) : AsciiString("");
		AsciiString sFact    = d ? d->getAsciiString(TheKey_playerFaction,      &exists) : AsciiString("");
		AsciiString sColor   = d ? d->getAsciiString(TheKey_playerColor,        &exists) : AsciiString("");
		AsciiString sAllies  = d ? d->getAsciiString(TheKey_playerAllies,       &exists) : AsciiString("");
		AsciiString sEnemies = d ? d->getAsciiString(TheKey_playerEnemies,      &exists) : AsciiString("");
		int money            = d ? d->getInt(TheKey_playerStartMoney,           &exists) : 0;
		bool isHuman         = d ? (d->getBool(TheKey_playerIsHuman,            &exists) != FALSE) : false;

		pos += _snprintf(buf + pos, maxLen - pos,
			"{\"index\":%d,\"name\":\"%s\",\"displayName\":\"%s\","
			"\"faction\":\"%s\",\"color\":\"%s\",\"money\":%d,\"isHuman\":%s,"
			"\"allies\":\"%s\",\"enemies\":\"%s\",\"scriptGroups\":[",
			si, sName.str(), sDisp.str(), sFact.str(), sColor.str(), money,
			isHuman ? "true" : "false", sAllies.str(), sEnemies.str());

		ScriptList* sl = side ? side->getScriptList() : nullptr;
		bool firstGroup = true;
		for (ScriptGroup* sg = sl ? sl->getScriptGroup() : nullptr;
		     sg && pos < maxLen - 256; sg = sg->getNext())
		{
			if (!firstGroup) { if (pos < maxLen - 1) buf[pos++] = ','; }
			firstGroup = false;

			pos += _snprintf(buf + pos, maxLen - pos,
				"{\"name\":\"%s\",\"active\":%s,\"scripts\":[",
				sg->getName().str(), sg->isActive() ? "true" : "false");

			bool firstScript = true;
			for (Script* s = sg->getScript(); s && pos < maxLen - 512; s = s->getNext()) {
				if (!firstScript) { if (pos < maxLen - 1) buf[pos++] = ','; }
				firstScript = false;

				pos += _snprintf(buf + pos, maxLen - pos,
					"{\"name\":\"%s\",\"active\":%s,\"easy\":%s,"
					"\"normal\":%s,\"hard\":%s,\"oneShot\":%s,\"subroutine\":%s",
					s->getName().str(),
					s->isActive()     ? "true" : "false",
					s->isEasy()       ? "true" : "false",
					s->isNormal()     ? "true" : "false",
					s->isHard()       ? "true" : "false",
					s->isOneShot()    ? "true" : "false",
					s->isSubroutine() ? "true" : "false");

				// Conditions — OR-list of AND-clauses
				pos += _snprintf(buf + pos, maxLen - pos, ",\"conditions\":[");
				bool firstOr = true;
				for (OrCondition* oc = s->getOrCondition();
				     oc && pos < maxLen - 256; oc = oc->getNextOrCondition())
				{
					if (!firstOr) { if (pos < maxLen - 1) buf[pos++] = ','; }
					firstOr = false;
					if (pos < maxLen - 1) buf[pos++] = '[';
					bool firstAnd = true;
					for (Condition* c = oc->getFirstAndCondition();
					     c && pos < maxLen - 128; c = c->getNext())
					{
						if (!firstAnd) { if (pos < maxLen - 1) buf[pos++] = ','; }
						firstAnd = false;
						pos += _snprintf(buf + pos, maxLen - pos,
							"{\"type\":%d,\"params\":[", (int)c->getConditionType());
						int nParms = c->getNumParameters();
						for (int pi = 0; pi < nParms && pos < maxLen - 128; pi++) {
							if (pi) { if (pos < maxLen - 1) buf[pos++] = ','; }
							pos = MF_AppendParam(buf, pos, maxLen, c->getParameter(pi));
						}
						if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; }
					}
					if (pos < maxLen - 1) buf[pos++] = ']';
				}
				if (pos < maxLen - 1) buf[pos++] = ']';

				// True actions
				pos += _snprintf(buf + pos, maxLen - pos, ",\"actionsTrue\":[");
				pos = MF_AppendActions(buf, pos, maxLen, s->getAction());
				if (pos < maxLen - 1) buf[pos++] = ']';

				// False actions
				pos += _snprintf(buf + pos, maxLen - pos, ",\"actionsFalse\":[");
				pos = MF_AppendActions(buf, pos, maxLen, s->getFalseAction());
				if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; } // close script
			}

			if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; } // close group
		}

		if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; } // close player
	}

	// Teams
	if (pos < maxLen - 16) pos += _snprintf(buf + pos, maxLen - pos, "],\"teams\":[");
	int nTeams = TheSidesList->getNumTeams();
	for (int ti = 0; ti < nTeams && pos < maxLen - 256; ti++) {
		if (ti) { if (pos < maxLen - 1) buf[pos++] = ','; }
		TeamsInfo* team = TheSidesList->getTeamInfo(ti);
		Dict* d = team ? team->getDict() : nullptr;
		Bool exists = FALSE;
		AsciiString tName   = d ? d->getAsciiString(TheKey_teamName,         &exists) : AsciiString("");
		AsciiString tOwner  = d ? d->getAsciiString(TheKey_teamOwner,        &exists) : AsciiString("");
		AsciiString tHome   = d ? d->getAsciiString(TheKey_teamHome,         &exists) : AsciiString("");

		pos += _snprintf(buf + pos, maxLen - pos,
			"{\"name\":\"%s\",\"owner\":\"%s\",\"home\":\"%s\",\"units\":[",
			tName.str(), tOwner.str(), tHome.str());

		// TheSuperHackers @bugfix Nemellud 10/06/2026 EmbeddedMode: read the engine's
		// well-known unit slot keys (teamUnitType1..7) instead of made-up teamObject_N keys.
		// "count" kept as alias of maxCount for older UI callers.
		bool firstUnit = true;
		for (int ui = 1; ui <= 7 && pos < maxLen - 160; ui++) {
			char objKey[64], minKey[64], maxKey[64];
			_snprintf(objKey, sizeof(objKey), "teamUnitType%d",     ui);
			_snprintf(minKey, sizeof(minKey), "teamUnitMinCount%d", ui);
			_snprintf(maxKey, sizeof(maxKey), "teamUnitMaxCount%d", ui);
			AsciiString tpl = d ? d->getAsciiString(NAMEKEY(objKey), &exists) : AsciiString("");
			if (!exists || tpl.isEmpty() || tpl == AsciiString("<none>")) continue;
			int minCnt = d ? d->getInt(NAMEKEY(minKey), &exists) : 0;
			if (!exists) minCnt = 0;
			int maxCnt = d ? d->getInt(NAMEKEY(maxKey), &exists) : 1;
			if (!exists) maxCnt = 1;
			if (!firstUnit) { if (pos < maxLen - 1) buf[pos++] = ','; }
			firstUnit = false;
			pos += _snprintf(buf + pos, maxLen - pos,
				"{\"template\":\"%s\",\"minCount\":%d,\"maxCount\":%d,\"count\":%d}",
				tpl.str(), minCnt, maxCnt, maxCnt);
		}

		if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; }
	}

	if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; buf[pos] = '\0'; }
	return 0;
}

// ── SidesList CRUD helpers ────────────────────────────────────────────────────

static Parameter::ParameterType MF_ParamTypeFromName(const char* name)
{
	if (!name || !name[0]) return Parameter::TEXT_STRING;
	if (strcmp(name, "INT") == 0)                   return Parameter::INT;
	if (strcmp(name, "REAL") == 0)                  return Parameter::REAL;
	if (strcmp(name, "SCRIPT") == 0)                return Parameter::SCRIPT;
	if (strcmp(name, "TEAM") == 0)                  return Parameter::TEAM;
	if (strcmp(name, "COUNTER") == 0)               return Parameter::COUNTER;
	if (strcmp(name, "FLAG") == 0)                  return Parameter::FLAG;
	if (strcmp(name, "COMPARISON") == 0)            return Parameter::COMPARISON;
	if (strcmp(name, "WAYPOINT") == 0)              return Parameter::WAYPOINT;
	if (strcmp(name, "BOOLEAN") == 0)               return Parameter::BOOLEAN;
	if (strcmp(name, "TRIGGER_AREA") == 0)          return Parameter::TRIGGER_AREA;
	if (strcmp(name, "TEXT_STRING") == 0)           return Parameter::TEXT_STRING;
	if (strcmp(name, "SIDE") == 0)                  return Parameter::SIDE;
	if (strcmp(name, "SOUND") == 0)                 return Parameter::SOUND;
	if (strcmp(name, "SCRIPT_SUBROUTINE") == 0)     return Parameter::SCRIPT_SUBROUTINE;
	if (strcmp(name, "UNIT") == 0)                  return Parameter::UNIT;
	if (strcmp(name, "OBJECT_TYPE") == 0)           return Parameter::OBJECT_TYPE;
	if (strcmp(name, "COORD3D") == 0)               return Parameter::COORD3D;
	if (strcmp(name, "ANGLE") == 0)                 return Parameter::ANGLE;
	if (strcmp(name, "TEAM_STATE") == 0)            return Parameter::TEAM_STATE;
	if (strcmp(name, "RELATION") == 0)              return Parameter::RELATION;
	if (strcmp(name, "AI_MOOD") == 0)               return Parameter::AI_MOOD;
	if (strcmp(name, "DIALOG") == 0)                return Parameter::DIALOG;
	if (strcmp(name, "MUSIC") == 0)                 return Parameter::MUSIC;
	if (strcmp(name, "MOVIE") == 0)                 return Parameter::MOVIE;
	if (strcmp(name, "WAYPOINT_PATH") == 0)         return Parameter::WAYPOINT_PATH;
	if (strcmp(name, "LOCALIZED_TEXT") == 0)        return Parameter::LOCALIZED_TEXT;
	if (strcmp(name, "BRIDGE") == 0)                return Parameter::BRIDGE;
	if (strcmp(name, "PERCENT") == 0)               return Parameter::PERCENT;
	return Parameter::TEXT_STRING;
}

// Set a parameter's value from a string based on its ParameterType
static void MF_SetParam(Parameter* p, const char* valStr)
{
	if (!p || !valStr) return;
	switch (p->getParameterType()) {
		case Parameter::INT:
		case Parameter::COMPARISON:
		case Parameter::BOOLEAN:
		case Parameter::RELATION:
		case Parameter::AI_MOOD:
		case Parameter::KIND_OF_PARAM:
		case Parameter::RADAR_EVENT_TYPE:
		case Parameter::COMMANDBUTTON_ABILITY:
		case Parameter::BOUNDARY:
		case Parameter::BUILDABLE:
		case Parameter::SURFACES_ALLOWED:
		case Parameter::SHAKE_INTENSITY:
		case Parameter::COLOR:
			p->friend_setInt(atoi(valStr));
			break;
		case Parameter::REAL:
		case Parameter::ANGLE:
		case Parameter::PERCENT:
			p->friend_setReal((float)atof(valStr));
			break;
		default:
			p->friend_setString(AsciiString(valStr));
			break;
	}
}

// Parse conditions from flat-keyed JSON: cond{ci}_type, cond{ci}_pCount, cond{ci}_p{pi}_pt/v
// Returns the first OrCondition (linked list), or nullptr.
static OrCondition* MF_ParseConditions(const char* json, int condCount)
{
	OrCondition* firstOr = nullptr;
	OrCondition* lastOr  = nullptr;
	// Per OR-groep (index 0..31): de OrCondition en de laatste AND-conditie erin.
	OrCondition* grpOr[32]   = { nullptr };
	Condition*   grpLast[32] = { nullptr };

	for (int ci = 0; ci < condCount; ci++) {
		char key[64];
		int  type = 0;
		_snprintf(key, sizeof(key), "cond%d_type", ci);
		if (!MF_JsonGetInt(json, key, &type)) continue;

		Condition* cond = newInstance(Condition)((Condition::ConditionType)type);
		if (!cond) continue;

		int pCount = 0;
		_snprintf(key, sizeof(key), "cond%d_pCount", ci);
		MF_JsonGetInt(json, key, &pCount);
		for (int pi = 0; pi < pCount; pi++) {
			char ptKey[80], vKey[80], ptStr[64], vStr[256];
			_snprintf(ptKey, sizeof(ptKey), "cond%d_p%d_pt", ci, pi);
			_snprintf(vKey,  sizeof(vKey),  "cond%d_p%d_v",  ci, pi);
			ptStr[0] = vStr[0] = '\0';
			MF_JsonGetStr(json, ptKey, ptStr, sizeof(ptStr));
			MF_JsonGetStr(json, vKey,  vStr,  sizeof(vStr));
			Parameter* p = cond->getParameter(pi);
			if (p) MF_SetParam(p, vStr);
		}

		// TheSuperHackers @bugfix Nemellud 12/06/2026 EmbeddedMode: honor the per-condition OR-group
		// (cond{i}_or). Conditions with the same group are AND-linked in one OrCondition; different
		// groups become separate OrConditions (OR). Previously every condition got its own OR clause,
		// so multi-condition AND (e.g. "all bosses destroyed") wrongly became OR ("any boss destroyed"),
		// and the scripts modal's OR feature was ignored. Default group 0 → all conditions AND.
		int orGroup = 0;
		_snprintf(key, sizeof(key), "cond%d_or", ci);
		MF_JsonGetInt(json, key, &orGroup);
		if (orGroup < 0 || orGroup >= 32) orGroup = 0;

		if (!grpOr[orGroup]) {
			OrCondition* orClause = newInstance(OrCondition);
			orClause->setFirstAndCondition(cond);
			grpOr[orGroup]   = orClause;
			grpLast[orGroup] = cond;
			if (!firstOr) firstOr = orClause;
			if (lastOr)   lastOr->setNextOrCondition(orClause);
			lastOr = orClause;
		} else {
			grpLast[orGroup]->setNextCondition(cond);
			grpLast[orGroup] = cond;
		}
	}
	return firstOr;
}

// Parse actions from flat-keyed JSON: act{prefix}{ai}_type, act{prefix}{ai}_pCount, ...
static ScriptAction* MF_ParseActions(const char* json, int actCount, const char* prefix)
{
	ScriptAction* firstAct = nullptr;
	ScriptAction* lastAct  = nullptr;

	for (int ai = 0; ai < actCount; ai++) {
		char key[64];
		int  type = 0;
		_snprintf(key, sizeof(key), "%s%d_type", prefix, ai);
		if (!MF_JsonGetInt(json, key, &type)) continue;

		ScriptAction* act = newInstance(ScriptAction)((ScriptAction::ScriptActionType)type);
		if (!act) continue;

		int pCount = 0;
		_snprintf(key, sizeof(key), "%s%d_pCount", prefix, ai);
		MF_JsonGetInt(json, key, &pCount);
		for (int pi = 0; pi < pCount; pi++) {
			char ptKey[80], vKey[80], ptStr[64], vStr[256];
			_snprintf(ptKey, sizeof(ptKey), "%s%d_p%d_pt", prefix, ai, pi);
			_snprintf(vKey,  sizeof(vKey),  "%s%d_p%d_v",  prefix, ai, pi);
			ptStr[0] = vStr[0] = '\0';
			MF_JsonGetStr(json, ptKey, ptStr, sizeof(ptStr));
			MF_JsonGetStr(json, vKey,  vStr,  sizeof(vStr));
			Parameter* p = act->getParameter(pi);
			if (p) MF_SetParam(p, vStr);
		}

		if (!firstAct) firstAct = act;
		if (lastAct)   lastAct->setNextAction(act);
		lastAct = act;
	}
	return firstAct;
}

// ── Handler implementations ───────────────────────────────────────────────────

// TheSuperHackers @feature Nemellud 02/07/2026 EmbeddedMode: commit pipe-driven
// SidesList mutations through WB's undo stack (same pattern as CTeamsDialog::OnOK),
// so Ctrl+Z reverts player/team/script changes made via the REST API. Falls back
// to a direct write when no document is active (should not happen in practice).
static void MF_CommitSidesUndoable(SidesList& newSides)
{
	Bool modified = newSides.validateSides();
	(void)modified;
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc && pDoc->GetActive3DView()) {
		SidesListUndoable* pUndo = new SidesListUndoable(newSides, pDoc);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	} else {
		*TheSidesList = newSides;
	}
}

// TheSuperHackers @tweak Nemellud 02/07/2026 EmbeddedMode: mutate a copy of the
// SidesList and commit via SidesListUndoable so the change is undoable (Ctrl+Z)
LRESULT CMainFrame::OnWbSetPlayer(WPARAM, LPARAM lParam)
{
	char* json = (char*)lParam;
	if (TheSidesList) {
		char name[64] = "";
		MF_JsonGetStr(json, "name", name, sizeof(name));
		SidesList newSides;
		newSides = *TheSidesList;
		SidesInfo* side = newSides.findSideInfo(AsciiString(name));
		if (side) {
			Dict* d = side->getDict();
			char sval[1024] = "";
			int   ival = 0;
			if (MF_JsonGetStr(json,  "displayName", sval, sizeof(sval))) d->setAsciiString(TheKey_playerDisplayName, AsciiString(sval));
			if (MF_JsonGetStr(json,  "color",       sval, sizeof(sval))) d->setAsciiString(TheKey_playerColor,       AsciiString(sval));
			if (MF_JsonGetStr(json,  "faction",     sval, sizeof(sval))) d->setAsciiString(TheKey_playerFaction,     AsciiString(sval));
			if (MF_JsonGetStr(json,  "allies",      sval, sizeof(sval))) d->setAsciiString(TheKey_playerAllies,      AsciiString(sval));
			if (MF_JsonGetStr(json,  "enemies",     sval, sizeof(sval))) d->setAsciiString(TheKey_playerEnemies,     AsciiString(sval));
			if (MF_JsonGetInt(json, "money", &ival))  d->setInt(TheKey_playerStartMoney, ival);
			bool bIsHuman = false;
			if (MF_JsonGetBool(json, "isHuman", &bIsHuman)) d->setBool(TheKey_playerIsHuman, bIsHuman ? TRUE : FALSE);
			MF_CommitSidesUndoable(newSides);
		}
	}
	delete[] json;
	return 0;
}

// TheSuperHackers @feature Nemellud 31/05/2026 EmbeddedMode: add new player side via pipe
LRESULT CMainFrame::OnWbAddPlayer(WPARAM, LPARAM lParam)
{
	char* json = (char*)lParam;
	if (TheSidesList) {
		char name[64] = "", faction[128] = "";
		MF_JsonGetStr(json, "name",    name,    sizeof(name));
		MF_JsonGetStr(json, "faction", faction, sizeof(faction));
		if (name[0] && !TheSidesList->findSideInfo(AsciiString(name))) {
			// TheSuperHackers @tweak Nemellud 02/07/2026 EmbeddedMode: undoable via SidesListUndoable
			Dict d;
			UnicodeString uName;
			uName.translate(AsciiString(name));
			d.setAsciiString  (TheKey_playerName,        AsciiString(name));
			d.setUnicodeString(TheKey_playerDisplayName,  uName);
			d.setAsciiString  (TheKey_playerFaction,      AsciiString(faction[0] ? faction : "FactionAmerica"));
			d.setBool         (TheKey_playerIsHuman,      false);
			d.setInt          (TheKey_playerStartMoney,   10000);
			d.setAsciiString  (TheKey_playerAllies,       AsciiString(""));
			d.setAsciiString  (TheKey_playerEnemies,      AsciiString(""));
			SidesList newSides;
			newSides = *TheSidesList;
			newSides.addSide(&d);
			MF_CommitSidesUndoable(newSides);
		}
	}
	delete[] json;
	return 0;
}

// TheSuperHackers @feature Nemellud 31/05/2026 EmbeddedMode: remove player side by name via pipe
LRESULT CMainFrame::OnWbDelPlayer(WPARAM, LPARAM lParam)
{
	char* json = (char*)lParam;
	if (TheSidesList) {
		char name[64] = "";
		MF_JsonGetStr(json, "name", name, sizeof(name));
		if (name[0]) {
			// TheSuperHackers @tweak Nemellud 02/07/2026 EmbeddedMode: undoable via SidesListUndoable
			SidesList newSides;
			newSides = *TheSidesList;
			int numSides = newSides.getNumSides();
			for (int i = 0; i < numSides; i++) {
				SidesInfo* si = newSides.getSideInfo(i);
				if (!si) continue;
				Bool exists = FALSE;
				AsciiString sName = si->getDict()->getAsciiString(TheKey_playerName, &exists);
				if (exists && sName == AsciiString(name)) {
					newSides.removeSide(i);
					MF_CommitSidesUndoable(newSides);
					break;
				}
			}
		}
	}
	delete[] json;
	return 0;
}

// TheSuperHackers @feature Nemellud 31/05/2026 EmbeddedMode: delete waypoint by name via pipe
LRESULT CMainFrame::OnWbDelWaypoint(WPARAM, LPARAM lParam)
{
	char* json = (char*)lParam;
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (pDoc) {
		char name[128] = "";
		MF_JsonGetStr(json, "name", name, sizeof(name));
		if (name[0]) {
			// Deselect all, then select only the named waypoint
			for (MapObject* pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext())
				pObj->setSelected(false);
			bool found = false;
			for (MapObject* pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
				if (pObj->isWaypoint() && pObj->getWaypointName() == AsciiString(name)) {
					pObj->setSelected(true);
					found = true;
					break;
				}
			}
			if (found) {
				DeleteObjectUndoable* pUndo = new DeleteObjectUndoable(pDoc);
				pDoc->AddAndDoUndoable(pUndo);
				REF_PTR_RELEASE(pUndo);
				pDoc->updateAllViews();
			}
		}
	}
	delete[] json;
	return 0;
}

// TheSuperHackers @feature Nemellud 31/05/2026 EmbeddedMode: create polygon trigger via pipe
LRESULT CMainFrame::OnWbAddTrigger(WPARAM, LPARAM lParam)
{
	char* json = (char*)lParam;
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (pDoc) {
		char name[128] = "";
		Bool isWater = FALSE, isRiver = FALSE;
		MF_JsonGetStr (json, "name",    name,    sizeof(name));
		MF_JsonGetBool(json, "isWater", &isWater);
		MF_JsonGetBool(json, "isRiver", &isRiver);

		if (name[0]) {
			PolygonTrigger* pNew = newInstance(PolygonTrigger)(8);
			pNew->setTriggerName(AsciiString(name));
			pNew->setWaterArea(isWater);
			pNew->setRiver(isRiver);

			// Parse "points":[{"x":N,"y":M},...] manually
			const char* pArr = strstr(json, "\"points\"");
			if (pArr) pArr = strchr(pArr, '[');
			if (pArr) {
				pArr++;
				while (*pArr && *pArr != ']') {
					const char* endObj = strchr(pArr, '}');
					if (!endObj) break;
					const char* px = strstr(pArr, "\"x\"");
					const char* py = strstr(pArr, "\"y\"");
					ICoord3D pt = {0, 0, 0};
					if (px && px < endObj) { px = strchr(px, ':'); if (px) pt.x = atoi(px + 1); }
					if (py && py < endObj) { py = strchr(py, ':'); if (py) pt.y = atoi(py + 1); }
					pNew->addPoint(pt);
					pArr = endObj + 1;
					if (*pArr == ',') pArr++;
				}
			}

			if (pNew->getNumPoints() >= 3) {
				AddPolygonUndoable* pUndo = new AddPolygonUndoable(pNew);
				pDoc->AddAndDoUndoable(pUndo);
				REF_PTR_RELEASE(pUndo);
				pDoc->updateAllViews();
			} else {
				deleteInstance(pNew);
			}
		}
	}
	delete[] json;
	return 0;
}

// TheSuperHackers @feature Nemellud 31/05/2026 EmbeddedMode: delete polygon trigger by name via pipe
LRESULT CMainFrame::OnWbDelTrigger(WPARAM, LPARAM lParam)
{
	char* json = (char*)lParam;
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (pDoc) {
		char name[128] = "";
		MF_JsonGetStr(json, "name", name, sizeof(name));
		if (name[0]) {
			for (PolygonTrigger* pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
				if (pTrig->getTriggerName() == AsciiString(name)) {
					DeletePolygonUndoable* pUndo = new DeletePolygonUndoable(pTrig);
					pDoc->AddAndDoUndoable(pUndo);
					REF_PTR_RELEASE(pUndo);
					pDoc->updateAllViews();
					break;
				}
			}
		}
	}
	delete[] json;
	return 0;
}

// TheSuperHackers @feature Nemellud 31/05/2026 EmbeddedMode: rename/update trigger properties via pipe
LRESULT CMainFrame::OnWbSetTrigger(WPARAM, LPARAM lParam)
{
	char* json = (char*)lParam;
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (pDoc) {
		char name[128] = "", newName[128] = "";
		MF_JsonGetStr(json, "name",    name,    sizeof(name));
		MF_JsonGetStr(json, "newName", newName, sizeof(newName));
		if (name[0]) {
			for (PolygonTrigger* pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
				if (pTrig->getTriggerName() == AsciiString(name)) {
					if (newName[0]) pTrig->setTriggerName(AsciiString(newName));
					Bool isWater = pTrig->isWaterArea(), isRiver = pTrig->isRiver();
					MF_JsonGetBool(json, "isWater", &isWater);
					MF_JsonGetBool(json, "isRiver", &isRiver);
					pTrig->setWaterArea(isWater);
					pTrig->setRiver(isRiver);
					pDoc->updateAllViews();
					break;
				}
			}
		}
	}
	delete[] json;
	return 0;
}

// TheSuperHackers @feature Nemellud 06/06/2026 EmbeddedMode: select waypoint/object by name via pipe
LRESULT CMainFrame::OnWbSelectObject(WPARAM, LPARAM lParam)
{
	char* json = (char*)lParam;
	if (!json) return 0;
	char name[256] = "";
	MF_JsonGetStr(json, "name", name, sizeof(name));
	delete[] json;
	if (!name[0]) return 0;

	MapObject* target = nullptr;
	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext()) {
		p->setSelected(FALSE);
		if (!target) {
			if (p->isWaypoint() && strcmp(p->getWaypointName().str(), name) == 0) target = p;
			else if (!p->isWaypoint() && strcmp(p->getName().str(), name) == 0) target = p;
		}
	}
	if (target) target->setSelected(TRUE);

	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (pDoc) pDoc->updateAllViews();
	return target ? 1 : 0;
}

// TheSuperHackers @feature Nemellud 06/06/2026 EmbeddedMode: handle generic pipe sub-commands (SUBCMD_LOAD)
LRESULT CMainFrame::OnWbPipeCmd(WPARAM wParam, LPARAM lParam)
{
	if (wParam == 1 /* SUBCMD_LOAD */) {
		char* path = reinterpret_cast<char*>(lParam);
		if (path) {
			AfxGetApp()->OpenDocumentFile(path);
			free(path);
		}
	}
	return 0;
}

// TheSuperHackers @feature Nemellud 06/06/2026 EmbeddedMode: create new map via pipe (no native dialog)
LRESULT CMainFrame::OnWbNewMap(WPARAM, LPARAM)
{
	// TheSuperHackers @bugfix Nemellud 08/07/2026 EmbeddedMode: ID_FILE_NEW goes through MFC's
	// default CDocument::SaveModified(), which pops a native "save changes?" dialog if the
	// current doc is dirty - that dialog blocks this pipe command indefinitely (the pipe has
	// no way to click it), silently stalling every pipe-driven new_map call until a human
	// happens to notice and dismiss it. A pipe-driven "new map" request already means the
	// caller intends to discard the current map, so clear the modified flag first - same
	// intent as resize_map/ResizeFromPipe just above, which never goes through this MFC
	// check at all.
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (pDoc) pDoc->SetModifiedFlag(FALSE);
	// g_wbNewMapReq is already set by WbPipeServer; trigger File New to invoke OnNewDocument
	PostMessage(WM_COMMAND, ID_FILE_NEW, 0);
	return 0;
}

// TheSuperHackers @feature Nemellud 06/06/2026 EmbeddedMode: resize map via pipe (no native dialog)
LRESULT CMainFrame::OnWbResizeMap(WPARAM, LPARAM)
{
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (pDoc)
		pDoc->ResizeFromPipe();
	return 0;
}

LRESULT CMainFrame::OnWbSetTeam(WPARAM, LPARAM lParam)
{
	// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: create/update team via pipe
	char* json = (char*)lParam;
	if (!TheSidesList) { delete[] json; return 0; }

	char name[128] = "", owner[64] = "", home[128] = "";
	int  maxInst = 1;
	MF_JsonGetStr(json, "name",         name,  sizeof(name));
	MF_JsonGetStr(json, "owner",        owner, sizeof(owner));
	MF_JsonGetStr(json, "home",         home,  sizeof(home));
	MF_JsonGetInt(json, "maxInstances", &maxInst);
	if (!name[0]) { delete[] json; return 0; }

	// TheSuperHackers @tweak Nemellud 02/07/2026 EmbeddedMode: undoable via SidesListUndoable
	SidesList newSides;
	newSides = *TheSidesList;
	Int idx = -1;
	Bool exists = FALSE;
	TeamsInfo* existing = newSides.findTeamInfo(AsciiString(name), &idx);

	Dict newDict;
	newDict.setAsciiString(TheKey_teamName,         AsciiString(name));
	newDict.setAsciiString(TheKey_teamOwner,        AsciiString(owner[0] ? owner : "team"));
	if (home[0]) newDict.setAsciiString(TheKey_teamHome, AsciiString(home));
	newDict.setInt(TheKey_teamMaxInstances, maxInst > 0 ? maxInst : 1);

	// TheSuperHackers @bugfix Nemellud 10/06/2026 EmbeddedMode: unit slots were written to
	// made-up dict keys (teamObject_N); the engine and the Team dialog read the well-known
	// keys teamUnitType1..7 / teamUnitMinCount1..7 / teamUnitMaxCount1..7 (1-indexed).
	// Unit slots: unit0_template, unit0_min, unit0_max (max fallback: unit0_count)
	for (int ui = 0; ui < 7; ui++) {
		char tplKey[64], minKey[64], maxKey[64], cntKey[64], tpl[128];
		int minCnt = 0, maxCnt = 1;
		_snprintf(tplKey, sizeof(tplKey), "unit%d_template", ui);
		_snprintf(minKey, sizeof(minKey), "unit%d_min",      ui);
		_snprintf(maxKey, sizeof(maxKey), "unit%d_max",      ui);
		_snprintf(cntKey, sizeof(cntKey), "unit%d_count",    ui);
		if (!MF_JsonGetStr(json, tplKey, tpl, sizeof(tpl)) || !tpl[0]) break;
		MF_JsonGetInt(json, minKey, &minCnt);
		if (!MF_JsonGetInt(json, maxKey, &maxCnt)) MF_JsonGetInt(json, cntKey, &maxCnt);
		char objKey[64], minFKey[64];
		_snprintf(objKey,  sizeof(objKey),  "teamUnitType%d",     ui + 1);
		_snprintf(maxKey,  sizeof(maxKey),  "teamUnitMaxCount%d", ui + 1);
		_snprintf(minFKey, sizeof(minFKey), "teamUnitMinCount%d", ui + 1);
		newDict.setAsciiString(NAMEKEY(objKey), AsciiString(tpl));
		newDict.setInt(NAMEKEY(maxKey),  maxCnt > 0 ? maxCnt : 1);
		newDict.setInt(NAMEKEY(minFKey), minCnt >= 0 ? minCnt : 0);
	}

	// Identity flags
	Bool bSingleton = FALSE, bAutoReinforce = FALSE, bAiRecruit = FALSE, bExecActions = FALSE;
	MF_JsonGetBool(json, "singleton",     &bSingleton);
	MF_JsonGetBool(json, "autoReinforce", &bAutoReinforce);
	MF_JsonGetBool(json, "aiRecruitable", &bAiRecruit);
	MF_JsonGetBool(json, "execActions",   &bExecActions);
	newDict.setBool(TheKey_teamIsSingleton,             bSingleton);
	newDict.setBool(TheKey_teamAutoReinforce,           bAutoReinforce);
	newDict.setBool(TheKey_teamIsAIRecruitable,         bAiRecruit);
	newDict.setBool(TheKey_teamExecutesActionsOnCreate, bExecActions);

	// Production
	int iPriority = 0, iPriSucc = 0, iPriFail = 0, iBuildFrames = 0;
	char sProdCond[128] = "", sDesc[512] = "";
	MF_JsonGetInt(json, "priority",        &iPriority);
	MF_JsonGetInt(json, "prioritySuccess", &iPriSucc);
	MF_JsonGetInt(json, "priorityFailure", &iPriFail);
	MF_JsonGetInt(json, "buildFrames",     &iBuildFrames);
	MF_JsonGetStr(json, "productionCondition", sProdCond, sizeof(sProdCond));
	MF_JsonGetStr(json, "description",         sDesc,     sizeof(sDesc));
	newDict.setInt(TheKey_teamProductionPriority,                iPriority);
	newDict.setInt(TheKey_teamProductionPrioritySuccessIncrease, iPriSucc);
	newDict.setInt(TheKey_teamProductionPriorityFailureDecrease, iPriFail);
	newDict.setInt(TheKey_teamInitialIdleFrames,                 iBuildFrames);
	if (sProdCond[0]) newDict.setAsciiString(TheKey_teamProductionCondition, AsciiString(sProdCond));
	if (sDesc[0])     newDict.setAsciiString(TheKey_teamDescription,         AsciiString(sDesc));

	// Reinforcement
	int iVeterancy = 0;
	Bool bStartsFull = FALSE, bTransportsExit = FALSE;
	char sTransport[128] = "", sReinfOrigin[128] = "";
	MF_JsonGetInt (json, "veterancy",       &iVeterancy);
	MF_JsonGetBool(json, "startsFull",      &bStartsFull);
	MF_JsonGetBool(json, "transportsExit",  &bTransportsExit);
	MF_JsonGetStr (json, "transport",       sTransport,   sizeof(sTransport));
	MF_JsonGetStr (json, "reinforceOrigin", sReinfOrigin, sizeof(sReinfOrigin));
	newDict.setInt (TheKey_teamVeterancy,       iVeterancy);
	newDict.setBool(TheKey_teamStartsFull,      bStartsFull);
	newDict.setBool(TheKey_teamTransportsExit,  bTransportsExit);
	if (sTransport[0])   newDict.setAsciiString(TheKey_teamTransport,            AsciiString(sTransport));
	if (sReinfOrigin[0]) newDict.setAsciiString(TheKey_teamReinforcementOrigin,  AsciiString(sReinfOrigin));

	// Behavior scripts + flags
	int iAggr = 0, iDestrPct = 50;
	Bool bTransRet = FALSE, bAvoid = FALSE, bCommon = FALSE;
	char sCreate[128] = "", sIdle[128] = "", sEnemy[128] = "";
	char sDest[128] = "", sAllClear[128] = "", sUnitDest[128] = "";
	MF_JsonGetInt (json, "aggressiveness",   &iAggr);
	MF_JsonGetInt (json, "destroyedPercent", &iDestrPct);
	MF_JsonGetBool(json, "transportsReturn", &bTransRet);
	MF_JsonGetBool(json, "avoidThreats",     &bAvoid);
	MF_JsonGetBool(json, "commonTarget",     &bCommon);
	MF_JsonGetStr (json, "onCreateScript",   sCreate,   sizeof(sCreate));
	MF_JsonGetStr (json, "onIdleScript",     sIdle,     sizeof(sIdle));
	MF_JsonGetStr (json, "onEnemySighted",   sEnemy,    sizeof(sEnemy));
	MF_JsonGetStr (json, "onDestroyed",      sDest,     sizeof(sDest));
	MF_JsonGetStr (json, "onAllClear",       sAllClear, sizeof(sAllClear));
	MF_JsonGetStr (json, "onUnitDestroyed",  sUnitDest, sizeof(sUnitDest));
	newDict.setInt (TheKey_teamAggressiveness,       iAggr);
	newDict.setReal(TheKey_teamDestroyedThreshold,   iDestrPct / 100.0f);
	newDict.setBool(TheKey_teamTransportsReturn,     bTransRet);
	newDict.setBool(TheKey_teamAvoidThreats,         bAvoid);
	newDict.setBool(TheKey_teamAttackCommonTarget,   bCommon);
	if (sCreate[0])   newDict.setAsciiString(TheKey_teamOnCreateScript,          AsciiString(sCreate));
	if (sIdle[0])     newDict.setAsciiString(TheKey_teamOnIdleScript,            AsciiString(sIdle));
	if (sEnemy[0])    newDict.setAsciiString(TheKey_teamEnemySightedScript,      AsciiString(sEnemy));
	if (sDest[0])     newDict.setAsciiString(TheKey_teamOnDestroyedScript,       AsciiString(sDest));
	if (sAllClear[0]) newDict.setAsciiString(TheKey_teamAllClearScript,          AsciiString(sAllClear));
	if (sUnitDest[0]) newDict.setAsciiString(TheKey_teamOnUnitDestroyedScript,   AsciiString(sUnitDest));

	// Generic script hooks (0-15)
	for (int gi = 0; gi < 16; gi++) {
		char gsKey[32], gsVal[128] = "";
		_snprintf(gsKey, sizeof(gsKey), "genericScript%d", gi);
		if (!MF_JsonGetStr(json, gsKey, gsVal, sizeof(gsVal))) break;
		if (gsVal[0]) {
			char hookKey[40];
			_snprintf(hookKey, sizeof(hookKey), "teamGenericScriptHook%d", gi);
			newDict.setAsciiString(NAMEKEY(hookKey), AsciiString(gsVal));
		}
	}

	// Common Object Properties (use NAMEKEY — TheKey_teamObject* not in scope here)
	int iObjHealth = -1, iObjMaxHP = -1, iObjAggr = -99, iObjVet = -1;
	int iObjWeather = 0, iObjTime = 0, iObjStop = 0, iObjTarget = 0, iObjShroud = 0;
	Bool bObjEn = TRUE, bObjPow = FALSE, bObjUnsell = FALSE;
	Bool bObjSel = TRUE, bObjIndes = FALSE, bObjAiRec = FALSE;
	if (MF_JsonGetInt (json, "objHealth",     &iObjHealth))  newDict.setInt (NAMEKEY("teamObjectInitialHealth"),       iObjHealth);
	if (MF_JsonGetInt (json, "objMaxHP",      &iObjMaxHP))   newDict.setInt (NAMEKEY("teamObjectMaxHPs"),              iObjMaxHP);
	if (MF_JsonGetBool(json, "objEnabled",    &bObjEn))      newDict.setBool(NAMEKEY("teamObjectEnabled"),             bObjEn);
	if (MF_JsonGetBool(json, "objPowered",    &bObjPow))     newDict.setBool(NAMEKEY("teamObjectPowered"),             bObjPow);
	if (MF_JsonGetBool(json, "objUnsellable", &bObjUnsell))  newDict.setBool(NAMEKEY("teamObjectUnsellable"),          bObjUnsell);
	if (MF_JsonGetBool(json, "objSelectable", &bObjSel))     newDict.setBool(NAMEKEY("teamObjectSelectable"),          bObjSel);
	if (MF_JsonGetBool(json, "objIndestructible", &bObjIndes)) newDict.setBool(NAMEKEY("teamObjectIndestructible"),    bObjIndes);
	if (MF_JsonGetBool(json, "objAiRecruit",  &bObjAiRec))   newDict.setBool(NAMEKEY("teamObjectRecruitableAI"),       bObjAiRec);
	if (MF_JsonGetInt (json, "objAggr",       &iObjAggr))    newDict.setInt (NAMEKEY("teamObjectAggressiveness"),      iObjAggr);
	if (MF_JsonGetInt (json, "objVet",        &iObjVet))     newDict.setInt (NAMEKEY("teamObjectVeterancy"),           iObjVet);
	if (MF_JsonGetInt (json, "objWeather",    &iObjWeather))  newDict.setInt (NAMEKEY("teamObjectWeather"),             iObjWeather);
	if (MF_JsonGetInt (json, "objTime",       &iObjTime))     newDict.setInt (NAMEKEY("teamObjectTime"),                iObjTime);
	if (MF_JsonGetInt (json, "objStop",       &iObjStop))     newDict.setReal(NAMEKEY("teamObjectStoppingDistance"),   (Real)iObjStop);
	if (MF_JsonGetInt (json, "objTarget",     &iObjTarget))   newDict.setInt (NAMEKEY("teamObjectTargettingDistance"), iObjTarget);
	if (MF_JsonGetInt (json, "objShroud",     &iObjShroud))   newDict.setInt (NAMEKEY("teamObjectShroudClearingDistance"), iObjShroud);

	if (existing) {
		*existing->getDict() = newDict;
	} else {
		newSides.addTeam(&newDict);
	}

	MF_CommitSidesUndoable(newSides);
	delete[] json;
	return 0;
}

LRESULT CMainFrame::OnWbDelTeam(WPARAM, LPARAM lParam)
{
	// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: delete team via pipe
	// TheSuperHackers @tweak Nemellud 02/07/2026 EmbeddedMode: undoable via SidesListUndoable
	char* json = (char*)lParam;
	if (TheSidesList) {
		char name[128] = "";
		MF_JsonGetStr(json, "name", name, sizeof(name));
		SidesList newSides;
		newSides = *TheSidesList;
		Int idx = -1;
		newSides.findTeamInfo(AsciiString(name), &idx);
		if (idx >= 0) {
			newSides.removeTeam(idx);
			MF_CommitSidesUndoable(newSides);
		}
	}
	delete[] json;
	return 0;
}

LRESULT CMainFrame::OnWbSetScript(WPARAM wParam, LPARAM lParam)
{
	// TheSuperHackers @feature Nemellud 31/05/2026 EmbeddedMode: sync SendMessage; player fallback
	char* buf    = (char*)lParam;
	int   bufLen = (int)wParam;
	if (!TheSidesList || !buf) { if (bufLen > 0) _snprintf(buf, bufLen, "{\"ok\":false}"); return 0; }

	char playerName[64] = "", groupName[128] = "", scriptName[128] = "";
	MF_JsonGetStr(buf, "player", playerName, sizeof(playerName));
	MF_JsonGetStr(buf, "group",  groupName,  sizeof(groupName));
	MF_JsonGetStr(buf, "name",   scriptName, sizeof(scriptName));
	if (!scriptName[0]) { if (bufLen > 0) _snprintf(buf, bufLen, "{\"ok\":false}"); return 0; }

	// Default group name to script name if not specified
	if (!groupName[0]) _snprintf(groupName, sizeof(groupName), "%s", scriptName);

	// TheSuperHackers @tweak Nemellud 02/07/2026 EmbeddedMode: undoable via SidesListUndoable
	SidesList newSides;
	newSides = *TheSidesList;
	SidesInfo* side = newSides.findSideInfo(AsciiString(playerName));
	// Fallback: empty/unknown name → first player
	if (!side) {
		int n = newSides.getNumSides();
		for (int i = 0; i < n; i++) { SidesInfo* s = newSides.getSideInfo(i); if (s) { side = s; break; } }
	}
	if (!side) { if (bufLen > 0) _snprintf(buf, bufLen, "{\"ok\":false,\"error\":\"no player\"}"); return 0; }

	ScriptList* sl = side->getScriptList();
	if (!sl) { sl = newInstance(ScriptList); side->setScriptList(sl); }
	if (!sl) { if (bufLen > 0) _snprintf(buf, bufLen, "{\"ok\":false,\"error\":\"no script list\"}"); return 0; }

	// Find or create group
	ScriptGroup* group = nullptr;
	for (ScriptGroup* g = sl->getScriptGroup(); g; g = g->getNext()) {
		if (strcmp(g->getName().str(), groupName) == 0) { group = g; break; }
	}
	if (!group) {
		group = newInstance(ScriptGroup);
		group->setName(AsciiString(groupName));
		group->setActive(TRUE);
		sl->addGroup(group, 0);
	}

	// Find existing script (to replace) or create new
	Script* script = nullptr;
	for (Script* s = group->getScript(); s; s = s->getNext()) {
		if (strcmp(s->getName().str(), scriptName) == 0) { script = s; break; }
	}
	bool isNew = (script == nullptr);
	if (isNew) script = newInstance(Script);

	// Set script properties
	script->setName(AsciiString(scriptName));
	bool bval = true;
	if (MF_JsonGetBool(buf, "active",     &bval)) script->setActive(bval);     else script->setActive(true);
	if (MF_JsonGetBool(buf, "easy",       &bval)) script->setEasy(bval);       else script->setEasy(true);
	if (MF_JsonGetBool(buf, "normal",     &bval)) script->setNormal(bval);     else script->setNormal(true);
	if (MF_JsonGetBool(buf, "hard",       &bval)) script->setHard(bval);       else script->setHard(true);
	if (MF_JsonGetBool(buf, "oneShot",    &bval)) script->setOneShot(bval);    else script->setOneShot(false);
	if (MF_JsonGetBool(buf, "subroutine", &bval)) script->setSubroutine(bval); else script->setSubroutine(false);

	// Parse conditions
	int condCount = 0; MF_JsonGetInt(buf, "condCount", &condCount);
	OrCondition* orList = MF_ParseConditions(buf, condCount);
	if (!orList) {
		Condition* defCond = newInstance(Condition)(Condition::CONDITION_TRUE);
		orList = newInstance(OrCondition);
		orList->setFirstAndCondition(defCond);
	}
	script->setOrCondition(orList);

	// Parse true actions
	int actCount = 0; MF_JsonGetInt(buf, "actCount", &actCount);
	ScriptAction* trueActs = MF_ParseActions(buf, actCount, "act");
	if (!trueActs) trueActs = newInstance(ScriptAction)(ScriptAction::NO_OP);
	script->setAction(trueActs);

	// Parse false actions
	int actFalseCount = 0; MF_JsonGetInt(buf, "actFalseCount", &actFalseCount);
	ScriptAction* falseActs = MF_ParseActions(buf, actFalseCount, "actF");
	script->setFalseAction(falseActs);

	if (isNew) group->addScript(script, 0);

	MF_CommitSidesUndoable(newSides);

	if (bufLen > 0) _snprintf(buf, bufLen, "{\"ok\":true}");
	return 0;
}

LRESULT CMainFrame::OnWbDelScript(WPARAM wParam, LPARAM lParam)
{
	// TheSuperHackers @feature Nemellud 31/05/2026 EmbeddedMode: sync SendMessage
	char* buf    = (char*)lParam;
	int   bufLen = (int)wParam;
	bool  ok     = false;

	if (TheSidesList && buf) {
		char playerName[64] = "", groupName[128] = "", scriptName[128] = "";
		MF_JsonGetStr(buf, "player", playerName, sizeof(playerName));
		MF_JsonGetStr(buf, "group",  groupName,  sizeof(groupName));
		MF_JsonGetStr(buf, "name",   scriptName, sizeof(scriptName));

		// TheSuperHackers @tweak Nemellud 02/07/2026 EmbeddedMode: undoable via SidesListUndoable
		SidesList newSides;
		newSides = *TheSidesList;
		SidesInfo* side = newSides.findSideInfo(AsciiString(playerName));
		if (!side) {
			int n = newSides.getNumSides();
			for (int i = 0; i < n; i++) { SidesInfo* s = newSides.getSideInfo(i); if (s) { side = s; break; } }
		}
		if (side) {
			ScriptList* sl = side->getScriptList();
			if (sl) {
				for (ScriptGroup* g = sl->getScriptGroup(); g; g = g->getNext()) {
					if (strcmp(g->getName().str(), groupName) != 0) continue;
					for (Script* s = g->getScript(); s; s = s->getNext()) {
						if (strcmp(s->getName().str(), scriptName) == 0) {
							g->deleteScript(s);
							ok = true;
							break;
						}
					}
					break;
				}
			}
		}
		if (ok) MF_CommitSidesUndoable(newSides);
	}
	if (bufLen > 0) _snprintf(buf, bufLen, "{\"ok\":%s}", ok ? "true" : "false");
	return 0;
}

LRESULT CMainFrame::OnWbSetGroup(WPARAM wParam, LPARAM lParam)
{
	// TheSuperHackers @feature Nemellud 31/05/2026 EmbeddedMode: sync SendMessage; fallback to first player for empty name
	char* buf    = (char*)lParam;
	int   bufLen = (int)wParam;
	bool  ok     = false;

	if (TheSidesList && buf) {
		char playerName[64] = "", groupName[128] = "";
		bool active = true, subroutine = false;
		MF_JsonGetStr(buf, "player",     playerName, sizeof(playerName));
		MF_JsonGetStr(buf, "name",       groupName,  sizeof(groupName));
		MF_JsonGetBool(buf, "active",     &active);
		MF_JsonGetBool(buf, "subroutine", &subroutine);

		// TheSuperHackers @tweak Nemellud 02/07/2026 EmbeddedMode: undoable via SidesListUndoable
		SidesList newSides;
		newSides = *TheSidesList;
		SidesInfo* side = newSides.findSideInfo(AsciiString(playerName));
		// Fallback: if not found (e.g. empty name = neutral player), use first side
		if (!side) {
			int n = newSides.getNumSides();
			for (int i = 0; i < n; i++) {
				SidesInfo* s = newSides.getSideInfo(i);
				if (s) { side = s; break; }
			}
		}
		if (side) {
			ScriptList* sl = side->getScriptList();
			if (!sl) { sl = newInstance(ScriptList); side->setScriptList(sl); }
			if (sl) {
				ScriptGroup* group = nullptr;
				for (ScriptGroup* g = sl->getScriptGroup(); g; g = g->getNext()) {
					if (strcmp(g->getName().str(), groupName) == 0) { group = g; break; }
				}
				if (!group) {
					group = newInstance(ScriptGroup);
					group->setName(AsciiString(groupName));
					group->setSubroutine(subroutine ? TRUE : FALSE);
					sl->addGroup(group, 0);
				}
				group->setActive(active ? TRUE : FALSE);
				ok = true;
			}
		}
		if (ok) MF_CommitSidesUndoable(newSides);
	}
	if (bufLen > 0) _snprintf(buf, bufLen, "{\"ok\":%s}", ok ? "true" : "false");
	return 0;
}

LRESULT CMainFrame::OnWbAddSkirmish(WPARAM wParam, LPARAM lParam)
{
	// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: add standard skirmish players + validateSides
	// Replicates PlayerListDlg::OnAddskirmishplayers()
	char* responseBuf = (char*)lParam;
	int   responseBufLen = (int)wParam;
	if (!TheSidesList) {
		if (responseBuf) _snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"no sides list\"}");
		return 0;
	}

	struct SkirmishPlayer { const char* faction; const char* name; };
	static const SkirmishPlayer SKIRMISH_PLAYERS[] = {
		{ "FactionCivilian",                  "PlyrCivilian"                   },
		{ "FactionAmerica",                   "SkirmishAmerica"                },
		{ "FactionChina",                     "SkirmishChina"                  },
		{ "FactionGLA",                       "SkirmishGLA"                    },
		{ "FactionAmericaAirForceGeneral",    "SkirmishAmericaAirForceGeneral" },
		{ "FactionAmericaLaserGeneral",       "SkirmishAmericaLaserGeneral"    },
		{ "FactionAmericaSuperWeaponGeneral", "SkirmishAmericaSuperWeaponGeneral" },
		{ "FactionChinaTankGeneral",          "SkirmishChinaTankGeneral"       },
		{ "FactionChinaNukeGeneral",          "SkirmishChinaNukeGeneral"       },
		{ "FactionChinaInfantryGeneral",      "SkirmishChinaInfantryGeneral"   },
		{ "FactionGLADemolitionGeneral",      "SkirmishGLADemolitionGeneral"   },
		{ "FactionGLAToxinGeneral",           "SkirmishGLAToxinGeneral"        },
		{ "FactionGLAStealthGeneral",         "SkirmishGLAStealthGeneral"      },
	};

	// TheSuperHackers @tweak Nemellud 02/07/2026 EmbeddedMode: undoable via SidesListUndoable
	SidesList newSides;
	newSides = *TheSidesList;
	int added = 0;
	for (int i = 0; i < (int)(sizeof(SKIRMISH_PLAYERS) / sizeof(SKIRMISH_PLAYERS[0])); i++) {
		const char* pName    = SKIRMISH_PLAYERS[i].name;
		const char* pFaction = SKIRMISH_PLAYERS[i].faction;
		if (newSides.findSideInfo(AsciiString(pName))) continue;

		Dict d;
		UnicodeString uName;
		uName.translate(AsciiString(pName));
		d.setAsciiString  (TheKey_playerName,       AsciiString(pName));
		d.setBool         (TheKey_playerIsHuman,     false);
		d.setUnicodeString(TheKey_playerDisplayName, uName);
		d.setAsciiString  (TheKey_playerFaction,     AsciiString(pFaction));
		d.setAsciiString  (TheKey_playerEnemies,     AsciiString(""));
		d.setAsciiString  (TheKey_playerAllies,      AsciiString(""));
		newSides.addSide(&d);
		added++;
	}

	// Commit when players were added, or when validateSides had repairs to apply
	// (the old code always ran validateSides on TheSidesList directly)
	Bool fixed = newSides.validateSides();
	if (added > 0 || fixed) {
		MF_CommitSidesUndoable(newSides);
	}

	if (responseBuf)
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true,\"added\":%d}", added);
	return 0;
}

// TheSuperHackers @feature Nemellud 07/06/2026 EmbeddedMode: place road segment via pipe
static bool MF_JsonGetFloat(const char* json, const char* key, float* out) {
	char pat[64]; _snprintf(pat, sizeof(pat), "\"%s\"", key);
	const char* p = strstr(json, pat); if (!p) return false;
	p += strlen(pat); while (*p == ':' || *p == ' ') p++;
	return sscanf(p, "%f", out) == 1;
}

LRESULT CMainFrame::OnWbPlaceRoad(WPARAM, LPARAM lp)
{
	char* json = reinterpret_cast<char*>(lp);
	if (!json) return 0;
	float x1=0, y1=0, x2=0, y2=0;
	char roadType[128] = "TwoLane";
	int corner = 0;
	MF_JsonGetFloat(json, "x1", &x1); MF_JsonGetFloat(json, "y1", &y1);
	MF_JsonGetFloat(json, "x2", &x2); MF_JsonGetFloat(json, "y2", &y2);
	MF_JsonGetStr(json, "type", roadType, sizeof(roadType));
	MF_JsonGetInt(json, "corner", &corner);
	delete[] json;

	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc) return 0;

	Coord3D loc1 = {x1, y1, 0.0f};
	Coord3D loc2 = {x2, y2, 0.0f};
	AsciiString roadName(roadType);

	MapObject* pNew1 = newInstance(MapObject)(loc1, roadName, 0.0f, 0, nullptr, nullptr);
	MapObject* pNew2 = newInstance(MapObject)(loc2, roadName, 0.0f, 0, nullptr, nullptr);
	pNew1->setColor(RGB(255,255,0));
	pNew2->setColor(RGB(255,255,0));
	pNew1->setFlag(FLAG_ROAD_POINT1);
	pNew2->setFlag(FLAG_ROAD_POINT2);
	if (corner == 1) {
		pNew1->setFlag(FLAG_ROAD_CORNER_ANGLED); pNew2->setFlag(FLAG_ROAD_CORNER_ANGLED);
	} else if (corner == 2) {
		pNew1->setFlag(FLAG_ROAD_CORNER_TIGHT); pNew2->setFlag(FLAG_ROAD_CORNER_TIGHT);
	}
	pNew1->getProperties()->setAsciiString(TheKey_originalOwner, "team");
	pNew2->getProperties()->setAsciiString(TheKey_originalOwner, "team");
	pNew1->setNextMap(pNew2);

	AddObjectUndoable* pUndo = new AddObjectUndoable(pDoc, pNew1);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo);
	pDoc->updateAllViews();
	return 1;
}

// TheSuperHackers @feature Nemellud 07/06/2026 EmbeddedMode: list all road segments via pipe
LRESULT CMainFrame::OnWbListRoads(WPARAM wParam, LPARAM lParam)
{
	char* buf = reinterpret_cast<char*>(lParam);
	int maxLen = (int)wParam;
	if (!buf || maxLen < 32) return 0;

	int pos = 0;
	pos += _snprintf(buf + pos, maxLen - pos, "{\"ok\":true,\"roads\":[");
	bool first = true;
	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext()) {
		if (!p->getFlag(FLAG_ROAD_POINT1)) continue;
		MapObject* p2 = p->getNext();
		if (!p2 || !p2->getFlag(FLAG_ROAD_POINT2)) continue;
		const Coord3D* l1 = p->getLocation();
		const Coord3D* l2 = p2->getLocation();
		if (!first && pos < maxLen - 2) buf[pos++] = ',';
		pos += _snprintf(buf + pos, maxLen - pos,
			"{\"type\":\"%s\",\"x1\":%.1f,\"y1\":%.1f,\"x2\":%.1f,\"y2\":%.1f,\"flags\":%d}",
			p->getName().str(),
			l1 ? l1->x : 0.f, l1 ? l1->y : 0.f,
			l2 ? l2->x : 0.f, l2 ? l2->y : 0.f,
			(int)p->getFlags());
		first = false;
	}
	if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; buf[pos] = '\0'; }
	return 0;
}

// TheSuperHackers @feature Nemellud 07/06/2026 EmbeddedMode: delete road or bridge nearest to coords
// Coords in g_wbDelRoadReq (filled by pipe thread before SendMessage); wParam=bufLen, lParam=char* buf.
extern struct WbDelRoadReq { float x1, y1; bool bridge; } g_wbDelRoadReq;
LRESULT CMainFrame::OnWbDelRoad(WPARAM wParam, LPARAM lParam)
{
	float tx = g_wbDelRoadReq.x1, ty = g_wbDelRoadReq.y1;
	bool isBridge = g_wbDelRoadReq.bridge;
	Int flagP1 = isBridge ? FLAG_BRIDGE_POINT1 : FLAG_ROAD_POINT1;
	Int flagP2 = isBridge ? FLAG_BRIDGE_POINT2 : FLAG_ROAD_POINT2;

	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc) return 0;

	MapObject* best1 = nullptr;
	float bestDist = 1e9f;
	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext()) {
		if (!p->getFlag(flagP1)) continue;
		MapObject* p2 = p->getNext();
		if (!p2 || !p2->getFlag(flagP2)) continue;
		const Coord3D* l = p->getLocation();
		if (!l) continue;
		float dx = l->x - tx, dy = l->y - ty;
		float d = dx*dx + dy*dy;
		if (d < bestDist) { bestDist = d; best1 = p; }
	}
	if (!best1) return 0;
	MapObject* best2 = best1->getNext();
	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext())
		p->setSelected(false);
	best1->setSelected(true);
	if (best2) best2->setSelected(true);
	DeleteObjectUndoable* pUndo = new DeleteObjectUndoable(pDoc);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo);
	pDoc->updateAllViews();
	return 1;
}

// TheSuperHackers @feature Nemellud 07/06/2026 EmbeddedMode: select road or bridge in WB from UI list
LRESULT CMainFrame::OnWbSelRoad(WPARAM, LPARAM lp)
{
	char* json = reinterpret_cast<char*>(lp);
	if (!json) return 0;
	float tx = 0, ty = 0; int bridge = 0;
	MF_JsonGetFloat(json, "x1", &tx); MF_JsonGetFloat(json, "y1", &ty);
	MF_JsonGetInt(json, "bridge", &bridge);
	delete[] json;

	Int flagP1 = bridge ? FLAG_BRIDGE_POINT1 : FLAG_ROAD_POINT1;
	Int flagP2 = bridge ? FLAG_BRIDGE_POINT2 : FLAG_ROAD_POINT2;

	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc) return 0;

	MapObject* best1 = nullptr;
	float bestDist = 1e9f;
	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext()) {
		if (!p->getFlag(flagP1)) continue;
		MapObject* p2 = p->getNext();
		if (!p2 || !p2->getFlag(flagP2)) continue;
		const Coord3D* l = p->getLocation();
		if (!l) continue;
		float dx = l->x - tx, dy = l->y - ty;
		float d = dx*dx + dy*dy;
		if (d < bestDist) { bestDist = d; best1 = p; }
	}
	if (!best1) return 0;

	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext())
		p->setSelected(false);
	best1->setSelected(true);
	MapObject* best2 = best1->getNext();
	if (best2 && best2->getFlag(flagP2)) best2->setSelected(true);
	pDoc->updateAllViews();
	return 1;
}

// TheSuperHackers @feature Nemellud 07/06/2026 EmbeddedMode: place bridge segment via pipe
LRESULT CMainFrame::OnWbPlaceBridge(WPARAM, LPARAM lp)
{
	char* json = reinterpret_cast<char*>(lp);
	if (!json) return 0;
	float x1=0, y1=0, x2=0, y2=0;
	char bridgeType[128] = "TwoLane";
	MF_JsonGetFloat(json, "x1", &x1); MF_JsonGetFloat(json, "y1", &y1);
	MF_JsonGetFloat(json, "x2", &x2); MF_JsonGetFloat(json, "y2", &y2);
	MF_JsonGetStr(json, "type", bridgeType, sizeof(bridgeType));
	delete[] json;

	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc) return 0;

	Coord3D loc1 = {x1, y1, 0.0f};
	Coord3D loc2 = {x2, y2, 0.0f};
	AsciiString bName(bridgeType);

	// Always create a two-point bridge (POINT1 + POINT2).
	// Landmark/destructible bridges (AsianFloodBridge etc.) share their name with roads.ini Bridge
	// entries but belong in the Objects panel — placing them here via two-point is correct behavior.
	int bridgeN = 0;
	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext())
		if (p->getFlag(FLAG_BRIDGE_POINT1) && p->getName() == bName) bridgeN++;

	MapObject* pNew1 = newInstance(MapObject)(loc1, bName, 0.0f, 0, nullptr, nullptr);
	MapObject* pNew2 = newInstance(MapObject)(loc2, bName, 0.0f, 0, nullptr, nullptr);
	pNew1->setColor(RGB(255,255,0));
	pNew2->setColor(RGB(255,255,0));
	pNew1->setFlag(FLAG_BRIDGE_POINT1);
	pNew2->setFlag(FLAG_BRIDGE_POINT2);
	pNew1->getProperties()->setAsciiString(TheKey_originalOwner, "team");
	pNew2->getProperties()->setAsciiString(TheKey_originalOwner, "team");

	char autoName[128];
	_snprintf(autoName, sizeof(autoName), "%s_%02d", bName.str(), bridgeN + 1);
	pNew1->getProperties()->setAsciiString(TheKey_objectName, AsciiString(autoName));

	pNew1->setNextMap(pNew2);
	AddObjectUndoable* pUndo = new AddObjectUndoable(pDoc, pNew1);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo);

	pDoc->updateAllViews();
	return 1;
}

// TheSuperHackers @feature Nemellud 07/06/2026 EmbeddedMode: list all bridge segments via pipe
LRESULT CMainFrame::OnWbListBridges(WPARAM wParam, LPARAM lParam)
{
	char* buf = reinterpret_cast<char*>(lParam);
	int maxLen = (int)wParam;
	if (!buf || maxLen < 32) return 0;

	int pos = 0;
	pos += _snprintf(buf + pos, maxLen - pos, "{\"ok\":true,\"bridges\":[");
	bool first = true;

	// Two-point bridges (POINT1 + POINT2 pairs)
	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext()) {
		if (!p->getFlag(FLAG_BRIDGE_POINT1)) continue;
		MapObject* p2 = p->getNext();
		if (!p2 || !p2->getFlag(FLAG_BRIDGE_POINT2)) continue;
		const Coord3D* l1 = p->getLocation();
		const Coord3D* l2 = p2->getLocation();
		if (!first && pos < maxLen - 2) buf[pos++] = ',';
		char objName[128] = "";
		const Dict* bd = p->getProperties();
		if (bd) {
			Bool bex = FALSE;
			AsciiString sn = bd->getAsciiString(TheKey_objectName, &bex);
			if (bex) strncpy(objName, sn.str(), sizeof(objName) - 1);
		}
		pos += _snprintf(buf + pos, maxLen - pos,
			"{\"type\":\"%s\",\"name\":\"%s\",\"x1\":%.1f,\"y1\":%.1f,\"x2\":%.1f,\"y2\":%.1f}",
			p->getName().str(), objName,
			l1 ? l1->x : 0.f, l1 ? l1->y : 0.f,
			l2 ? l2->x : 0.f, l2 ? l2->y : 0.f);
		first = false;
	}

	// Single-point (landmark) bridges placed via Objects panel — isBridge() == true
	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext()) {
		if (p->getFlag(FLAG_BRIDGE_POINT1) || p->getFlag(FLAG_BRIDGE_POINT2)) continue;
		const ThingTemplate* tt = p->getThingTemplate();
		if (!tt || !tt->isBridge()) continue;
		const Coord3D* l1 = p->getLocation();
		if (!first && pos < maxLen - 2) buf[pos++] = ',';
		char objName[128] = "";
		const Dict* bd = p->getProperties();
		if (bd) {
			Bool bex = FALSE;
			AsciiString sn = bd->getAsciiString(TheKey_objectName, &bex);
			if (bex) strncpy(objName, sn.str(), sizeof(objName) - 1);
		}
		pos += _snprintf(buf + pos, maxLen - pos,
			"{\"type\":\"%s\",\"name\":\"%s\",\"x1\":%.1f,\"y1\":%.1f,\"x2\":%.1f,\"y2\":%.1f,\"landmark\":true}",
			p->getName().str(), objName,
			l1 ? l1->x : 0.f, l1 ? l1->y : 0.f,
			l1 ? l1->x : 0.f, l1 ? l1->y : 0.f);
		first = false;
	}

	if (pos < maxLen - 2) { buf[pos++] = ']'; buf[pos++] = '}'; buf[pos] = '\0'; }
	return 0;
}

// TheSuperHackers @feature Nemellud 07/06/2026 EmbeddedMode: set objectName on bridge POINT1 by coordinates
LRESULT CMainFrame::OnWbSetBridgeName(WPARAM, LPARAM lp)
{
	char* json = reinterpret_cast<char*>(lp);
	if (!json) return 0;
	float tx = 0, ty = 0;
	char name[128] = "";
	MF_JsonGetFloat(json, "x1", &tx);
	MF_JsonGetFloat(json, "y1", &ty);
	MF_JsonGetStr(json, "name", name, sizeof(name));
	delete[] json;

	MapObject* best1 = nullptr;
	float bestDist = 1e9f;
	for (MapObject* p = MapObject::getFirstMapObject(); p; p = p->getNext()) {
		if (!p->getFlag(FLAG_BRIDGE_POINT1)) continue;
		const Coord3D* l = p->getLocation();
		if (!l) continue;
		float dx = l->x - tx, dy = l->y - ty;
		float d = dx*dx + dy*dy;
		if (d < bestDist) { bestDist = d; best1 = p; }
	}
	if (!best1) return 0;

	Dict* d = best1->getProperties();
	if (!d) return 0;
	d->setAsciiString(TheKey_objectName, AsciiString(name));

	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (pDoc) pDoc->updateAllViews();
	return 1;
}

// TheSuperHackers @feature Nemellud 07/06/2026 EmbeddedMode: set road type/corner in RoadOptions (activation via separate 'tool' pipe command)
LRESULT CMainFrame::OnWbSetRoadTool(WPARAM, LPARAM lp)
{
	char* json = reinterpret_cast<char*>(lp);
	if (!json) return 0;
	char roadName[128] = "";
	int angled = 0, tight = 0, bridge = 0;
	MF_JsonGetStr(json, "name",   roadName, sizeof(roadName));
	MF_JsonGetInt(json, "angled", &angled);
	MF_JsonGetInt(json, "tight",  &tight);
	MF_JsonGetInt(json, "bridge", &bridge);
	delete[] json;

	RoadOptions::setFromPipe(roadName[0] ? roadName : nullptr, !!angled, !!tight, !!bridge);
	return 1;
}

// TheSuperHackers @feature Nemellud 10/06/2026 EmbeddedMode: save map to explicit path without native dialog
LRESULT CMainFrame::OnWbSaveToPath(WPARAM, LPARAM)
{
	CWorldBuilderDoc* pDoc = (CWorldBuilderDoc*)GetActiveDocument();
	if (!pDoc || g_wbSavePath[0] == '\0') return 0;
	BOOL ok = pDoc->SaveToPath(g_wbSavePath);
	g_wbSavePath[0] = '\0';
	return ok ? 1 : 0;
}

// TheSuperHackers @feature Nemellud 10/06/2026 EmbeddedMode: read global lighting state via pipe.
// Returns timeOfDay plus per-target (terrain/objects) arrays of 3 lights (sun, accent1, accent2)
// with ambient/diffuse as 0-255 ints and the raw light direction vector.
LRESULT CMainFrame::OnWbGetLighting(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int maxLen = (int)wParam;
	if (!buf || maxLen < 256) return 0;

	int tod = (int)TheGlobalData->m_timeOfDay;
	if (tod < TIME_OF_DAY_FIRST || tod >= TIME_OF_DAY_COUNT) tod = TIME_OF_DAY_FIRST;

	int pos = 0;
	pos += _snprintf(buf + pos, maxLen - pos, "{\"ok\":true,\"timeOfDay\":%d", tod);
	const char* targetNames[2] = { "terrain", "objects" };
	for (int s = 0; s < 2 && pos < maxLen - 256; s++) {
		pos += _snprintf(buf + pos, maxLen - pos, ",\"%s\":[", targetNames[s]);
		for (int li = 0; li < 3 && pos < maxLen - 200; li++) {
			const GlobalData::TerrainLighting* tl = (s == 0)
				? &TheGlobalData->m_terrainLighting[tod][li]
				: &TheGlobalData->m_terrainObjectsLighting[tod][li];
			pos += _snprintf(buf + pos, maxLen - pos,
				"%s{\"ambient\":[%d,%d,%d],\"diffuse\":[%d,%d,%d],\"pos\":[%.4f,%.4f,%.4f]}",
				li ? "," : "",
				(int)(tl->ambient.red * 255.0f + 0.5f), (int)(tl->ambient.green * 255.0f + 0.5f), (int)(tl->ambient.blue * 255.0f + 0.5f),
				(int)(tl->diffuse.red * 255.0f + 0.5f), (int)(tl->diffuse.green * 255.0f + 0.5f), (int)(tl->diffuse.blue * 255.0f + 0.5f),
				tl->lightPos.x, tl->lightPos.y, tl->lightPos.z);
		}
		if (pos < maxLen - 1) buf[pos++] = ']';
	}
	if (pos < maxLen - 2) { buf[pos++] = '}'; buf[pos] = '\0'; }
	return 0;
}

// TheSuperHackers @feature Nemellud 10/06/2026 EmbeddedMode: set global lighting via pipe.
// Json fields (all optional): timeOfDay 1-4 (switches and re-renders), target "terrain"/"objects"/"both"
// (default both), light 0-2, ambR/ambG/ambB and difR/difG/difB as 0-255 ints, azimuth 0-360 and
// elevation -90..90 in degrees (same direction math as GlobalLightOptions::showLightFeedback).
LRESULT CMainFrame::OnWbSetLighting(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int maxLen = (int)wParam;
	if (!buf || maxLen < 64) return 0;
	char json[2048];
	strncpy(json, buf, sizeof(json) - 1); json[sizeof(json) - 1] = '\0';

	WbView3d* pView = CWorldBuilderDoc::GetActive3DView();
	if (!pView) pView = WbView3d::s_instance;
	if (!pView) { _snprintf(buf, maxLen, "{\"ok\":false,\"error\":\"no 3d view\"}"); return 0; }

	bool todChanged = false;
	int tod;
	if (MF_JsonGetInt(json, "timeOfDay", &tod)) {
		if (tod < TIME_OF_DAY_FIRST) tod = TIME_OF_DAY_FIRST;
		if (tod >= TIME_OF_DAY_COUNT) tod = TIME_OF_DAY_COUNT - 1;
		TheWritableGlobalData->m_timeOfDay = (TimeOfDay)tod;
		TheWritableGlobalData->setTimeOfDay((TimeOfDay)tod);
		pView->resetRenderObjects();
		todChanged = true;
	}

	int curTod = (int)TheGlobalData->m_timeOfDay;
	if (curTod < TIME_OF_DAY_FIRST || curTod >= TIME_OF_DAY_COUNT) curTod = TIME_OF_DAY_FIRST;

	int light = 0;
	MF_JsonGetInt(json, "light", &light);
	if (light < 0) light = 0;
	if (light > 2) light = 2;

	char targetStr[16] = "";
	int target = GlobalLightOptions::K_BOTH;
	if (MF_JsonGetStr(json, "target", targetStr, sizeof(targetStr))) {
		if (strcmp(targetStr, "terrain") == 0) target = GlobalLightOptions::K_TERRAIN;
		else if (strcmp(targetStr, "objects") == 0) target = GlobalLightOptions::K_OBJECTS;
	}

	// Start from the current values of the (first) targeted array so partial updates work
	GlobalData::TerrainLighting tl = (target == GlobalLightOptions::K_OBJECTS)
		? TheGlobalData->m_terrainObjectsLighting[curTod][light]
		: TheGlobalData->m_terrainLighting[curTod][light];

	int v;
	bool changed = false;
	if (MF_JsonGetInt(json, "ambR", &v)) { tl.ambient.red   = v / 255.0f; changed = true; }
	if (MF_JsonGetInt(json, "ambG", &v)) { tl.ambient.green = v / 255.0f; changed = true; }
	if (MF_JsonGetInt(json, "ambB", &v)) { tl.ambient.blue  = v / 255.0f; changed = true; }
	if (MF_JsonGetInt(json, "difR", &v)) { tl.diffuse.red   = v / 255.0f; changed = true; }
	if (MF_JsonGetInt(json, "difG", &v)) { tl.diffuse.green = v / 255.0f; changed = true; }
	if (MF_JsonGetInt(json, "difB", &v)) { tl.diffuse.blue  = v / 255.0f; changed = true; }

	int az, el;
	if (MF_JsonGetInt(json, "azimuth", &az) && MF_JsonGetInt(json, "elevation", &el)) {
		double azr = az * PI / 180.0;
		double elr = el * PI / 180.0;
		tl.lightPos.x = (Real)(sin(PI / 2.0 + elr) * cos(azr));
		tl.lightPos.y = (Real)(sin(PI / 2.0 + elr) * sin(azr));
		tl.lightPos.z = (Real)(cos(PI / 2.0 + elr));
		changed = true;
	}

	if (changed)
		pView->setLighting(&tl, target, light);

	// TheSuperHackers @bugfix Nemellud 11/06/2026 EmbeddedMode: setLighting only flags the terrain
	// dirty; in embedded mode the change stayed invisible until a 2D/3D toggle forced a rebuild.
	// refreshLightingNow re-lights the terrain and renders synchronously right now.
	if (changed || todChanged)
		pView->refreshLightingNow();

	_snprintf(buf, maxLen, "{\"ok\":true}");
	return 0;
}

// TheSuperHackers @feature Nemellud 10/06/2026 EmbeddedMode: restore EA factory-default lighting.
LRESULT CMainFrame::OnWbResetLighting(WPARAM wParam, LPARAM lParam)
{
	char* buf = (char*)lParam;
	int maxLen = (int)wParam;
	GlobalLightOptions::resetLightingToDefaults();
	// Force the terrain to re-light + render immediately (see OnWbSetLighting note).
	WbView3d* pView = CWorldBuilderDoc::GetActive3DView();
	if (!pView) pView = WbView3d::s_instance;
	if (pView) pView->refreshLightingNow();
	if (buf && maxLen > 16) _snprintf(buf, maxLen, "{\"ok\":true}");
	return 0;
}

