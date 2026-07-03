/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// TheSuperHackers @feature Nemellud 24/05/2026 EmbeddedMode: named pipe server for Electron shell

#pragma once

#ifndef WBPIPESERVER_H
#define WBPIPESERVER_H

#include <windows.h>

// Named pipe server — listens on \\.\pipe\wb-engine
// Receives newline-delimited JSON commands from the Electron shell.
// Responds with JSON on the same connection.

class WbPipeServer
{
public:
	static void Start(HWND mainHwnd);
	static void Stop();

	// State mirrors — updated on main thread, read on pipe thread
	static volatile bool s_viewTopDown;    // true = 2D top-down projection
	static volatile bool s_sfToolActive;   // legacy compat
	static char s_activeTool[32];          // name of active tool
	static void setViewTopDown(bool v);
	static void setSfToolActive(bool v);
	static void setActiveTool(const char* name);

private:
	static DWORD WINAPI ThreadProc(LPVOID param);
	static void         HandleClient(HANDLE pipe, HWND hwnd);
	static bool         DispatchCommand(const char* json, HWND hwnd,
	                                    char* responseBuf, int responseBufLen);

	static HANDLE s_thread;
	static HWND   s_hwnd;
	static volatile bool s_running;
};

// ── Existing WM messages (WM_USER + 100–108) ─────────────────────────────────
#define WM_WB_PIPE_CMD        (WM_USER + 100)  // wParam=SUBCMD_*, lParam=heap ptr
#define WM_WB_SF_MODE         (WM_USER + 101)  // wParam = SFToolMode value
#define WM_WB_SET_PROJECTION  (WM_USER + 102)  // wParam = 1 top-down, 0 perspective
#define WM_WB_SF_GET_STATE    (WM_USER + 103)  // sync: wParam=bufLen, lParam=char* buf
#define WM_WB_SF_SETINT       (WM_USER + 104)  // wParam=SF_PROP_*, lParam=int value
#define WM_WB_SF_ACTION       (WM_USER + 105)  // wParam=SF_ACT_*
#define WM_WB_SF_OPEN_TEX     (WM_USER + 106)  // wParam=0 inner / 1 border
#define WM_WB_SF_SELECT       (WM_USER + 107)  // wParam=shape id
#define WM_WB_SF_GET_TEX_LIST (WM_USER + 108)  // sync: wParam=bufLen, lParam=char* buf

// ── Tier B — height/texture tools (WM_USER + 110–116) ────────────────────────
#define WM_WB_BRUSH_SET       (WM_USER + 110)  // wParam=BRUSH_PROP_*, lParam=int
#define WM_WB_BRUSH_GET_STATE (WM_USER + 111)  // sync: wParam=bufLen, lParam=char*
#define WM_WB_MOUND_SET       (WM_USER + 112)  // wParam=MOUND_PROP_*, lParam=int
#define WM_WB_MOUND_GET_STATE (WM_USER + 113)  // sync
#define WM_WB_TEX_SET         (WM_USER + 114)  // wParam=TEX_PROP_*, lParam=int
#define WM_WB_TEX_GET_STATE   (WM_USER + 115)  // sync
#define WM_WB_TEX_ACTION      (WM_USER + 116)  // wParam=TEX_ACT_*

// ── Tier C — other terrain tools (WM_USER + 117–127) ─────────────────────────
#define WM_WB_FEATHER_SET     (WM_USER + 117)  // wParam=FEATHER_PROP_*, lParam=int
#define WM_WB_FEATHER_GET     (WM_USER + 118)  // sync
#define WM_WB_SCORCH_SET      (WM_USER + 119)  // wParam=SCORCH_PROP_*, lParam=int (size*100 for Real)
#define WM_WB_SCORCH_GET      (WM_USER + 120)  // sync
#define WM_WB_MESHMOLD_SET    (WM_USER + 121)  // wParam=MESHMOLD_PROP_*, lParam=int (float*100 for Real)
#define WM_WB_MESHMOLD_GET    (WM_USER + 122)  // sync
#define WM_WB_MESHMOLD_ACTION (WM_USER + 123)  // wParam=MESHMOLD_ACT_*
#define WM_WB_WATER_SET       (WM_USER + 124)  // wParam=WATER_PROP_*, lParam=int
#define WM_WB_WATER_GET       (WM_USER + 125)  // sync
#define WM_WB_RAMP_SET        (WM_USER + 126)  // wParam=RAMP_PROP_*, lParam=int (width*100)
#define WM_WB_RAMP_GET        (WM_USER + 127)  // sync

// ── Tier D — camera, lighting, contour, map settings (WM_USER + 128–133) ─────
#define WM_WB_LIGHTING_SET    (WM_USER + 128)  // wParam=LIGHT_PROP_*|(lightIdx<<8), lParam=int
#define WM_WB_LIGHTING_GET    (WM_USER + 129)  // sync
#define WM_WB_LIGHTING_ACTION (WM_USER + 130)  // wParam=LIGHT_ACT_*
#define WM_WB_CONTOUR_SET     (WM_USER + 131)  // wParam=CONTOUR_PROP_*, lParam=int
#define WM_WB_CONTOUR_GET     (WM_USER + 132)  // sync
#define WM_WB_MAP_GET_SETTINGS (WM_USER + 133) // sync

// ── Tier H — map data read-back (WM_USER + 134–149) ──────────────────────────
#define WM_WB_GET_MAP_INFO      (WM_USER + 134)  // sync, large buf
#define WM_WB_GET_HEIGHTMAP     (WM_USER + 135)  // sync, large buf (~350KB JSON)
#define WM_WB_GET_TEXTUREMAP    (WM_USER + 136)  // sync, large buf
#define WM_WB_GET_OBJECTS       (WM_USER + 137)  // sync, large buf
#define WM_WB_GET_WAYPOINTS     (WM_USER + 138)  // sync
#define WM_WB_GET_TRIGGERS      (WM_USER + 139)  // sync
#define WM_WB_GET_TEAMS         (WM_USER + 140)  // sync
#define WM_WB_GET_OBJ_TEMPLATES (WM_USER + 141)  // sync, large buf
#define WM_WB_GET_SELECTED      (WM_USER + 142)  // sync
#define WM_WB_OBJ_GET_PROPS     (WM_USER + 143)  // sync
#define WM_WB_OBJ_SET_PROP      (WM_USER + 144)  // async, lParam=heap ptr
#define WM_WB_WORLDDICT_GET     (WM_USER + 145)  // sync
#define WM_WB_WORLDDICT_SET     (WM_USER + 146)  // async, lParam=heap ptr
#define WM_WB_TIME_OF_DAY_GET   (WM_USER + 147)  // sync
#define WM_WB_TIME_OF_DAY_SET   (WM_USER + 148)  // wParam=TimeOfDay value
#define WM_WB_IMPASSABLE_SLOPE  (WM_USER + 149)  // wParam=0 get/1 set, lParam=slope*100

// ── Tier I — tool stroke, object placement, scripts, players (WM_USER + 150–165)
#define WM_WB_TOOL_STROKE       (WM_USER + 150)  // async, lParam=heap ptr to points array
#define WM_WB_SF_CREATE         (WM_USER + 151)  // async, lParam=heap ptr to shape def
#define WM_WB_OBJ_SET_TEMPLATE  (WM_USER + 152)  // async, lParam=heap ptr to template+owner
#define WM_WB_PLACE_OBJECT      (WM_USER + 153)  // async, lParam=heap ptr to object def
#define WM_WB_SCRIPT_ADD        (WM_USER + 154)  // async, lParam=heap ptr
#define WM_WB_SCRIPT_DELETE     (WM_USER + 155)  // async, lParam=heap ptr
#define WM_WB_SCRIPT_SET_ACTIVE (WM_USER + 156)  // async, lParam=heap ptr
#define WM_WB_SCRIPTS_GET       (WM_USER + 157)  // sync
#define WM_WB_PLAYER_ADD        (WM_USER + 158)  // async, lParam=heap ptr
#define WM_WB_PLAYER_REMOVE     (WM_USER + 159)  // async, lParam=heap ptr (name string)
#define WM_WB_PLAYERS_GET       (WM_USER + 160)  // sync
#define WM_WB_TEAM_ADD          (WM_USER + 161)  // async, lParam=heap ptr
#define WM_WB_TEAM_REMOVE       (WM_USER + 162)  // async, lParam=heap ptr (name string)
#define WM_WB_TEAMS_GET         (WM_USER + 163)  // sync
#define WM_WB_SET_MACRO_TEXTURE (WM_USER + 164)  // async, lParam=heap ptr (name string)
#define WM_WB_GET_MACRO_TEXTURE (WM_USER + 165)  // sync
#define WM_WB_GET_VIEW_STATE    (WM_USER + 170)  // sync: wParam=bufLen, lParam=char* buf — returns 2D view camera state
#define WM_WB_ROTATE_SELECTED   (WM_USER + 171)  // sync: sets angle of all selected objects; reads g_wbRotateAngleDeg
#define WM_WB_PLANT_TREE        (WM_USER + 172)  // sync: plant vegetation object at world coords; reads g_wbPlantTreeReq
#define WM_WB_PLANT_GROVE       (WM_USER + 173)  // sync: scatter vegetation over area; reads g_wbPlantGroveReq

// ── Tier J — SidesList CRUD: players, teams, scripts (WM_USER + 174–180) ──────
// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: SidesList wizard pipe commands
#define WM_WB_GET_SIDELIST  (WM_USER + 174)  // sync: full JSON dump of players+teams+scripts; wParam=bufLen, lParam=char*
#define WM_WB_SET_PLAYER    (WM_USER + 175)  // async: update player dict; lParam=heap json ptr
#define WM_WB_SET_TEAM      (WM_USER + 176)  // async: create/update team; lParam=heap json ptr
#define WM_WB_DEL_TEAM      (WM_USER + 177)  // async: delete team by name; lParam=heap json ptr
#define WM_WB_SET_SCRIPT    (WM_USER + 178)  // async: create/update script; lParam=heap json ptr
#define WM_WB_DEL_SCRIPT    (WM_USER + 179)  // async: delete script; lParam=heap json ptr
#define WM_WB_SET_GROUP     (WM_USER + 180)  // async: create/update script group; lParam=heap json ptr
#define WM_WB_ADD_SKIRMISH  (WM_USER + 181)  // sync: add all standard skirmish players + validateSides
#define WM_WB_ADD_PLAYER    (WM_USER + 182)  // async: add new player side; lParam=heap json ptr (name, faction)
#define WM_WB_DEL_PLAYER    (WM_USER + 183)  // async: remove player side by name; lParam=heap json ptr
#define WM_WB_DEL_WAYPOINT  (WM_USER + 184)  // async: delete waypoint by name; lParam=heap json ptr
#define WM_WB_ADD_TRIGGER   (WM_USER + 185)  // async: create polygon trigger; lParam=heap json ptr
#define WM_WB_DEL_TRIGGER   (WM_USER + 186)  // async: delete trigger by name; lParam=heap json ptr
#define WM_WB_SET_TRIGGER   (WM_USER + 187)  // async: rename/update trigger props; lParam=heap json ptr
#define WM_WB_SELECT_OBJECT (WM_USER + 188)  // async: deselect all, select named waypoint/object; lParam=heap json ptr
// TheSuperHackers @feature Nemellud 06/06/2026 EmbeddedMode: new map / resize map via pipe (no native dialog)
#define WM_WB_NEW_MAP       (WM_USER + 189)  // async: create new map from g_wbNewMapReq; no lParam
#define WM_WB_RESIZE_MAP    (WM_USER + 190)  // async: resize map from g_wbResizeMapReq; no lParam
// TheSuperHackers @feature Nemellud 07/06/2026 EmbeddedMode: roads and bridges via pipe
#define WM_WB_PLACE_ROAD    (WM_USER + 191)  // async: place road segment; lParam=heap json ptr
#define WM_WB_LIST_ROADS    (WM_USER + 192)  // sync:  list all road segments; wParam=bufLen, lParam=buf
#define WM_WB_DEL_ROAD      (WM_USER + 193)  // sync:  delete road nearest to coords; lParam=heap json ptr
#define WM_WB_PLACE_BRIDGE  (WM_USER + 194)  // async: place bridge segment; lParam=heap json ptr
#define WM_WB_LIST_BRIDGES  (WM_USER + 195)  // sync:  list all bridge segments; wParam=bufLen, lParam=buf
#define WM_WB_SET_ROAD_TOOL (WM_USER + 196)  // async: set RoadOptions type/corner; lParam=heap json ptr
#define WM_WB_SEL_ROAD      (WM_USER + 197)  // async: select road/bridge nearest to coords in WB; lParam=heap json ptr
#define WM_WB_SET_BRIDGE_NAME (WM_USER + 198)  // async: set wbScriptName dict property on bridge; lParam=heap json ptr
#define WM_WB_SAVE_TO_PATH    (WM_USER + 199)  // sync: save map to explicit path (no dialog); reads g_wbSavePath
// TheSuperHackers @feature Nemellud 10/06/2026 EmbeddedMode: global lighting via pipe
#define WM_WB_GET_LIGHTING    (WM_USER + 200)  // sync: read global lighting state; wParam=bufLen, lParam=buf
#define WM_WB_SET_LIGHTING    (WM_USER + 201)  // sync: set light colors/angles/timeOfDay; json in buf, result written back
#define WM_WB_RESET_LIGHTING  (WM_USER + 202)  // sync: restore EA factory-default lighting for all times of day
// TheSuperHackers @feature Nemellud 03/07/2026 EmbeddedMode: absolute get/set of impassable-areas overlay
#define WM_WB_IMPASSABLE_VIEW (WM_USER + 203)  // sync: wParam=-1 query / 0 off / 1 on; returns state 0/1

extern char g_wbSavePath[260];

struct WbNewMapReq {
	int x, y, border, height;
	bool valid;
};
struct WbResizeMapReq {
	int x, y, border, height;
	bool anchorTop, anchorBottom, anchorLeft, anchorRight;
	bool valid;
};
extern WbNewMapReq    g_wbNewMapReq;
extern WbResizeMapReq g_wbResizeMapReq;

extern float g_wbRotateAngleDeg;  // degrees — written by pipe thread before SendMessage

// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: plant_tree pipe command
struct WbPlantTreeReq {
	float wx, wy;      // world coordinates
	float angle;       // degrees; <0 = random
	char  name[64];    // tree/vegetation template name
};

// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: plant_grove brush command
struct WbPlantGroveReq {
	float cx, cy;       // center world coordinates
	float radius;       // brush radius in world units
	float density;      // trees per area: 0.05 (sparse) – 1.0 (dense)
	float minSpacing;   // minimum distance between trees (world units)
	char  name[64];     // primary tree template name
	int   skipWater;    // 1 = skip water tiles
	int   skipSteep;    // 1 = skip steep terrain
	int   randRot;      // 1 = random rotation per tree
	int   mix;          // 1 = mix with numbered variants (e.g. Tree01/02/03)
	int   seed;         // random seed; -1 = use rand()
};

// ── SF property keys (WM_WB_SF_SETINT wParam) ────────────────────────────────
#define SF_PROP_INNER_HEIGHT       1
#define SF_PROP_BORDER_WIDTH       2
#define SF_PROP_AUTO_BLEND         3
#define SF_PROP_BLEND_INWARD       4
#define SF_PROP_INNER_TEX          5
#define SF_PROP_BORDER_TEX         6
#define SF_PROP_FILL_AUTO_BLEND    7
#define SF_PROP_FILL_BLEND_INWARD  8
#define SF_PROP_INNER_AUTO_BLEND   9
#define SF_PROP_INNER_BLEND_INWARD 10
#define SF_PROP_AUTO_SAVE          11

// ── SF action keys (WM_WB_SF_ACTION wParam) ──────────────────────────────────
#define SF_ACT_APPLY       1
#define SF_ACT_DELETE      2
#define SF_ACT_DUPLICATE   3
#define SF_ACT_FLIP_H      4
#define SF_ACT_FLIP_V      5
#define SF_ACT_FINISH_POLY 6
#define SF_ACT_FINISH_LINE 7
#define SF_ACT_CLEAR_LINES 8
#define SF_ACT_ROTATE      9
// TheSuperHackers @feature Nemellud 12/06/2026 ShapeFill: losse copy/paste voor Ctrl+C/Ctrl+V
#define SF_ACT_COPY        10
#define SF_ACT_PASTE       11

// ── Brush property keys (WM_WB_BRUSH_SET wParam) ─────────────────────────────
#define BRUSH_PROP_WIDTH   0
#define BRUSH_PROP_FEATHER 1
#define BRUSH_PROP_HEIGHT  2
#define BRUSH_PROP_SHAPE   3   // 0=round, 1=square

// ── Mound property keys (WM_WB_MOUND_SET wParam) ─────────────────────────────
#define MOUND_PROP_WIDTH   0
#define MOUND_PROP_FEATHER 1
#define MOUND_PROP_AMOUNT  2
#define MOUND_PROP_SHAPE   3  // 0=round, 1=square

// ── Texture property keys (WM_WB_TEX_SET wParam) ─────────────────────────────
#define TEX_PROP_FG_CLASS 0
#define TEX_PROP_BG_CLASS 1
#define TEX_PROP_WIDTH    2
#define TEX_PROP_MODE     3   // 0=texture, 1=pathing
#define TEX_PROP_PASSABLE 4   // 0=impassable, 1=passable

// ── Texture action keys (WM_WB_TEX_ACTION wParam) ────────────────────────────
#define TEX_ACT_SWAP 0

// ── Feather property keys (WM_WB_FEATHER_SET wParam) ─────────────────────────
#define FEATHER_PROP_AMOUNT 0
#define FEATHER_PROP_RADIUS 1
#define FEATHER_PROP_RATE   2

// ── Scorch property keys (WM_WB_SCORCH_SET wParam) ───────────────────────────
#define SCORCH_PROP_TYPE 0
#define SCORCH_PROP_SIZE 1   // lParam = size*100 (float encoded as int)

// ── MeshMold property keys (WM_WB_MESHMOLD_SET wParam) ───────────────────────
#define MESHMOLD_PROP_SCALE     0  // lParam = scale*100
#define MESHMOLD_PROP_HEIGHT    1  // lParam = height*100
#define MESHMOLD_PROP_ANGLE     2
#define MESHMOLD_PROP_RAISEONLY 3
#define MESHMOLD_PROP_LOWERONLY 4

// ── MeshMold action keys (WM_WB_MESHMOLD_ACTION wParam) ──────────────────────
#define MESHMOLD_ACT_APPLY 0

// ── Water property keys (WM_WB_WATER_SET wParam) ─────────────────────────────
#define WATER_PROP_HEIGHT  0
#define WATER_PROP_SPACING 1

// ── Ramp property keys (WM_WB_RAMP_SET wParam) ───────────────────────────────
#define RAMP_PROP_WIDTH 0   // lParam = width*100 (Real encoded as int)

// ── Lighting property keys (WM_WB_LIGHTING_SET wParam) ───────────────────────
// wParam encodes: low byte = property, bits 8-9 = light index (0=sun,1=acc1,2=acc2)
#define LIGHT_PROP_AZIMUTH   0
#define LIGHT_PROP_ELEVATION 1
#define LIGHT_PROP_R         2
#define LIGHT_PROP_G         3
#define LIGHT_PROP_B         4
#define LIGHT_ACT_RESET      0

// ── Contour property keys (WM_WB_CONTOUR_SET wParam) ─────────────────────────
#define CONTOUR_PROP_STEP   0
#define CONTOUR_PROP_OFFSET 1
#define CONTOUR_PROP_WIDTH  2

// ── Tier I — programmatic terrain + object write (WM_USER + 166–169) ─────────
// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: Tier I terrain/object write via pipe
#define WM_WB_SF_CREATE_PIPE   (WM_USER + 166)  // sync: creates + optionally applies shape
#define WM_WB_MAP_HEIGHT_SET   (WM_USER + 167)  // sync: fill rect region with height value
#define WM_WB_PLACE_WAYPOINT    (WM_USER + 168)  // sync: place named waypoint at world coords
#define WM_WB_PLACE_OBJECT_PIPE (WM_USER + 169)  // sync: place game object at world coords
#define WM_WB_LINK_WAYPOINTS    (WM_USER + 183)  // sync: link two named waypoints

// Shared data structures for Tier I (written by pipe thread, read by main thread via SendMessage)
struct WbHeightRect {
	int x, y, w, h, val;
};

struct WbPlaceReq {
	float wx, wy;         // world coordinates (game units, Y-up)
	float angle;          // degrees
	char  name[64];       // waypoint name OR object template name
	char  team[64];       // owner team (empty = "team")
	char  pathLabel[64];  // waypoint path label (optional, assigned to waypointPathLabel1)
};

struct WbLinkReq {
	char name1[64];  // first waypoint name
	char name2[64];  // second waypoint name
};

#endif // WBPIPESERVER_H
