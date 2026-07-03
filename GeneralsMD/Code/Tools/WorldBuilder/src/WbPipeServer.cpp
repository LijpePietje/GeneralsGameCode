/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// TheSuperHackers @feature Nemellud 24/05/2026 EmbeddedMode: named pipe server
// TheSuperHackers @feature Nemellud 25/05/2026 EmbeddedMode: full tool + map read-back protocol v5
// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: mouse_wheel direct to view, synthetic right-mouse for pan

#include "StdAfx.h"
#include "WbPipeServer.h"
#include "ShapeFillTool.h"
#include "wbview3d.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Tier I shared state — written by pipe thread before SendMessage, read by main-thread handler.
// Safe: pipe allows max 1 connection, SendMessage is synchronous.
ShapeDef     g_wbPipeCreateShape;
bool         g_wbPipeCreateApply = false;
WbHeightRect g_wbPipeHeightRect  = {0,0,0,0,0};
WbPlaceReq      g_wbPlaceReq        = {0};
WbLinkReq       g_wbLinkReq         = {0};
float           g_wbRotateAngleDeg  = 0.0f;
WbPlantTreeReq  g_wbPlantTreeReq    = {0};
WbPlantGroveReq g_wbPlantGroveReq   = {0};
// TheSuperHackers @feature Nemellud 06/06/2026 EmbeddedMode: new map / resize map structs
WbNewMapReq    g_wbNewMapReq    = {0,0,0,0,false};
// TheSuperHackers @feature Nemellud 10/06/2026 EmbeddedMode: save-to-path path buffer
char           g_wbSavePath[260] = "";
WbResizeMapReq g_wbResizeMapReq = {0,0,0,0,false,false,false,false,false};
// TheSuperHackers @feature Nemellud 07/06/2026 EmbeddedMode: del_road sync struct
struct WbDelRoadReq { float x1, y1; bool bridge; };
WbDelRoadReq   g_wbDelRoadReq   = {0,0,false};
extern int   g_wbScreenQuerySx;
extern int   g_wbScreenQuerySy;

// Resource IDs for WM_COMMAND dispatch
#define WB_ID_TERRAIN    32771
#define WB_ID_TEXTURE    32902
#define WB_ID_OBJECTS    32918
#define WB_ID_WAYPOINTS  32964
#define WB_ID_SCRIPTS    32959
#define WB_ID_SHAPEFILL  33347
#define WB_ID_UNDO       57643
#define WB_ID_REDO       57644
#define WB_ID_SAVE       57603
#define WB_ID_VIEW_3D    32943
#define WB_ID_VIEW_2D    32944

HANDLE WbPipeServer::s_thread  = NULL;
HWND   WbPipeServer::s_hwnd    = NULL;
volatile bool WbPipeServer::s_running = false;

volatile bool WbPipeServer::s_viewTopDown  = true;
volatile bool WbPipeServer::s_sfToolActive = false;
char WbPipeServer::s_activeTool[32] = "unknown";

/*static*/ void WbPipeServer::setViewTopDown(bool v) { s_viewTopDown  = v; }
/*static*/ void WbPipeServer::setSfToolActive(bool v) { s_sfToolActive = v; }
/*static*/ void WbPipeServer::setActiveTool(const char* name) {
	strncpy(s_activeTool, name, sizeof(s_activeTool) - 1);
	s_activeTool[sizeof(s_activeTool) - 1] = '\0';
}

static const char* PIPE_NAME = "\\\\.\\pipe\\wb-engine";

void WbPipeServer::Start(HWND mainHwnd)
{
	if (s_thread != NULL)
		return;

	// TheSuperHackers @fix Nemellud 06/06/2026 EmbeddedMode: force ShowEntireMap=1 so the 3D view always renders the full map.
	// Profile may have stored 0 from a previous partial-view session via the View menu.
	::AfxGetApp()->WriteProfileInt("MainFrame", "ShowEntireMap", 1);

	s_hwnd    = mainHwnd;
	s_running = true;
	s_thread  = CreateThread(NULL, 0, ThreadProc, NULL, 0, NULL);
}

void WbPipeServer::Stop()
{
	s_running = false;
	HANDLE dummy = CreateFile(PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
	                          0, NULL, OPEN_EXISTING, 0, NULL);
	if (dummy != INVALID_HANDLE_VALUE)
		CloseHandle(dummy);

	if (s_thread) {
		WaitForSingleObject(s_thread, 2000);
		CloseHandle(s_thread);
		s_thread = NULL;
	}
}

DWORD WINAPI WbPipeServer::ThreadProc(LPVOID /*param*/)
{
	while (s_running) {
		HANDLE pipe = CreateNamedPipeA(
			PIPE_NAME,
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
			1, 65536, 65536, 0, NULL);

		if (pipe == INVALID_HANDLE_VALUE)
			break;

		BOOL connected = ConnectNamedPipe(pipe, NULL);
		if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
			CloseHandle(pipe);
			continue;
		}

		if (s_running)
			HandleClient(pipe, s_hwnd);

		DisconnectNamedPipe(pipe);
		CloseHandle(pipe);
	}
	return 0;
}

static bool ReadLine(HANDLE pipe, char* buf, int maxLen)
{
	int pos = 0;
	while (pos < maxLen - 1) {
		char ch;
		DWORD read = 0;
		if (!ReadFile(pipe, &ch, 1, &read, NULL) || read == 0)
			return false;
		if (ch == '\n')
			break;
		if (ch != '\r')
			buf[pos++] = ch;
	}
	buf[pos] = '\0';
	return pos > 0;
}

void WbPipeServer::HandleClient(HANDLE pipe, HWND hwnd)
{
	// 64KB input matches pipe buffer; scripts with conditions can exceed 4096 bytes
	static char line[65536];
	// 1MB static buffer — single-threaded per connection, safe
	static char response[1048576];

	while (s_running && ReadLine(pipe, line, sizeof(line))) {
		response[0] = '\0';
		DispatchCommand(line, hwnd, response, sizeof(response));

		DWORD written = 0;
		int len = (int)strlen(response);
		response[len] = '\n';
		WriteFile(pipe, response, len + 1, &written, NULL);
	}
}

// ── Minimal JSON helpers ──────────────────────────────────────────────────────

static bool JsonGetInt(const char* json, const char* key, int* out)
{
	char search[64];
	_snprintf(search, sizeof(search), "\"%s\"", key);
	const char* p = strstr(json, search);
	if (!p) return false;
	p += strlen(search);
	while (*p == ' ' || *p == ':') p++;
	if (*p == '"') p++;
	if ((*p >= '0' && *p <= '9') || *p == '-') { *out = atoi(p); return true; }
	return false;
}

static bool JsonGetBool(const char* json, const char* key, bool* out)
{
	char search[64];
	_snprintf(search, sizeof(search), "\"%s\"", key);
	const char* p = strstr(json, search);
	if (!p) return false;
	p += strlen(search);
	while (*p == ' ' || *p == ':') p++;
	if (*p == '"') p++;
	if (strncmp(p, "true",  4) == 0) { *out = true;  return true; }
	if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
	return false;
}

static bool JsonGetStr(const char* json, const char* key, char* out, int outLen)
{
	char search[64];
	_snprintf(search, sizeof(search), "\"%s\"", key);
	const char* p = strstr(json, search);
	if (!p) return false;
	p += strlen(search);
	while (*p == ' ' || *p == ':') p++;
	if (*p == '"') {
		p++;
		int i = 0;
		while (*p && *p != '"' && i < outLen - 1)
			out[i++] = *p++;
		out[i] = '\0';
		return true;
	}
	return false;
}

static bool JsonGetFloat(const char* json, const char* key, float* out)
{
	char search[64];
	_snprintf(search, sizeof(search), "\"%s\"", key);
	const char* p = strstr(json, search);
	if (!p) return false;
	p += strlen(search);
	while (*p == ' ' || *p == ':') p++;
	if (*p == '"') p++;
	if ((*p >= '0' && *p <= '9') || *p == '-') { *out = (float)atof(p); return true; }
	return false;
}

// ── View toggle lookup table ──────────────────────────────────────────────────

struct NamedCmd { const char* name; UINT id; };

static const NamedCmd VIEW_TOGGLES[] = {
	{"show_grid",          32772},
	{"show_texture",       32927},
	{"show_terrain",       33342},
	{"show_objects",       32926},
	{"show_waypoints",     32966},
	{"show_triggers",      32969},
	{"show_shadows",       32976},
	{"show_labels",        33003},
	{"show_models",        33004},
	{"show_bounding_boxes",33008},
	{"show_sight_ranges",  33009},
	{"show_weapon_ranges", 33010},
	{"show_garrisoned",    33326},
	{"show_map_boundaries",33331},
	{"show_letterbox",     33012},
	{"show_sound_flags",   33340},
	{"show_sound_circles", 33346},
	{"highlight_test_art", 33011},
	{"show_impassable",    32981},
	{"show_wireframe",     32934},
	{"show_top_down",      32944},
	{"show_extra_blends",  33339},
	{"show_clouds",        32945},
	{"show_soft_water",    33335},
	{"show_macrotexture",  32956},
	{"snap_to_grid",       32939},
	{"show_brush_feedback",32954},
	{"show_contours",      32930},
	{"reload_textures",    32974},
	{"show_all_3d",        32943},
	{"partial_96",         32989},
	{"partial_128",        32990},
	{"partial_160",        32991},
	{"partial_192",        32992},
	// Texture Sizing menu
	{"remove_cliff_mapping",33014},
	{"optimize_tiles",     32994},
	// Window menu
	{"window_640",         32949},
	{"window_800",         32950},
	{"window_1024",        32951},
	{"window_reset",       32947},
	{nullptr, 0}
};

// ── Edit operations lookup table ──────────────────────────────────────────────

static const NamedCmd EDIT_OPS[] = {
	{"cut",              0xE122},  // ID_EDIT_CUT
	{"copy",             0xE123},  // ID_EDIT_COPY
	{"paste",            0xE125},  // ID_EDIT_PASTE
	{"delete",           32931},
	{"select_duplicate", 32940},
	{"select_similar",   32988},
	{"pick_anything",    33001},
	{"pick_structures",  32994},
	{"pick_infantry",    32995},
	{"pick_vehicles",    32996},
	{"pick_shrubbery",   32997},
	{"pick_manmade",     32998},
	{"pick_natural",     32999},
	{"pick_roads",       33327},
	{"pick_waypoints",   33002},
	{"validate_map",     33337},
	{"fix_teams",        33338},
	{nullptr, 0}
};

// ── Layer operations lookup table ─────────────────────────────────────────────

static const NamedCmd LAYER_OPS[] = {
	{"new",           33017},
	{"delete",        33018},
	{"hide",          33324},
	{"select_object", 33344},
	{"select_active", 33345},
	{nullptr, 0}
};

// ── Dialog operations lookup table ───────────────────────────────────────────

static const NamedCmd DIALOG_OPS[] = {
	{"players",       32970},
	{"teams",         32978},
	{"scripts",       32977},
	{"global_light",  32928},
	{"camera",        32928}, // same dialog, shown differently by WB
	{"impassable",    32953},
	{"contour",       32930},
	{nullptr, 0}
};

// ── Full tool name→ID map ─────────────────────────────────────────────────────

static const NamedCmd TOOL_MAP[] = {
	{"brush",         32771},
	{"terrain",       32771},
	{"mound",         32900},
	{"dig",           32901},
	{"texture",       32902},
	{"bigtexture",    32792},
	{"feather",       32791},
	{"scorch",        33007},
	{"ramp",          61467},
	{"meshmold",      32955},
	{"water",         32986},
	{"road",          32937},
	{"fence",         32979},
	{"grove",         32924},
	{"polygon",       32968},
	{"pointer",       32921},
	{"eyedropper",    32913},
	{"floodfill",     32903},
	{"ruler",         32958},
	{"scroll",        32973},
	{"objects",       32918},
	{"waypoints",     32964},
	{"scripts",       32959},
	{"shapefill",     33347},
	{nullptr, 0}
};

static UINT lookupCmd(const NamedCmd* table, const char* name)
{
	for (int i = 0; table[i].name; i++)
		if (strcmp(table[i].name, name) == 0)
			return table[i].id;
	return 0;
}

// ── Main dispatcher ───────────────────────────────────────────────────────────

bool WbPipeServer::DispatchCommand(const char* json, HWND hwnd,
                                   char* responseBuf, int responseBufLen)
{
	char cmd[64] = "";
	if (!JsonGetStr(json, "cmd", cmd, sizeof(cmd))) {
		_snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"missing cmd\"}");
		return false;
	}

	// {"cmd":"status"}
	if (strcmp(cmd, "status") == 0) {
		const char* viewMode = s_viewTopDown ? "2d" : "3d";
		_snprintf(responseBuf, responseBufLen,
			"{\"ok\":true,\"ready\":%s,\"viewMode\":\"%s\",\"activeTool\":\"%s\"}",
			hwnd ? "true" : "false", viewMode, s_activeTool);
		return true;
	}

	if (!hwnd) {
		_snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"no window\"}");
		return false;
	}

	// ── Tier A — simple WM_COMMAND dispatchers ─────────────────────────────────

	if (strcmp(cmd, "undo") == 0) {
		PostMessage(hwnd, WM_COMMAND, WB_ID_UNDO, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}
	if (strcmp(cmd, "redo") == 0) {
		PostMessage(hwnd, WM_COMMAND, WB_ID_REDO, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}
	if (strcmp(cmd, "save") == 0) {
		PostMessage(hwnd, WM_COMMAND, WB_ID_SAVE, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}
	// TheSuperHackers @feature Nemellud 10/06/2026 EmbeddedMode: save to explicit path (no dialog)
	if (strcmp(cmd, "save_to") == 0) {
		char path[MAX_PATH] = "";
		JsonGetStr(json, "path", path, sizeof(path));
		if (path[0] == '\0') {
			_snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"missing path\"}");
			return false;
		}
		strncpy(g_wbSavePath, path, sizeof(g_wbSavePath) - 1);
		g_wbSavePath[sizeof(g_wbSavePath) - 1] = '\0';
		LRESULT ok = SendMessage(hwnd, WM_WB_SAVE_TO_PATH, 0, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":%s}", ok ? "true" : "false");
		return (ok != 0);
	}

	if (strcmp(cmd, "tool") == 0) {
		char name[64] = "";
		JsonGetStr(json, "name", name, sizeof(name));
		UINT id = lookupCmd(TOOL_MAP, name);
		if (!id) {
			_snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"unknown tool\"}");
			return false;
		}
		PostMessage(hwnd, WM_COMMAND, id, 0);
		setActiveTool(name);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true,\"tool\":\"%s\"}", name);
		return true;
	}

	if (strcmp(cmd, "view") == 0) {
		char mode[16] = "";
		JsonGetStr(json, "mode", mode, sizeof(mode));
		WPARAM topDown = (strcmp(mode, "2d") == 0) ? 1 : 0;
		PostMessage(hwnd, WM_WB_SET_PROJECTION, topDown, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true,\"mode\":\"%s\"}", mode);
		return true;
	}

	if (strcmp(cmd, "load") == 0) {
		char path[MAX_PATH] = "";
		JsonGetStr(json, "path", path, sizeof(path));
		if (path[0] == '\0') {
			_snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"missing path\"}");
			return false;
		}
		char* heapPath = (char*)malloc(strlen(path) + 1);
		if (heapPath) {
			strcpy(heapPath, path);
			PostMessage(hwnd, WM_WB_PIPE_CMD, 1 /* SUBCMD_LOAD */, (LPARAM)heapPath);
		}
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "view_toggle") == 0) {
		char name[64] = "";
		JsonGetStr(json, "name", name, sizeof(name));
		UINT id = lookupCmd(VIEW_TOGGLES, name);
		if (!id) {
			_snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"unknown toggle\"}");
			return false;
		}
		PostMessage(hwnd, WM_COMMAND, id, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true,\"name\":\"%s\"}", name);
		return true;
	}

	// TheSuperHackers @feature Nemellud 03/07/2026 EmbeddedMode: absolute get/set of the
	// impassable-areas overlay. Unlike view_toggle (blind WM_COMMAND flip) this reports
	// the actual engine state, so the Electron UI can resync after a refresh.
	if (strcmp(cmd, "impassable_view") == 0) {
		int state = -1;  // -1 = query only
		JsonGetInt(json, "state", &state);
		int cur = (int)SendMessage(hwnd, WM_WB_IMPASSABLE_VIEW, (WPARAM)state, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true,\"state\":%d}", cur);
		return true;
	}

	if (strcmp(cmd, "edit") == 0) {
		char op[64] = "";
		JsonGetStr(json, "op", op, sizeof(op));
		UINT id = lookupCmd(EDIT_OPS, op);
		if (!id) {
			_snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"unknown edit op\"}");
			return false;
		}
		PostMessage(hwnd, WM_COMMAND, id, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "layer") == 0) {
		char op[64] = "";
		JsonGetStr(json, "op", op, sizeof(op));
		UINT id = lookupCmd(LAYER_OPS, op);
		if (!id) {
			_snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"unknown layer op\"}");
			return false;
		}
		PostMessage(hwnd, WM_COMMAND, id, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "dialog") == 0) {
		char name[64] = "";
		JsonGetStr(json, "name", name, sizeof(name));
		UINT id = lookupCmd(DIALOG_OPS, name);
		if (!id) {
			_snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"unknown dialog\"}");
			return false;
		}
		PostMessage(hwnd, WM_COMMAND, id, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "camera_action") == 0) {
		char action[32] = "";
		JsonGetStr(json, "action", action, sizeof(action));
		if      (strcmp(action, "view_home")            == 0) PostMessage(hwnd, WM_COMMAND, 33334, 0);
		else if (strcmp(action, "toggle_pitch_rotate")  == 0) PostMessage(hwnd, WM_COMMAND, 33332, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// ── Existing ShapeFill commands ────────────────────────────────────────────

	if (strcmp(cmd, "shapefill_mode") == 0) {
		char mode[32] = "";
		JsonGetStr(json, "mode", mode, sizeof(mode));
		int modeId = -1;
		if      (strcmp(mode, "rect")    == 0) modeId = 0;
		else if (strcmp(mode, "circle")  == 0) modeId = 1;
		else if (strcmp(mode, "polygon") == 0) modeId = 2;
		else if (strcmp(mode, "select")  == 0) modeId = 3;
		else if (strcmp(mode, "line")    == 0) modeId = 4;
		else if (strcmp(mode, "fill")    == 0) modeId = 5;
		else if (strcmp(mode, "edit")    == 0) modeId = 6;
		if (modeId < 0) {
			_snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"unknown mode\"}");
			return false;
		}
		PostMessage(hwnd, WM_WB_SF_MODE, (WPARAM)modeId, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true,\"mode\":\"%s\"}", mode);
		return true;
	}

	if (strcmp(cmd, "shapefill_get_state") == 0) {
		SendMessage(hwnd, WM_WB_SF_GET_STATE, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "shapefill_set") == 0) {
		bool anySet = false;
		int  ival;
		bool bval;
		if (JsonGetInt(json,  "innerHeight",       &ival)) { PostMessage(hwnd, WM_WB_SF_SETINT, SF_PROP_INNER_HEIGHT,        (LPARAM)ival); anySet = true; }
		if (JsonGetInt(json,  "borderWidth",       &ival)) { PostMessage(hwnd, WM_WB_SF_SETINT, SF_PROP_BORDER_WIDTH,        (LPARAM)ival); anySet = true; }
		if (JsonGetBool(json, "autoBlend",         &bval)) { PostMessage(hwnd, WM_WB_SF_SETINT, SF_PROP_AUTO_BLEND,          (LPARAM)(bval?1:0)); anySet = true; }
		if (JsonGetBool(json, "blendInward",       &bval)) { PostMessage(hwnd, WM_WB_SF_SETINT, SF_PROP_BLEND_INWARD,        (LPARAM)(bval?1:0)); anySet = true; }
		if (JsonGetInt(json,  "innerTexClass",     &ival)) { PostMessage(hwnd, WM_WB_SF_SETINT, SF_PROP_INNER_TEX,           (LPARAM)ival); anySet = true; }
		if (JsonGetInt(json,  "borderTexClass",    &ival)) { PostMessage(hwnd, WM_WB_SF_SETINT, SF_PROP_BORDER_TEX,          (LPARAM)ival); anySet = true; }
		if (JsonGetBool(json, "fillAutoBlend",     &bval)) { PostMessage(hwnd, WM_WB_SF_SETINT, SF_PROP_FILL_AUTO_BLEND,     (LPARAM)(bval?1:0)); anySet = true; }
		if (JsonGetBool(json, "fillBlendInward",   &bval)) { PostMessage(hwnd, WM_WB_SF_SETINT, SF_PROP_FILL_BLEND_INWARD,   (LPARAM)(bval?1:0)); anySet = true; }
		if (JsonGetBool(json, "innerAutoBlend",    &bval)) { PostMessage(hwnd, WM_WB_SF_SETINT, SF_PROP_INNER_AUTO_BLEND,    (LPARAM)(bval?1:0)); anySet = true; }
		if (JsonGetBool(json, "innerBlendInward",  &bval)) { PostMessage(hwnd, WM_WB_SF_SETINT, SF_PROP_INNER_BLEND_INWARD,  (LPARAM)(bval?1:0)); anySet = true; }
		if (JsonGetBool(json, "autoSave",          &bval)) { PostMessage(hwnd, WM_WB_SF_SETINT, SF_PROP_AUTO_SAVE,           (LPARAM)(bval?1:0)); anySet = true; }
		_snprintf(responseBuf, responseBufLen, "{\"ok\":%s}", anySet ? "true" : "false");
		return anySet;
	}

	if (strcmp(cmd, "shapefill_action") == 0) {
		char action[32] = "";
		JsonGetStr(json, "action", action, sizeof(action));
		WPARAM act = 0;
		if      (strcmp(action, "apply")       == 0) act = SF_ACT_APPLY;
		else if (strcmp(action, "delete")      == 0) act = SF_ACT_DELETE;
		else if (strcmp(action, "duplicate")   == 0) act = SF_ACT_DUPLICATE;
		else if (strcmp(action, "copy")        == 0) act = SF_ACT_COPY;
		else if (strcmp(action, "paste")       == 0) act = SF_ACT_PASTE;
		else if (strcmp(action, "flip_h")      == 0) act = SF_ACT_FLIP_H;
		else if (strcmp(action, "flip_v")      == 0) act = SF_ACT_FLIP_V;
		else if (strcmp(action, "finish_poly") == 0) act = SF_ACT_FINISH_POLY;
		else if (strcmp(action, "finish_line") == 0) act = SF_ACT_FINISH_LINE;
		else if (strcmp(action, "clear_lines") == 0) act = SF_ACT_CLEAR_LINES;
		else if (strcmp(action, "rotate")      == 0) act = SF_ACT_ROTATE;
		if (!act) {
			_snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"unknown action\"}");
			return false;
		}
		PostMessage(hwnd, WM_WB_SF_ACTION, act, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "shapefill_open_tex") == 0) {
		char which[16] = "";
		JsonGetStr(json, "which", which, sizeof(which));
		WPARAM w = (strcmp(which, "border") == 0) ? 1 : 0;
		PostMessage(hwnd, WM_WB_SF_OPEN_TEX, w, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "get_texture_list") == 0) {
		SendMessage(hwnd, WM_WB_SF_GET_TEX_LIST, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "shapefill_select") == 0) {
		int id = -1;
		JsonGetInt(json, "id", &id);
		// TheSuperHackers @feature Nemellud 12/06/2026 ShapeFill: kind:"line" selecteert een lijn i.p.v. shape
		char kind[16] = "";
		JsonGetStr(json, "kind", kind, sizeof(kind));
		LPARAM isLine = (strcmp(kind, "line") == 0) ? 1 : 0;
		PostMessage(hwnd, WM_WB_SF_SELECT, (WPARAM)id, isLine);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// ── Tier B — BrushTool ────────────────────────────────────────────────────

	if (strcmp(cmd, "brush_get_state") == 0) {
		SendMessage(hwnd, WM_WB_BRUSH_GET_STATE, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "brush_set") == 0) {
		int ival;
		if (JsonGetInt(json, "width",   &ival)) PostMessage(hwnd, WM_WB_BRUSH_SET, BRUSH_PROP_WIDTH,   (LPARAM)ival);
		if (JsonGetInt(json, "feather", &ival)) PostMessage(hwnd, WM_WB_BRUSH_SET, BRUSH_PROP_FEATHER, (LPARAM)ival);
		if (JsonGetInt(json, "height",  &ival)) PostMessage(hwnd, WM_WB_BRUSH_SET, BRUSH_PROP_HEIGHT,  (LPARAM)ival);
		if (JsonGetInt(json, "shape",   &ival)) PostMessage(hwnd, WM_WB_BRUSH_SET, BRUSH_PROP_SHAPE,   (LPARAM)ival);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// ── Tier B — MoundTool ────────────────────────────────────────────────────

	if (strcmp(cmd, "mound_get_state") == 0) {
		SendMessage(hwnd, WM_WB_MOUND_GET_STATE, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "mound_set") == 0) {
		int ival;
		if (JsonGetInt(json, "width",   &ival)) PostMessage(hwnd, WM_WB_MOUND_SET, MOUND_PROP_WIDTH,   (LPARAM)ival);
		if (JsonGetInt(json, "feather", &ival)) PostMessage(hwnd, WM_WB_MOUND_SET, MOUND_PROP_FEATHER, (LPARAM)ival);
		if (JsonGetInt(json, "amount",  &ival)) PostMessage(hwnd, WM_WB_MOUND_SET, MOUND_PROP_AMOUNT,  (LPARAM)ival);
		if (JsonGetInt(json, "shape",   &ival)) PostMessage(hwnd, WM_WB_MOUND_SET, MOUND_PROP_SHAPE,   (LPARAM)ival);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// ── Tier B — TerrainMaterial (texture painter) ────────────────────────────

	if (strcmp(cmd, "texture_get_state") == 0) {
		SendMessage(hwnd, WM_WB_TEX_GET_STATE, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "texture_set") == 0) {
		int ival;
		bool bval;
		if (JsonGetInt(json, "fgClass",  &ival)) PostMessage(hwnd, WM_WB_TEX_SET, TEX_PROP_FG_CLASS, (LPARAM)ival);
		if (JsonGetInt(json, "bgClass",  &ival)) PostMessage(hwnd, WM_WB_TEX_SET, TEX_PROP_BG_CLASS, (LPARAM)ival);
		if (JsonGetInt(json, "width",    &ival)) PostMessage(hwnd, WM_WB_TEX_SET, TEX_PROP_WIDTH,    (LPARAM)ival);
		if (JsonGetInt(json, "mode",     &ival)) PostMessage(hwnd, WM_WB_TEX_SET, TEX_PROP_MODE,     (LPARAM)ival);
		if (JsonGetInt(json, "passable", &ival)) PostMessage(hwnd, WM_WB_TEX_SET, TEX_PROP_PASSABLE, (LPARAM)ival);
		if (JsonGetBool(json, "pathing", &bval)) PostMessage(hwnd, WM_WB_TEX_SET, TEX_PROP_MODE,     (LPARAM)(bval?1:0));
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "texture_swap") == 0) {
		PostMessage(hwnd, WM_WB_TEX_ACTION, TEX_ACT_SWAP, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// ── Tier C — FeatherTool ──────────────────────────────────────────────────

	if (strcmp(cmd, "feather_get_state") == 0) {
		SendMessage(hwnd, WM_WB_FEATHER_GET, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "feather_set") == 0) {
		int ival;
		if (JsonGetInt(json, "amount", &ival)) PostMessage(hwnd, WM_WB_FEATHER_SET, FEATHER_PROP_AMOUNT, (LPARAM)ival);
		if (JsonGetInt(json, "radius", &ival)) PostMessage(hwnd, WM_WB_FEATHER_SET, FEATHER_PROP_RADIUS, (LPARAM)ival);
		if (JsonGetInt(json, "rate",   &ival)) PostMessage(hwnd, WM_WB_FEATHER_SET, FEATHER_PROP_RATE,   (LPARAM)ival);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// ── Tier C — ScorchTool ───────────────────────────────────────────────────

	if (strcmp(cmd, "scorch_get_state") == 0) {
		SendMessage(hwnd, WM_WB_SCORCH_GET, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "scorch_set") == 0) {
		int ival;
		float fval;
		if (JsonGetInt(json, "type", &ival))   PostMessage(hwnd, WM_WB_SCORCH_SET, SCORCH_PROP_TYPE, (LPARAM)ival);
		if (JsonGetFloat(json, "size", &fval)) PostMessage(hwnd, WM_WB_SCORCH_SET, SCORCH_PROP_SIZE, (LPARAM)(int)(fval * 100.0f));
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// ── Tier C — MeshMoldTool ─────────────────────────────────────────────────

	if (strcmp(cmd, "meshmold_get_state") == 0) {
		SendMessage(hwnd, WM_WB_MESHMOLD_GET, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "meshmold_set") == 0) {
		int ival;
		float fval;
		if (JsonGetFloat(json, "scale",  &fval)) PostMessage(hwnd, WM_WB_MESHMOLD_SET, MESHMOLD_PROP_SCALE,     (LPARAM)(int)(fval * 100.0f));
		if (JsonGetFloat(json, "height", &fval)) PostMessage(hwnd, WM_WB_MESHMOLD_SET, MESHMOLD_PROP_HEIGHT,    (LPARAM)(int)(fval * 100.0f));
		if (JsonGetInt(json,   "angle",  &ival)) PostMessage(hwnd, WM_WB_MESHMOLD_SET, MESHMOLD_PROP_ANGLE,     (LPARAM)ival);
		if (JsonGetInt(json,   "raiseOnly", &ival)) PostMessage(hwnd, WM_WB_MESHMOLD_SET, MESHMOLD_PROP_RAISEONLY, (LPARAM)ival);
		if (JsonGetInt(json,   "lowerOnly", &ival)) PostMessage(hwnd, WM_WB_MESHMOLD_SET, MESHMOLD_PROP_LOWERONLY, (LPARAM)ival);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "meshmold_action") == 0) {
		PostMessage(hwnd, WM_WB_MESHMOLD_ACTION, MESHMOLD_ACT_APPLY, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// ── Tier C — WaterTool ────────────────────────────────────────────────────

	if (strcmp(cmd, "water_get_state") == 0) {
		SendMessage(hwnd, WM_WB_WATER_GET, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "water_set") == 0) {
		int ival;
		if (JsonGetInt(json, "height",  &ival)) PostMessage(hwnd, WM_WB_WATER_SET, WATER_PROP_HEIGHT,  (LPARAM)ival);
		if (JsonGetInt(json, "spacing", &ival)) PostMessage(hwnd, WM_WB_WATER_SET, WATER_PROP_SPACING, (LPARAM)ival);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// ── Tier C — RampTool ────────────────────────────────────────────────────

	if (strcmp(cmd, "ramp_get_state") == 0) {
		SendMessage(hwnd, WM_WB_RAMP_GET, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "ramp_set") == 0) {
		float fval;
		if (JsonGetFloat(json, "width", &fval)) PostMessage(hwnd, WM_WB_RAMP_SET, RAMP_PROP_WIDTH, (LPARAM)(int)(fval * 100.0f));
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// ── Tier D — Contour ─────────────────────────────────────────────────────

	if (strcmp(cmd, "contour_get_state") == 0) {
		SendMessage(hwnd, WM_WB_CONTOUR_GET, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "contour_set") == 0) {
		int ival;
		if (JsonGetInt(json, "step",   &ival)) PostMessage(hwnd, WM_WB_CONTOUR_SET, CONTOUR_PROP_STEP,   (LPARAM)ival);
		if (JsonGetInt(json, "offset", &ival)) PostMessage(hwnd, WM_WB_CONTOUR_SET, CONTOUR_PROP_OFFSET, (LPARAM)ival);
		if (JsonGetInt(json, "width",  &ival)) PostMessage(hwnd, WM_WB_CONTOUR_SET, CONTOUR_PROP_WIDTH,  (LPARAM)ival);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// ── Tier I — Programmatic terrain write ──────────────────────────────────

	if (strcmp(cmd, "shapefill_create") == 0) {
		// Build ShapeDef from JSON fields
		ShapeDef def;
		char typeStr[16] = "rect";
		JsonGetStr(json, "type", typeStr, sizeof(typeStr));
		def.type = (strcmp(typeStr, "circle") == 0) ? SHAPE_CIRCLE
		         : (strcmp(typeStr, "polygon") == 0) ? SHAPE_POLYGON
		         : SHAPE_RECT;
		int ival;
		if (JsonGetInt(json, "x0", &ival))          def.x0          = ival;
		if (JsonGetInt(json, "y0", &ival))          def.y0          = ival;
		if (JsonGetInt(json, "x1", &ival))          def.x1          = ival;
		if (JsonGetInt(json, "y1", &ival))          def.y1          = ival;
		if (JsonGetInt(json, "cx", &ival))          def.cx          = ival;
		if (JsonGetInt(json, "cy", &ival))          def.cy          = ival;
		if (JsonGetInt(json, "r",  &ival))          def.r           = ival;
		if (JsonGetInt(json, "innerHeight", &ival)) def.innerHeight = ival;
		if (JsonGetInt(json, "borderWidth", &ival)) def.borderWidth = ival;
		if (JsonGetInt(json, "innerTex",    &ival)) def.innerTexClass  = ival;
		if (JsonGetInt(json, "borderTex",   &ival)) def.borderTexClass = ival;
		bool bval;
		if (JsonGetBool(json, "autoBlend", &bval)) def.autoBlendOuter = bval ? TRUE : FALSE;
		JsonGetBool(json, "apply", &g_wbPipeCreateApply);
		g_wbPipeCreateShape = def;
		int newId = (int)SendMessage(hwnd, WM_WB_SF_CREATE_PIPE, 0, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true,\"id\":%d}", newId);
		return true;
	}

	if (strcmp(cmd, "map_height_set") == 0) {
		int ival;
		g_wbPipeHeightRect = {0,0,0,0,20};
		if (JsonGetInt(json, "x",     &ival)) g_wbPipeHeightRect.x   = ival;
		if (JsonGetInt(json, "y",     &ival)) g_wbPipeHeightRect.y   = ival;
		if (JsonGetInt(json, "w",     &ival)) g_wbPipeHeightRect.w   = ival;
		if (JsonGetInt(json, "h",     &ival)) g_wbPipeHeightRect.h   = ival;
		if (JsonGetInt(json, "value", &ival)) g_wbPipeHeightRect.val = ival;
		SendMessage(hwnd, WM_WB_MAP_HEIGHT_SET, 0, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "place_waypoint") == 0) {
		float fval;
		g_wbPlaceReq = {0};
		if (JsonGetFloat(json, "wx", &fval)) g_wbPlaceReq.wx = fval;
		if (JsonGetFloat(json, "wy", &fval)) g_wbPlaceReq.wy = fval;
		JsonGetStr(json, "name",      g_wbPlaceReq.name,      sizeof(g_wbPlaceReq.name));
		JsonGetStr(json, "team",      g_wbPlaceReq.team,      sizeof(g_wbPlaceReq.team));
		JsonGetStr(json, "pathLabel", g_wbPlaceReq.pathLabel, sizeof(g_wbPlaceReq.pathLabel));
		int ok = (int)SendMessage(hwnd, WM_WB_PLACE_WAYPOINT, 0, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":%s}", ok ? "true" : "false");
		return true;
	}

	if (strcmp(cmd, "link_waypoints") == 0) {
		g_wbLinkReq = {0};
		JsonGetStr(json, "name1", g_wbLinkReq.name1, sizeof(g_wbLinkReq.name1));
		JsonGetStr(json, "name2", g_wbLinkReq.name2, sizeof(g_wbLinkReq.name2));
		int ok = (int)SendMessage(hwnd, WM_WB_LINK_WAYPOINTS, 0, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":%s}", ok ? "true" : "false");
		return true;
	}

	if (strcmp(cmd, "place_object") == 0) {
		float fval;
		g_wbPlaceReq = {0};
		if (JsonGetFloat(json, "wx",    &fval)) g_wbPlaceReq.wx    = fval;
		if (JsonGetFloat(json, "wy",    &fval)) g_wbPlaceReq.wy    = fval;
		if (JsonGetFloat(json, "angle", &fval)) g_wbPlaceReq.angle = fval;
		JsonGetStr(json, "template", g_wbPlaceReq.name, sizeof(g_wbPlaceReq.name));
		JsonGetStr(json, "team",     g_wbPlaceReq.team, sizeof(g_wbPlaceReq.team));
		int ok = (int)SendMessage(hwnd, WM_WB_PLACE_OBJECT_PIPE, 0, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":%s}", ok ? "true" : "false");
		return true;
	}

	// TheSuperHackers @feature Nemellud 07/06/2026 EmbeddedMode: road and bridge placement via pipe
	if (strcmp(cmd, "place_road") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_PLACE_ROAD, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}
	if (strcmp(cmd, "map_get_roads") == 0) {
		SendMessage(hwnd, WM_WB_LIST_ROADS, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}
	if (strcmp(cmd, "del_road") == 0) {
		// Synchronous so a list query immediately after sees the updated state.
		// Pre-fill global struct (pipe allows max 1 connection, SendMessage is sync).
		float fx1 = 0, fy1 = 0; int bridge = 0;
		JsonGetFloat(json, "x1", &fx1); JsonGetFloat(json, "y1", &fy1);
		JsonGetInt(json, "bridge", &bridge);
		g_wbDelRoadReq.x1 = fx1; g_wbDelRoadReq.y1 = fy1; g_wbDelRoadReq.bridge = !!bridge;
		int ok = (int)SendMessage(hwnd, WM_WB_DEL_ROAD, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		if (!responseBuf[0]) _snprintf(responseBuf, responseBufLen, "{\"ok\":%s}", ok ? "true" : "false");
		return true;
	}
	if (strcmp(cmd, "sel_road") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_SEL_ROAD, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}
	if (strcmp(cmd, "place_bridge") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_PLACE_BRIDGE, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}
	if (strcmp(cmd, "map_get_bridges") == 0) {
		SendMessage(hwnd, WM_WB_LIST_BRIDGES, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}
	if (strcmp(cmd, "set_bridge_name") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_SET_BRIDGE_NAME, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}
	if (strcmp(cmd, "set_road_tool") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_SET_ROAD_TOOL, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: scatter vegetation over a circular area
	if (strcmp(cmd, "plant_grove") == 0) {
		float fval; int ival;
		g_wbPlantGroveReq = {0};
		g_wbPlantGroveReq.radius     = 60.0f;
		g_wbPlantGroveReq.density    = 0.3f;
		g_wbPlantGroveReq.minSpacing = 15.0f;
		g_wbPlantGroveReq.skipWater  = 1;
		g_wbPlantGroveReq.skipSteep  = 1;
		g_wbPlantGroveReq.randRot    = 1;
		g_wbPlantGroveReq.seed       = -1;
		if (JsonGetFloat(json, "cx",          &fval)) g_wbPlantGroveReq.cx         = fval;
		if (JsonGetFloat(json, "cy",          &fval)) g_wbPlantGroveReq.cy         = fval;
		if (JsonGetFloat(json, "radius",      &fval)) g_wbPlantGroveReq.radius     = fval;
		if (JsonGetFloat(json, "density",     &fval)) g_wbPlantGroveReq.density    = fval;
		if (JsonGetFloat(json, "min_spacing", &fval)) g_wbPlantGroveReq.minSpacing = fval;
		if (JsonGetInt  (json, "skip_water",  &ival)) g_wbPlantGroveReq.skipWater  = ival;
		if (JsonGetInt  (json, "skip_steep",  &ival)) g_wbPlantGroveReq.skipSteep  = ival;
		if (JsonGetInt  (json, "rand_rot",    &ival)) g_wbPlantGroveReq.randRot    = ival;
		if (JsonGetInt  (json, "mix",         &ival)) g_wbPlantGroveReq.mix        = ival;
		if (JsonGetInt  (json, "seed",        &ival)) g_wbPlantGroveReq.seed       = ival;
		JsonGetStr(json, "type", g_wbPlantGroveReq.name, sizeof(g_wbPlantGroveReq.name));
		int planted = (int)SendMessage(hwnd, WM_WB_PLANT_GROVE, 0, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":%s,\"planted\":%d}", planted > 0 ? "true" : "false", planted);
		return true;
	}

	// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: plant vegetation/tree at world position
	if (strcmp(cmd, "plant_tree") == 0) {
		float fval;
		g_wbPlantTreeReq = {0};
		g_wbPlantTreeReq.angle = -1.0f; // default: random rotation
		if (JsonGetFloat(json, "wx",    &fval)) g_wbPlantTreeReq.wx    = fval;
		if (JsonGetFloat(json, "wy",    &fval)) g_wbPlantTreeReq.wy    = fval;
		if (JsonGetFloat(json, "angle", &fval)) g_wbPlantTreeReq.angle = fval;
		JsonGetStr(json, "type", g_wbPlantTreeReq.name, sizeof(g_wbPlantTreeReq.name));
		int ok = (int)SendMessage(hwnd, WM_WB_PLANT_TREE, 0, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":%s}", ok ? "true" : "false");
		return true;
	}

	// ── Tier H — Map data read-back ───────────────────────────────────────────

	if (strcmp(cmd, "rotate_selected") == 0) {
		float fval = 0;
		JsonGetFloat(json, "angle", &fval);
		g_wbRotateAngleDeg = fval;
		int ok = (int)SendMessage(hwnd, WM_WB_ROTATE_SELECTED, 0, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":%s}", ok > 0 ? "true" : "false");
		return true;
	}

	if (strcmp(cmd, "screen_to_world") == 0) {
		float fval = 0;
		if (JsonGetFloat(json, "sx", &fval)) g_wbScreenQuerySx = (int)fval;
		if (JsonGetFloat(json, "sy", &fval)) g_wbScreenQuerySy = (int)fval;
		SendMessage(hwnd, WM_WB_GET_VIEW_STATE, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "mouse_wheel") == 0) {
		int delta = 0;
		JsonGetInt(json, "delta", &delta);
		WPARAM wp = (WPARAM)(((short)delta) << 16);
		HWND viewHwnd = (WbView3d::s_instance && WbView3d::s_instance->m_hWnd) ? WbView3d::s_instance->m_hWnd : hwnd;
		PostMessage(viewHwnd, WM_MOUSEWHEEL, wp, MAKELPARAM(0, 0));
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "synthetic_rmouse") == 0) {
		char action[16] = "";
		JsonGetStr(json, "action", action, sizeof(action));
		int sx = 0, sy = 0;
		JsonGetInt(json, "sx", &sx);
		JsonGetInt(json, "sy", &sy);
		HWND viewHwnd = (WbView3d::s_instance && WbView3d::s_instance->m_hWnd) ? WbView3d::s_instance->m_hWnd : hwnd;
		POINT pt = { sx, sy };
		ScreenToClient(viewHwnd, &pt);
		LPARAM lp = MAKELPARAM(pt.x, pt.y);
		if (strcmp(action, "down") == 0)
			PostMessage(viewHwnd, WM_RBUTTONDOWN, MK_RBUTTON, lp);
		else if (strcmp(action, "move") == 0)
			PostMessage(viewHwnd, WM_MOUSEMOVE, MK_RBUTTON, lp);
		else if (strcmp(action, "up") == 0)
			PostMessage(viewHwnd, WM_RBUTTONUP, 0, lp);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "map_get_info") == 0) {
		SendMessage(hwnd, WM_WB_GET_MAP_INFO, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "map_get_heightmap") == 0) {
		SendMessage(hwnd, WM_WB_GET_HEIGHTMAP, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "map_get_texturemap") == 0) {
		char mode[16] = "blended";
		JsonGetStr(json, "mode", mode, sizeof(mode));
		int useBase = (strcmp(mode, "base") == 0) ? 1 : 0;
		SendMessage(hwnd, WM_WB_GET_TEXTUREMAP, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		(void)useBase; // handler reads the full response buffer, mode encoded separately if needed
		return true;
	}

	if (strcmp(cmd, "map_get_objects") == 0) {
		SendMessage(hwnd, WM_WB_GET_OBJECTS, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "map_get_waypoints") == 0) {
		SendMessage(hwnd, WM_WB_GET_WAYPOINTS, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "map_get_triggers") == 0) {
		SendMessage(hwnd, WM_WB_GET_TRIGGERS, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "map_get_teams") == 0) {
		SendMessage(hwnd, WM_WB_GET_TEAMS, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	// TheSuperHackers @feature Nemellud 30/05/2026 EmbeddedMode: SidesList wizard — full JSON + CRUD
	if (strcmp(cmd, "sidelist_get") == 0) {
		SendMessage(hwnd, WM_WB_GET_SIDELIST, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "sidelist_player_set") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_SET_PLAYER, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "sidelist_player_new") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_ADD_PLAYER, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "sidelist_player_del") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_DEL_PLAYER, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "del_waypoint") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_DEL_WAYPOINT, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "trigger_add") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_ADD_TRIGGER, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "trigger_del") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_DEL_TRIGGER, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "trigger_set") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_SET_TRIGGER, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "sidelist_team_set") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_SET_TEAM, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "sidelist_team_del") == 0) {
		char* heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_DEL_TEAM, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "sidelist_script_set") == 0) {
		// SendMessage (sync): copy json into responseBuf, handler reads + writes result back
		strncpy(responseBuf, json, responseBufLen - 1); responseBuf[responseBufLen - 1] = '\0';
		SendMessage(hwnd, WM_WB_SET_SCRIPT, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	// TheSuperHackers @feature Nemellud 10/06/2026 EmbeddedMode: global lighting via pipe
	if (strcmp(cmd, "lighting_get") == 0) {
		SendMessage(hwnd, WM_WB_GET_LIGHTING, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "lighting_set") == 0) {
		strncpy(responseBuf, json, responseBufLen - 1); responseBuf[responseBufLen - 1] = '\0';
		SendMessage(hwnd, WM_WB_SET_LIGHTING, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "lighting_reset") == 0) {
		SendMessage(hwnd, WM_WB_RESET_LIGHTING, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "sidelist_script_del") == 0) {
		strncpy(responseBuf, json, responseBufLen - 1); responseBuf[responseBufLen - 1] = '\0';
		SendMessage(hwnd, WM_WB_DEL_SCRIPT, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "sidelist_group_set") == 0) {
		strncpy(responseBuf, json, responseBufLen - 1); responseBuf[responseBufLen - 1] = '\0';
		SendMessage(hwnd, WM_WB_SET_GROUP, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "sidelist_add_skirmish") == 0) {
		SendMessage(hwnd, WM_WB_ADD_SKIRMISH, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "map_get_selected") == 0) {
		SendMessage(hwnd, WM_WB_GET_SELECTED, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "obj_get_props") == 0) {
		SendMessage(hwnd, WM_WB_OBJ_GET_PROPS, (WPARAM)responseBufLen, (LPARAM)responseBuf);
		return true;
	}

	if (strcmp(cmd, "obj_set_prop") == 0) {
		char *heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_OBJ_SET_PROP, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	if (strcmp(cmd, "select_map_object") == 0) {
		char *heap = new char[strlen(json) + 1];
		strcpy(heap, json);
		PostMessage(hwnd, WM_WB_SELECT_OBJECT, 0, (LPARAM)heap);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// TheSuperHackers @feature Nemellud 06/06/2026 EmbeddedMode: create new map via pipe (no native dialog)
	if (strcmp(cmd, "new_map") == 0) {
		int ival;
		g_wbNewMapReq = {0,0,0,0,false};
		g_wbNewMapReq.x      = 200; // defaults
		g_wbNewMapReq.y      = 200;
		g_wbNewMapReq.border = 10;
		g_wbNewMapReq.height = 20;
		if (JsonGetInt(json, "x",      &ival)) g_wbNewMapReq.x      = ival;
		if (JsonGetInt(json, "y",      &ival)) g_wbNewMapReq.y      = ival;
		if (JsonGetInt(json, "border", &ival)) g_wbNewMapReq.border = ival;
		if (JsonGetInt(json, "height", &ival)) g_wbNewMapReq.height = ival;
		g_wbNewMapReq.valid = true;
		PostMessage(hwnd, WM_WB_NEW_MAP, 0, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	// TheSuperHackers @feature Nemellud 06/06/2026 EmbeddedMode: resize map via pipe (no native dialog)
	if (strcmp(cmd, "resize_map") == 0) {
		int ival;
		char anchor[4] = "mc";
		g_wbResizeMapReq = {0,0,0,0,false,false,false,false,false};
		g_wbResizeMapReq.border = 10;
		g_wbResizeMapReq.height = 20;
		if (JsonGetInt(json, "x",      &ival))   g_wbResizeMapReq.x      = ival;
		if (JsonGetInt(json, "y",      &ival))   g_wbResizeMapReq.y      = ival;
		if (JsonGetInt(json, "border", &ival))   g_wbResizeMapReq.border = ival;
		if (JsonGetInt(json, "height", &ival))   g_wbResizeMapReq.height = ival;
		JsonGetStr(json, "anchor", anchor, sizeof(anchor));
		// anchor: char[0]='t'|'m'|'b', char[1]='l'|'c'|'r'
		g_wbResizeMapReq.anchorTop    = (anchor[0] == 't');
		g_wbResizeMapReq.anchorBottom = (anchor[0] == 'b');
		g_wbResizeMapReq.anchorLeft   = (anchor[1] == 'l');
		g_wbResizeMapReq.anchorRight  = (anchor[1] == 'r');
		g_wbResizeMapReq.valid = true;
		PostMessage(hwnd, WM_WB_RESIZE_MAP, 0, 0);
		_snprintf(responseBuf, responseBufLen, "{\"ok\":true}");
		return true;
	}

	_snprintf(responseBuf, responseBufLen, "{\"ok\":false,\"error\":\"unknown cmd\"}");
	return false;
}
