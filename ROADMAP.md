# Red's Stream Canvas roadmap

## Alpha 0.2 — editable canvas

Implemented in this milestone:

- Native Windows capture using Windows Graphics Capture.
- Multiple simultaneous screen/window sources.
- GPU-to-GPU frame copies and Direct2D composition.
- One clean output window for any app that can capture a window.
- Select sources directly on the output canvas.
- Drag sources to reposition them.
- Resize from edge and corner handles.
- Crop from edge handles in Crop mode.
- Hide/show selected sources.
- Bring selected sources to front or send them to back.
- Grid, side-by-side, and picture-in-picture layout presets.
- Reset crop for the selected source.
- Edit overlay can be turned off before streaming so guides are not visible.
- Keyboard shortcuts for common editor actions.
- Conservative adaptive 30/60 FPS output cadence.

## Next milestones

### Alpha 0.3 — persistence and polish

- Save and load canvas layouts.
- Source list with names, visibility state, and ordering.
- Fixed logical canvas sizes such as 720p and 1080p independent of window size.
- Snap-to-edge and alignment guides.
- Lock source position/size.
- Better cursor feedback for move/resize/crop handles.
- Per-source FPS caps for static/secondary content.
- More detailed performance telemetry and auto-quality decisions.

### Beta

- Portable release ZIP and optional installer.
- Automatic release builds and checksums.
- Optional signed binaries when practical.
- Cloudflare Pages download/guide site.
- Compatibility testing across Intel, AMD, NVIDIA, integrated graphics, Windows 10, and Windows 11.

## Non-goals

Red's Stream Canvas is not intended to become a full recorder or streaming suite. Recording, video encoding, audio mixing, replay buffers, effects chains, and plugin ecosystems remain out of scope unless a future feature can be added without compromising the lightweight multi-screen-compositor goal.
