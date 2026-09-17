# Red's Stream Canvas

**Red's Stream Canvas** is an independent, lightweight Windows compositor for combining multiple screens and application windows into **one clean output window**.

It is intentionally **not** a recorder, encoder, streaming service, or audio mixer. Any app that can capture a normal window can use the output.

## Project principles

1. **Lightweight first** — avoid CPU readbacks, duplicate encoding, and unnecessary subsystems.
2. **GPU composition** — capture frames stay on the GPU using Windows Graphics Capture + Direct3D 11/Direct2D.
3. **Hardware-adaptive defaults** — Auto mode starts conservatively and raises/lowers output cadence based on measured composition cost.
4. **Independent product** — no affiliation with VDO.Ninja or any other streaming/capture platform.
5. **Simple distribution** — GitHub releases for the app; a static Cloudflare Pages site for project/download information.

## Current status: Alpha 0.1 scaffold

This first build establishes the real native capture/composition pipeline:

- Add multiple screens or windows with the Windows capture picker.
- Each source is captured with `Windows.Graphics.Capture`.
- Frames are GPU-copied into stable Direct3D textures (no CPU pixel copy).
- Direct2D composites all active sources into one output window.
- Sources are automatically arranged in a grid and preserve aspect ratio.
- Output starts at 30 FPS in Auto mode and can move to 60 FPS when composition remains cheap.
- Closing a selected source removes it automatically.
- Separate control window and output window.

### Intentionally not implemented yet

- Drag/resize/crop editor.
- Saved layouts.
- Per-source visibility and ordering.
- Fixed canvas resolution independent from output-window size.
- Better adaptive-performance telemetry.
- Installer/signing.

## Requirements to build

- Windows 10/11.
- Visual Studio 2022/2026 with **Desktop development with C++**.
- Windows SDK with C++/WinRT headers.
- CMake 3.25+ (Visual Studio includes CMake support).

The runtime itself does **not** require Electron, .NET, OBS, FFmpeg, or the Windows App SDK.

## Build

Open **Developer PowerShell for Visual Studio** in the repository root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

For Visual Studio 2026, choose the Visual Studio generator installed on that machine.

The executable will be under a path similar to:

```text
build/Release/RedsStreamCanvas.exe
```

## How Alpha 0.1 works

1. Launch `RedsStreamCanvas.exe`.
2. Click **+ Add screen / window**.
3. Pick a monitor or application window from Windows' capture picker.
4. Repeat to add more sources.
5. Capture **Red's Stream Canvas — Output** in your streaming/calling/sharing app.

## Performance design

The capture callbacks use `Direct3D11CaptureFramePool::CreateFreeThreaded`, keeping frame arrival work off the UI message loop. Each accepted frame is copied GPU-to-GPU into a persistent texture. The renderer then wraps those textures as Direct2D bitmaps and scales them into the output swap chain.

No video encoding happens inside Red's Stream Canvas.

## Repository layout

```text
src/                 Native Win32 application
web/                 Static Cloudflare Pages landing site
.github/workflows/   GitHub build workflow
```

## License

MIT. See `LICENSE`.
