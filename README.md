# Red's Stream Canvas

**Red's Stream Canvas** is a lightweight Windows compositor that combines multiple
screens and application windows into **one output window**. Any application that
can capture a normal window (VDO.Ninja, Discord, meeting software, browsers,
OBS, etc.) can then use that output. Red's Stream Canvas is an independent
project and is not affiliated with VDO.Ninja or any other platform.

It does **not**:
- record video
- encode or stream
- mix or capture audio
- support plugins, transitions, filters, or a browser-source engine

CAPTURE → COMPOSITE → DISPLAY. Nothing more.

## Status: clean rewrite, Milestone 1

This is a from-scratch rewrite. The previous implementation had a black-frame
capture bug and later a freeze caused by unsafe cross-thread D3D11 context
use. This rewrite fixes the architecture at the root instead of patching
around it — see **Architecture** below.

Milestone 1 scope (current): one control window, one output window, pick a
single monitor or application window via the standard Windows capture picker,
and see it rendered live in the output window, with visible diagnostics.
Drag/resize/crop/multi-source/layouts are intentionally not implemented yet —
see `ROADMAP.md`-style milestone list in the project's original spec.

## Architecture (why this avoids the old black-screen / freeze bugs)

**One thread owns the D3D11 immediate context, permanently.**

| Thread | Responsibilities | Touches D3D11 context? |
|---|---|---|
| UI thread | Win32 message loop, buttons, capture picker, 1 Hz diagnostics refresh | No |
| Render thread | `CopyResource`, `Draw`, `Present`, `Map`/`Unmap` — everything | **Yes, exclusively** |
| WGC `FrameArrived` (thread pool) | Fetch the frame, hand off a COM pointer | No |

The capture callback (`CaptureSource::OnFrameArrived`) never issues a GPU
command. It calls `TryGetNextFrame()` and stores the resulting
`Direct3D11CaptureFrame` object into a mutex-guarded "latest wins" slot — a
cheap pointer swap, never held across a GPU call. Holding the frame *object*
(not just its texture) is what keeps the WGC ring-buffer slot valid; this is
exactly what that object's lifetime is for.

The render thread's `CaptureSource::RenderTick()` takes whatever frame is
waiting (if any), converts its `IDirect3DSurface` to `ID3D11Texture2D`,
`CopyResource`s it into a texture `CaptureSource` allocates once and reuses
every frame, then closes the WGC frame to return its ring-buffer slot to the
pool.

This design directly targets the failure modes described for the old
project:
- **Black frames from a device/adapter mismatch** — there is exactly one
  `ID3D11Device` in the whole process (`GraphicsDevice`), shared by the
  capture frame pool *and* the compositor. There is no separate implicit
  device for WGC to create, so `CopyResource` can never silently fail across
  adapters. `GraphicsDevice::Initialize()` also explicitly prefers the
  adapter with the most dedicated VRAM, so on Intel+NVIDIA hybrid laptops it
  reliably lands on the discrete GPU rather than whatever adapter happens to
  enumerate first.
- **The freeze from a mutex held across GPU work** — no mutex in this
  codebase is ever held while a D3D11 call is in flight. `LatestSlot<T>`'s
  lock only ever guards a `std::move` of a COM pointer.

## Requirements to build

- Windows 10 or 11, x64
- Visual Studio with **Desktop development with C++**
- Windows SDK with C++/WinRT headers
- CMake 3.25+

No Electron, no .NET requirement, no OBS, no FFmpeg, no Windows App SDK.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The executable is produced at `build/Release/RedsStreamCanvas.exe`.

GitHub Actions (`.github/workflows/build.yml`) builds this automatically on
every push to `main` and uploads it as a workflow artifact — **no GitHub
Release is published automatically**; that only happens once a build has
been manually verified on a real Windows machine, per the project's
distribution policy.

## Use (Milestone 1)

1. Run `RedsStreamCanvas.exe`. Two windows open: **Control** and **Output**.
2. In Control, click **+ Add screen / window** and pick a monitor or window.
3. The picked source should immediately appear, filling the Output window.
4. Watch the diagnostics panel in Control (refreshes ~1×/second) — it reports
   per-source `FrameArrived` counts, valid frames, content size, pixel
   format, copy-texture/SRV creation status, texture copy count, and the
   last HRESULT/error, plus renderer adapter/WARP/feature-level/frame-time
   info.

**If you still see black:** the diagnostics panel is built specifically to
localize it. Send me the panel's text — "0 FrameArrived callbacks" means the
session never started or the source closed; "FrameArrived > 0 but Valid
frames = 0" means `TryGetNextFrame` is returning nothing (frame pool likely
resized against the wrong size); "Valid frames > 0 but Copy texture created =
no" means texture creation itself is failing (check the HRESULT); "everything
yes, texture copies incrementing, but still visibly black" would point at the
compositor draw path instead of capture, which is a different, much smaller
place to look.

## Repository layout

```
src/
    main.cpp
    App.h / App.cpp                Composition root, render thread
    Graphics/
        GraphicsDevice.h/.cpp      The one shared ID3D11Device
        Renderer.h/.cpp            Swap chain + textured-quad compositor
        Shaders.h                  Inline HLSL (vertex + pixel shader)
    Capture/
        CaptureSource.h/.cpp       One WGC item -> stable SRV
        CaptureManager.h/.cpp      Picker flow + source list
    Canvas/
        CanvasModel.h/.cpp         Per-source layout data (dest rect, crop)
    UI/
        ControlWindow.h/.cpp       Editor + diagnostics panel
        OutputWindow.h/.cpp        The clean, capturable output window
.github/workflows/build.yml        CI build -> artifact ZIP
web/                                Static Cloudflare Pages landing site (unchanged)
```

## License

MIT. See `LICENSE`.
