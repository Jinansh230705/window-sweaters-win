// main_win.c — wWinMain bootstrap (replaces macOS main.c NSApplication path).
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include "tracker.h"
#include "events.h"
#include "tray.h"
#include "ipc.h"
#include "prefs.h"
#include "../core/knit_core.h"
#include "../core/charts.h"
#include "../core/apps.h"

#define WM_TRAY (WM_APP + 1)
#define WM_HINT (WM_APP + 2)

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
    case WM_TRAY:
      // The icon lives in the notification overflow ("hidden icons") on
      // stock Windows 11; any click — left, right, or double — opens the
      // menu, whose last item quits the app.
      if (l == WM_RBUTTONUP || l == WM_LBUTTONUP || l == WM_LBUTTONDBLCLK) tray_show_menu(h);
      return 0;
    case WM_HINT: {
      // Collapse event storms: one sync per pump, never a backlog. Reorder
      // only when a focus/reorder/show/hide event arrived — pure
      // move/resize drags skip the z-order pass entirely.
      MSG m2;
      while (PeekMessageW(&m2, h, WM_HINT, WM_HINT, PM_REMOVE)) {}
      tracker_on_hint();
      if (events_take_order_flag()) tracker_reorder();
      return 0;
    }
    case WM_TIMER:
      tracker_on_hint(); // light sync only; full EnumWindows lives in reconcile
      return 0;
    case WM_CLOSE:
      // Explicit teardown first so Quit is instant even under an event storm:
      // stop timers/hooks, destroy every overlay, remove the tray icon.
      prefs_save();
      reconcile_stop();
      events_uninstall();
      KillTimer(h, 1);
      tracker_shutdown();
      tray_remove(h);
      PostQuitMessage(0);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default: return DefWindowProcW(h, m, w, l);
  }
}

static void run_sweatersrc(void) {
  // Optional startup script: %USERPROFILE%\.sweatersrc (kept for parity).
  char home[MAX_PATH] = {0};
  DWORD n = GetEnvironmentVariableA("USERPROFILE", home, sizeof home);
  if (!n) return;
  char path[MAX_PATH];
  snprintf(path, sizeof path, "%s\\.sweatersrc", home);
  DWORD attr = GetFileAttributesA(path);
  if (attr == INVALID_FILE_ATTRIBUTES) return;
  STARTUPINFOA si = { sizeof si }; PROCESS_INFORMATION pi = {0};
  char cmd[MAX_PATH + 16];
  snprintf(cmd, sizeof cmd, "cmd /c \"%s\"", path);
  if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
  }
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show) {
  (void)prev; (void)cmd; (void)show;
  // CLI: forward to running instance if present
  int argc = 0; LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
  char* argv[64] = {0}; char abuf[64][256] = {0}; int ac = 0;
  for (int i = 0; i < argc && i < 64; i++) {
    WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, abuf[i], 256, NULL, NULL);
    argv[ac++] = abuf[i];
  }
  for (int i = 1; i < ac; i++) {
    if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) {
      MessageBoxW(NULL, L"Window Sweaters 1.5.0 (Windows port)", L"Window Sweaters", MB_OK);
      return 0;
    }
    if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
      MessageBoxW(NULL, L"Window Sweaters: tray app. CLI: width=N chart=NAME knit=on/off basket=NAME yarn=NAME dim=F gauge=F",
        L"Window Sweaters", MB_OK);
      return 0;
    }
  }
  int owns = ipc_claim_single();
  if (!owns) { ipc_forward_args(ac, argv); return 0; }
  // Lone "quit" with no running instance: nothing to close, just exit.
  if (ac == 2 && strcmp(argv[1], "quit") == 0) return 0;

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

  tracker_init();
  knit_charts_load(knit_charts_dir());
  knit_apps_load();
  prefs_load(tracker_settings()); // restored prefs; CLI args below still win
  uint32_t mask = parse_settings(tracker_settings(), ac - 1, argv + 1);
  (void)mask;

  WNDCLASSW c = {0};
  c.lpfnWndProc = wndproc; c.hInstance = inst; c.lpszClassName = L"WindowSweatersMsg";
  RegisterClassW(&c);
  HWND msg = CreateWindowW(L"WindowSweatersMsg", L"", 0, 0, 0, 0, 0, NULL, NULL, inst, NULL);

  tray_install(msg, WM_TRAY);
  events_install(msg, WM_HINT);
  reconcile_start(msg, WM_HINT, 500); // full EnumWindows safety net, 2Hz
  ipc_set_quit_window(msg);
  ipc_serve_begin();
  run_sweatersrc();
  tracker_refresh_full();

  MSG m;
  while (GetMessageW(&m, NULL, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }

  reconcile_stop();
  events_uninstall();
  tray_remove(msg);
  CoUninitialize();
  return 0;
}
