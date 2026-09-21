# Window Sweaters for Windows

A Windows tray app that dresses your windows in knitted borders — cosy yarn
rings in colours inspired by your favourite apps. A full port of the macOS
original to native Win32 (C, no frameworks, no runtime).

<img width="1536" height="1024" alt="preview" src="https://github.com/user-attachments/assets/b6a82eb9-d813-410e-9bed-8bcba73f7b35" />

## Features

- **By App colourways** for 40+ apps (VS Code, Chrome, Spotify, Slack, Office,
  Discord, …) plus Windows exe-name mappings (`chrome`, `Code`, `WINWORD`, …).
- Unknown apps borrow yarn from their own icon and get a stable
  zigzag / picnic / twinkle sweater — same as the Mac.
- Global patterns: any of the 47 built-in charts, or plain; your own PNG
  charts in `%APPDATA%\WindowSweaters\charts\` override built-ins by name.
- Per-app on/off that survives restarts, turn-off-for-all, tray menu for
  pattern / stitch size / style / width / run-on-startup / quit.
- Everything persists to `%APPDATA%\WindowSweaters\settings.ini`.

## Get it

Download `WindowSweaters.exe` from [Releases](../../releases) and run it —
no installer, no admin rights. Tick **Run on Startup** in the tray menu to
start it with Windows (per-user, removable from the same menu).

No installer, no admin rights, no Accessibility prompts — it only reads window
rectangles via DWM and draws its own transparent overlays.

## Use it

Click the yarn tray icon (it lives in the notification overflow — the `^`
hidden-icons popup — on stock Windows 11; any click opens the menu):

- **Sweaters: On/Off** — pause everything without quitting.
- **Pattern** — `By App` (default), `None (plain)`, or any chart globally.
- **Stitch Size** — Fine / Regular / Chunky.
- **Style** — Knit / Solid.
- **Width** — Wider / Narrower (±2px, 4–40px).
- **Apps** — tick apps on/off, or turn off/on for all apps. Choices persist.
- **Run on Startup** — start with Windows for this user.
- **Quit Window Sweaters** — graceful shutdown (same teardown every time).
  Scriptable too: `WindowSweaters.exe quit` closes the running instance.

## Make it yours

`apps.conf` (created on first run with examples):

```
Claude = #D58561 atelier-claude
Spotify = #3B6450 atelier-spotify
```

Names match exe-basename prefixes, case-insensitive, longest wins. Optional
startup script at `%USERPROFILE%\.sweatersrc` runs once at launch (same idea
as the Mac's `sweatersrc`).

A second instance forwards CLI args to the running one
(`WindowSweaters.exe width=14 chart=zigzag knit=off`):

| Key | Example |
|---|---|
| `width=` | `width=14` |
| `style=` | `style=k` knit, `style=s` solid |
| `chart=` | `chart=zigzag`, `chart=by-app`, `chart=none` |
| `yarn=` / `basket=` | `yarn=garter`, `basket=wool` |
| `gauge=` / `dim=` | `gauge=6`, `dim=0.2` |
| `ground=` `ambient=` `relief=` `sheen=` `tuck=` | fine-tune the wool |
| `anchor=` | `anchor=corner` / `anchor=centre` |
| `apps=reload` / `charts=reload` | reload configs without restart |
| `blacklist=` / `whitelist=` | `blacklist=foo,bar` |
| `order=` / `hidpi=` | `order=a`, `hidpi=on` |
| `quit` | gracefully close the running instance |

## Build it yourself

Prerequisites: [VS Build Tools](https://visualstudio.microsoft.com/downloads/)
(C++ workload) + Windows 10/11 SDK. No CMake, vcpkg, or dependencies.

```
git clone <this-repo>
cd window-sweaters-win
build.bat
```

Produces `out\WindowSweaters.exe` and runs the headless render test
(`out\tile_test.exe` → `out\tile_test.bmp`). Every push is also built by
[GitHub Actions](.github/workflows/build.yml); pushing a `v*` tag publishes a
Release zip automatically.

## Porting notes

| macOS (upstream) | Windows (here) |
|---|---|
| SkyLight private-API overlays (`border.c`) | Layered `WS_EX_TRANSPARENT` HWND + `UpdateLayeredWindow`, pinned below target |
| `CGPath`/`CGBitmapContext` tile renderer (`knit.c`) | Same CPU maths → raw ARGB tiles, GDI ring painter |
| `CGImageSource` PNG charts (`chart.c`) | WIC decoder, same flags/semantics |
| `SLSRegisterNotifyProc` events (`events.c`) | `SetWinEventHook` + 500ms `EnumWindows` safety net |
| `CGWindowListCopyWindowInfo` heal (`reconcile.c`) | Same algorithm over `EnumWindows` + `DWMWA_CLOAKED` |
| `NSStatusItem` menu (`menubar.m`) | `Shell_NotifyIconW` + popup menus |
| `NSRunningApplication` icons (`autoyarn.m`) | `SHGetFileInfo` icon → same HSL soften |
| Bundle-ID visibility gate (`hidden.m`) | Same default+exceptions model, keyed by exe basename |
| Mach bootstrap IPC (`mach.c`) | Mutex + named pipe, same `key=value` protocol |
| `NSUserDefaults` persistence | `settings.ini` in `%APPDATA%\WindowSweaters` |
| `CVDisplayLink` vsync (`animation.c`) | Dropped — knit is static, repaints are on-demand |

**Known gaps vs the Mac app:** fixed 9px corner radius (Windows exposes no
per-window radius); knit + solid styles only (no glow/gradient); Zigzag uses
the collection cream rather than a per-app deepened shade on pale apps;
unmatched apps share three fallback charts instead of generated per-icon
clones; no `yabai` bridge; resize repaints at ~20fps instead of hiding.

## Contributing

`build.bat` must stay green (app + render test). Keep the `src/core`
(portable knit logic) free of Win32 calls; Windows specifics belong in
`src/win`. Upstream pattern data changes should be ported cell-for-cell from
the original repo's `src/chart.c` / `src/apps.c`.

## Credits

- **Original app:** [Window Sweaters](https://github.com/saragordic/window-sweaters)
  by [Sara Gordic](https://github.com/saragordic) (macOS, GPL-3.0). All pattern
  artwork, colourways, and knit-shading maths originate there.
- The original is built on [JankyBorders](https://github.com/FelixKratz/JankyBorders)
  by Felix Kratz — see [NOTICE.md](NOTICE.md).
- This port reuses the upstream collection data, palettes, matching logic and
  shading algorithms verbatim where possible, and re-implements only the
  Apple-only shell (SkyLight overlays, AppKit menu, Mach IPC) with Windows
  natives. Licensed under the same [GPL-3.0](LICENSE).

## License

[GPL-3.0](LICENSE), same as the original. See [NOTICE.md](NOTICE.md) for
attribution.
