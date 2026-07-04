# ShapeFillTool — Build Notes

## Overzicht

ShapeFillTool is een nieuw tool voor WorldBuilder (C&C Generals Zero Hour) dat rechthoeken, cirkels en polygonen op het terrein tekent, met instelbare hoogte en textuur. Het zit in de **feature/map-reader** branch van `c:\ProjectWorldbuilder`.

---

## Bestanden

| Bestand | Status | Doel |
|---|---|---|
| `src/ShapeFillTool.cpp` | Nieuw (untracked) | Tool implementatie |
| `include/ShapeFillTool.h` | Nieuw (untracked) | Tool header |
| `src/ShapeFillOptions.cpp` | Nieuw (untracked) | Options panel implementatie |
| `include/ShapeFillOptions.h` | Nieuw (untracked) | Options panel header |
| `res/WorldBuilder.rc` | Gewijzigd | Toolbar button + IDD_SHAPE_FILL_OPTIONS dialog |
| `res/resource.h` | Gewijzigd | Nieuwe IDs: IDD_SHAPE_FILL_OPTIONS=240, ID_SHAPE_FILL_TOOL=33347, IDC_SF_* |
| `CMakeLists.txt` | Gewijzigd | ShapeFillTool.cpp/.h + ShapeFillOptions.cpp/.h toegevoegd |
| `include/Tool.h` | Gewijzigd | `virtual void drawOverlay(CDC*, WbView*)` toegevoegd aan Tool base class |

### Nog te doen (na iconentest):
- `src/MainFrm.cpp` — panel aanmaken + ON_COMMAND handler voor ID_SHAPE_FILL_TOOL
- `include/MainFrm.h` — `ShapeFillOptions* m_pShapeFillOptions;` member
- `src/WorldBuilder.cpp` — ShapeFillTool instantie registreren
- `src/WorldBuilderView.cpp` — `drawOverlay(pDC, pView)` aanroepen in OnDraw
- `src/wbview3d.cpp` — `drawOverlay(pDC, pView)` aanroepen in 3D view paint

---

## Iconen-probleem (OPGELOST / IN PROGRESS)

### Symptoom
Na toevoegen van code-wijzigingen: toolbar icons tonen "Chinese tekens" (garbled).

### Oorzaak (vermoed)
De toolbar bitmap `res/Toolbar.bmp` is **4bpp** (niet 24bpp). Als de bitmap met verkeerde row-size wordt herschreven, raakt hij corrupt.

### Binary search resultaten
- Origineel (git stash) → icons GOED
- Alleen RC + resource.h → icons GOED
- Alle wijzigingen (code + RC) → icons SLECHT

### Huidige aanpak
Code-wijzigingen stap voor stap toevoegen en na elke stap testen:
1. CMakeLists.txt + Tool.h (minimal) — nu aan het testen
2. MainFrm.h/cpp
3. WorldBuilder.h/cpp
4. WorldBuilderView.cpp + wbview3d.cpp

### Toolbar.bmp feiten
- `res/Toolbar.bmp`: 4bpp, 560×15px, 35 icon-slots (16×15px elk)
- Row size formule: `((width * 4 + 31) // 32) * 4` = 280 bytes per rij
- BMP rows zijn opgeslagen bottom-to-top
- Pixel data offset: 118 bytes (na BMP header + DIB header + 16-color palette)
- **NOOIT** herschrijven met 24bpp aanname — dit corrupt de bitmap!

### Toolbar.bmp herstellen
```bash
cd c:/ProjectWorldbuilder/TheSuperHackers/GeneralsMD
git checkout HEAD -- Code/Tools/WorldBuilder/res/Toolbar.bmp
```

---

## Builden

### Commando
```
cd C:\ProjectWorldbuilder\TheSuperHackers
do_build.bat
```
Of handmatig:
```batch
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -no_logo
cmake --build build/win32 --target z_worldbuilder --config Release
```

### Build duurt
~60 seconden bij eerste compile van gewijzigde bestanden. Daarna sneller (alleen gewijzigde .obj opnieuw).

### Output
`C:\ProjectWorldbuilder\TheSuperHackers\build\win32\GeneralsMD\Code\Tools\WorldBuilder\Release\WorldBuilder.exe`

### Installeren (naar game dir)
```batch
copy /Y "build\win32\...\WorldBuilder.exe" "C:\Program Files (x86)\Origin Games\...\WorldBuilder.exe"
```

---

## ShapeFillTool — Technische Details

### Rasterisatie
- **Tile-center coördinaten**: alle drie rasterizers gebruiken `x + 0.5f` en `y + 0.5f` voor afstandsberekeningen
- **Rect**: `dx = abs((x+0.5f) - cx)` etc.
- **Circle**: `dist = sqrt(px*px + py*py)` met tile-center cancel
- **Polygon**: float point-in-polygon + edge-distance vanuit tile center

### Cirkel tekenen (overlay)
- Gebruik `tileCenterToView()` voor het center (niet tile-corner)
- Teken als **ellips** (niet cirkel) vanwege de non-square pixel-aspect van de WorldBuilder 2D view
- Separate rx en ry berekenen via `tileCenterToView(cx ± r, cy)` en `tileCenterToView(cx, cy ± r)`

### Polygon matryoshka (inner ring)
- Gebruik `insetPolygon(shape.points, borderWidth)` — edge-offset + intersectie
- **NIET** centroid-shrink gebruiken (werkt niet goed voor concave polygonen)
- Matcht het JavaScript algoritme uit de browser editor

### Live drag preview
- `m_hasDraft` + `m_draftShape` static members in ShapeFillTool
- `updateDragShape(tx, ty)` vult `m_draftShape` en zet `m_hasDraft = true`
- `drawOverlayStatic()` tekent `m_draftShape` als `m_hasDraft == true`
- `mouseUp()` zet `m_hasDraft = false`

### Delete + lijst refreshen
- `OnClickDelete()`: aanroep `ShapeFillTool::deleteSelectedShape()` + `updateFromTool()` + `syncAndRedraw()`
- `updateFromTool()` herlaadt de listbox met actuele shapes

---

## IDs in resource.h

```cpp
#define IDD_SHAPE_FILL_OPTIONS   240   // na IDD_MAPOBJECT_PROPPAGE_SOUND=239
#define ID_SHAPE_FILL_TOOL       33347 // na bestaand ID_TEAM_EDIT=33346
#define IDC_SF_MODE_RECT         1384
#define IDC_SF_MODE_CIRCLE       1385
#define IDC_SF_MODE_POLYGON      1386
#define IDC_SF_MODE_SELECT       1387
#define IDC_SF_INNER_HEIGHT_EDIT 1388
#define IDC_SF_INNER_HEIGHT_POPUP 1389
#define IDC_SF_OUTER_HEIGHT_EDIT 1390
#define IDC_SF_OUTER_HEIGHT_POPUP 1391
#define IDC_SF_BORDER_WIDTH_EDIT 1392
#define IDC_SF_BORDER_WIDTH_POPUP 1393
#define IDC_SF_INNER_TEX_BTN     1394
#define IDC_SF_BORDER_TEX_BTN    1395
#define IDC_SF_AUTO_BLEND        1396
#define IDC_SF_APPLY_BTN         1397
#define IDC_SF_DELETE_BTN        1398
#define IDC_SF_COPY_BTN          1399
#define IDC_SF_PASTE_BTN         1400
#define IDC_SF_FLIP_H_BTN        1401
#define IDC_SF_FLIP_V_BTN        1402
#define IDC_SF_FINISH_POLY       1403
#define IDC_SF_COORDS_LABEL      1404
#define IDC_SF_SHAPE_LIST        1405
```

---

## mss32.dll waarschuwing

TheSuperHackers cmake build **overschrijft** de echte `mss32.dll` (Miles Sound System, 346KB) met een 13KB stub! Gevolg: geen audio in Generals Online.

**Fix**: Kopieer de echte DLL terug na elke build:
```
Bron: C:\Program Files (x86)\Origin Games\Command and Conquer Generals\mss32.dll
Doel: C:\Program Files (x86)\Origin Games\Command and Conquer Generals Zero Hour\mss32.dll
```
