# Red's Stream Canvas

**Red's Stream Canvas** is an independent, lightweight Windows compositor that combines multiple screens and application windows into **one output window**.

It is intentionally **not** a recorder, encoder, streaming service, or audio mixer. Any application that can capture a normal window can use the output.

## Alpha 0.2

Alpha 0.2 adds the first real canvas editor while keeping the renderer GPU-first:

- Add multiple screens/windows with the Windows capture picker.
- Select sources directly in the output window.
- Drag sources to move them.
- Resize from edge/corner handles.
- Turn on Crop mode and drag edge handles to crop.
- Hide/show a selected source.
- Bring a source to the front or send it to the back.
- Grid, side-by-side, and picture-in-picture presets.
- Reset crop for the selected source.
- Turn **Edit OFF** before streaming to hide all editor handles/guides.
- Adaptive 30/60 FPS compositor cadence.

### Output-window shortcuts

- `G` — Grid layout
- `P` — Picture-in-picture layout
- `C` — Toggle crop mode
- `H` — Hide/show selected source
- `Delete` — Remove selected source
- `Esc` — Deselect and leave crop mode

## Project principles

1. **Lightweight first** — avoid CPU readbacks, duplicate encoding, and unnecessary subsystems.
2. **GPU composition** — capture frames remain on the GPU using Windows Graphics Capture + Direct3D 11/Direct2D.
3. **Hardware-adaptive defaults** — the app should behave sensibly across weak laptops and powerful desktops rather than targeting one PC.
4. **Independent product** — no affiliation with VDO.Ninja or any other streaming/capture platform.
5. **Simple distribution** — GitHub releases for binaries and a static Cloudflare Pages site for project information.

## Requirements to build

- Windows 10/11.
- Visual Studio with **Desktop development with C++**.
- Windows SDK with C++/WinRT headers.
- CMake 3.25+.

The runtime does **not** require Electron, .NET, OBS, FFmpeg, or the Windows App SDK.

## Build

Open **Developer PowerShell for Visual Studio** in the repository root:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The executable will be under a path similar to:

```text
build/Release/RedsStreamCanvas.exe
```

GitHub Actions also builds the Windows executable automatically. Open the latest successful **Build Windows** workflow run and download the `RedsStreamCanvas-windows-x64` artifact.

## Use

1. Run `RedsStreamCanvas.exe`.
2. Click **+ Add screen / window** and pick a monitor or app window.
3. Add more sources as needed.
4. Arrange them in **Red's Stream Canvas — Output** or use a preset layout.
5. Turn **Edit OFF** to remove editor guides.
6. In your streaming/calling/sharing app, capture **Red's Stream Canvas — Output** as one window.

## Performance design

Capture callbacks use `Direct3D11CaptureFramePool::CreateFreeThreaded`. Accepted frames are copied GPU-to-GPU into persistent Direct3D textures. Direct2D scales/crops those textures into the output swap chain. No video encoding happens inside Red's Stream Canvas.

## Repository layout

```text
src/                 Native Win32 application
web/                 Static Cloudflare Pages landing site
.github/workflows/   GitHub Windows build workflow
```

## License

MIT. See `LICENSE`.
