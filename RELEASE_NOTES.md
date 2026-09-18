# Red's Stream Canvas — Alpha 0.2

First runnable public alpha of the lightweight multi-screen compositor.

## What works

- Add multiple monitors or application windows through the Windows capture picker.
- GPU-first Windows Graphics Capture + Direct3D 11/Direct2D composition.
- One output window for use with screen-sharing or streaming applications.
- Select sources directly on the output canvas.
- Drag sources to reposition them.
- Resize from edges and corners.
- Crop sources interactively.
- Hide/show a selected source.
- Bring a source forward or send it backward.
- Remove a selected source.
- Grid, side-by-side, and picture-in-picture layouts.
- Edit mode can be disabled so editor handles are not visible in the shared output.
- Adaptive 30/60 FPS output cadence.
- Self-contained x64 executable using the static MSVC runtime.

## Notes

This is an **alpha** build. It has been compiled and validated by GitHub Actions on Windows, but it is not code-signed yet. Windows SmartScreen may therefore warn when launching it.

No recording, video encoding, streaming engine, microphone capture, or audio mixer is included. Red's Stream Canvas only captures, composites, and displays video sources.
