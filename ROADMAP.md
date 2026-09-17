# Red's Stream Canvas Roadmap

## Alpha 0.1 — Native pipeline
- [x] Native Win32 shell
- [x] Windows Graphics Capture picker
- [x] Multiple concurrent capture sources
- [x] GPU-only frame path
- [x] One Direct2D/DXGI output window
- [x] Automatic grid layout
- [x] Aspect-ratio preserving scaling
- [x] Basic adaptive 30/60 FPS output cadence
- [x] GitHub Actions Windows build scaffold
- [x] Cloudflare Pages static site scaffold

## Alpha 0.2 — Canvas editor
- [ ] Source cards/list
- [ ] Click-to-select source on canvas
- [ ] Drag source
- [ ] Resize handles
- [ ] Crop handles
- [ ] Layer ordering
- [ ] Hide/show
- [ ] Lock source
- [ ] Snap guides
- [ ] Preset layouts (50/50, PiP, 2x2, focus + sidebar)

## Alpha 0.3 — Performance
- [ ] Frame-time telemetry using rolling percentiles
- [ ] Per-source frame-copy throttling
- [ ] Pause capture work for hidden sources
- [ ] Dirty-region support where beneficial
- [ ] Adapter selection on hybrid-GPU laptops
- [ ] Resolution-aware automatic performance profile
- [ ] Frame-drop and GPU-reset recovery

## Alpha 0.4 — Usability
- [ ] Save/load canvas layouts
- [ ] Fixed canvas resolutions (720p, 1080p, 1440p, custom)
- [ ] Borderless output mode
- [ ] Always-on-top optional output
- [ ] Hotkeys
- [ ] Source relinking after app restart

## Beta
- [ ] Installer / portable ZIP releases
- [ ] Automated versioning and release notes
- [ ] Crash logging that is local/off by default
- [ ] Accessibility pass
- [ ] Multi-DPI testing
- [ ] Windows 10/11 compatibility matrix

## Explicit non-goals
- Video recording
- Stream encoding
- Audio capture/mixing
- Replay buffers
- Plugin ecosystem
- Rebuilding OBS
