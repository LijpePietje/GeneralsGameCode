# ShapeFillTool — Restore on a New PC

This repo contains the ShapeFillTool addition to C&C Generals Zero Hour WorldBuilder.
Follow these steps to build it from scratch on a new machine.

---

## Prerequisites

- **Windows 10/11 x64**
- **Visual Studio 2022 Community** (with "Desktop development with C++" workload)
- **Git**
- The **SuperHackers GeneralsGameCode** repo (the base codebase)

---

## Step 1 — Clone the base repo

```bash
git clone https://github.com/TheSuperHackers/GeneralsGameCode.git
cd GeneralsGameCode
```

Follow their README to configure and generate the CMake build (requires CMake + VS2022):

```bash
cmake -B build/win32 -A Win32 -DCMAKE_BUILD_TYPE=Release
```

Verify it builds the unmodified WorldBuilder first:
```
cmake --build build/win32 --target z_worldbuilder --config Release
```

---

## Step 2 — Copy new files from this repo

Copy these files into the matching paths inside `GeneralsGameCode/`:

| File in this repo | Destination |
|---|---|
| `include/ShapeFillTool.h` | `GeneralsMD/Code/Tools/WorldBuilder/include/` |
| `include/ShapeFillOptions.h` | `GeneralsMD/Code/Tools/WorldBuilder/include/` |
| `src/ShapeFillTool.cpp` | `GeneralsMD/Code/Tools/WorldBuilder/src/` |
| `src/ShapeFillOptions.cpp` | `GeneralsMD/Code/Tools/WorldBuilder/src/` |
| `res/Toolbar.bmp` | `GeneralsMD/Code/Tools/WorldBuilder/res/` (replaces original) |

---

## Step 3 — Patch existing files

Apply each change below. All paths are relative to `GeneralsMD/Code/Tools/WorldBuilder/`.

---

### `CMakeLists.txt`

After `"src/ScorchTool.cpp"`, add:
```cmake
    "src/ShapeFillOptions.cpp"
    "src/ShapeFillTool.cpp"
```

After `"include/ScorchTool.h"`, add:
```cmake
    "include/ShapeFillOptions.h"
    "include/ShapeFillTool.h"
```

---

### `include/Tool.h`

After `virtual WorldHeightMapEdit *getHeightMap(void) {return nullptr;}`, add:
```cpp
virtual void drawOverlay(CDC* /*pDC*/, WbView* /*pView*/) {}
```

---

### `include/wbview.h`

After the two `virtual Bool viewToDocCoords` / `docToViewCoords` declarations, add:
```cpp
virtual Int getScrollOffsetX() const { return 0; }
virtual Int getScrollOffsetY() const { return 0; }
```

---

### `include/WorldBuilderView.h`

In the `AFX_VIRTUAL` public section, after `PreCreateWindow`, add:
```cpp
virtual Int getScrollOffsetX() const override { return mXScrollOffset; }
virtual Int getScrollOffsetY() const override { return mYScrollOffset; }
```

---

### `include/wbview3d.h`

After `virtual void pitchCamera(Real delta) override;`, add:
```cpp
bool getTopDownProjection(void) const { return m_projection; }
void setTopDownProjection(bool enable);
```

---

### `include/WorldBuilder.h`

Add include after `#include "ScorchTool.h"`:
```cpp
#include "ShapeFillTool.h"
```

Change:
```cpp
enum {NUM_VIEW_TOOLS=25};
```
to:
```cpp
enum {NUM_VIEW_TOOLS=26};
```

Add member after `RulerTool m_rulerTool;`:
```cpp
ShapeFillTool  m_shapeFillTool;
```

---

### `include/MainFrm.h`

Add include after `#include "RulerOptions.h"`:
```cpp
#include "ShapeFillOptions.h"
```

Add member after `ScorchOptions m_scorchOptions;`:
```cpp
ShapeFillOptions  m_shapeFillOptions;
```

---

### `src/WorldBuilder.cpp`

After `m_tools[24] = &m_rulerTool;`, add:
```cpp
m_tools[25] = &m_shapeFillTool;
```

---

### `src/MainFrm.cpp`

In `OnCreate`, after the scorchOptions block (the last `if (m_optionsPanelHeight < ...)` line before the GlobalLight section), add:
```cpp
m_shapeFillOptions.Create(IDD_SHAPE_FILL_OPTIONS, this);
m_shapeFillOptions.SetWindowPos(nullptr, frameRect.left, frameRect.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
m_shapeFillOptions.GetWindowRect(&frameRect);
if (m_optionsPanelWidth < frameRect.Width()) m_optionsPanelWidth = frameRect.Width();
if (m_optionsPanelHeight < frameRect.Height()) m_optionsPanelHeight = frameRect.Height();
```

In `showOptionsDialog`, add a case:
```cpp
case IDD_SHAPE_FILL_OPTIONS: newOptions = &m_shapeFillOptions; break;
```

---

### `src/WorldBuilderView.cpp`

Add include at the top (after `#include "WorldBuilder.h"`):
```cpp
#include "ShapeFillTool.h"
```

At the very end of `OnPaint()`, before the closing `}`, add:
```cpp
ShapeFillTool::drawOverlayStatic(&dc, this);
```

---

### `src/wbview3d.cpp`

Add includes (after `#include "ImpassableOptions.h"`):
```cpp
#include "ShapeFillTool.h"
#include "ShapeFillOptions.h"
```

At the end of `drawLabels(HDC hdc)`, before the closing `}`, add:
```cpp
ShapeFillTool::drawOverlayStatic(nullptr, this);
```

In `OnViewShowtopdownview()`, add at the end before `}`:
```cpp
ShapeFillOptions::updateTopDownState();
```

Add new method after `OnUpdateViewShowtopdownview`:
```cpp
void WbView3d::setTopDownProjection(bool enable)
{
    if (m_projection == enable) return;
    m_projection = enable;
    if (m_heightMapRenderObj) m_heightMapRenderObj->setFlattenHeights(m_projection);
    invalObjectInView(nullptr);
    ::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowTopDownView", m_projection?1:0);
    ShapeFillOptions::updateTopDownState();
}
```

---

### `res/WorldBuilder.rc`

Find `IDD_SHAPE_FILL_OPTIONS` dialog definition and replace the entire block with the version from this repo's `res/WorldBuilder.rc` (search for `IDD_SHAPE_FILL_OPTIONS DIALOG`).

---

### `res/resource.h`

After `#define IDC_SF_SHAPE_LIST  1405`, add:
```cpp
#define IDC_SF_TOPDOWN_BTN  1406
```

---

## Step 4 — Build

```bash
# In a VS2022 Developer Command Prompt (x86):
cmake --build build/win32 --target z_worldbuilder --config Release
```

Output: `build/win32/GeneralsMD/Release/WorldBuilderZH.exe`

---

## Step 5 — Deploy

Copy `WorldBuilderZH.exe` to your C&C Generals Zero Hour installation directory.

---

## Notes

- The ShapeFill toolbar icon is slot 36 in `Toolbar.bmp` (4bpp BMP, 576×15px).
- Tool ID is `ID_SHAPE_FILL_TOOL` — defined in `resource.h`.
- The tool only works visually in **Top Down View** (Ctrl+F). The options panel shows a "Switch to Top Down View" button when in 3D mode.
