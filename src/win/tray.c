// tray.c — tray icon + menu: On/Off, Pattern, Stitch, Style, Width, Apps, Quit.
#include "tray.h"
#include "tracker.h"
#include "prefs.h"
#include "startup.h"
#include "../../resource.h"
#include <shellapi.h>
#include "../core/knit_core.h"
#include "../core/charts.h"
#include "../core/apps.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define IDM_ONOFF 1001
#define IDM_WIDER 1003
#define IDM_NARROW 1004
#define IDM_QUIT 1005
#define IDM_APP_BASE 2000
#define IDM_OFFALL 1010
#define IDM_ONALL 1011
#define IDM_STARTUP 1012
#define IDM_PAT_BYAPP 1100
#define IDM_PAT_NONE 1101
#define IDM_PAT_BASE 1200 // + chart index (max 128 charts)
#define IDM_ST_BASE 1300  // + 0 fine / 1 regular / 2 chunky
#define IDM_STYLE_KNIT 1310
#define IDM_STYLE_SOLID 1311
static const float kStitchGauge[] = { 9.f, 6.f, 3.5f };
static const char* kStitchName[] = { "Fine", "Regular", "Chunky" };

static NOTIFYICONDATAW ni;
static int g_menu_open = 0;
int tray_menu_open(void) { return g_menu_open; }

// Debug trail: %TEMP%\ws_tray.log records registration results and every
// callback the shell delivers. If clicks do nothing, this file says whether
// the shell ever delivered them.
#include <stdarg.h>
void tray_log(const char* fmt, ...) {
  char path[MAX_PATH] = {0};
  if (!GetEnvironmentVariableA("TEMP", path, sizeof path)) return;
  size_t n = strlen(path);
  snprintf(path + n, sizeof path - n, "\\ws_tray.log");
  HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, 0, NULL);
  if (h != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER sz = {0};
    if (GetFileSizeEx(h, &sz) && sz.QuadPart > 65536) {
      CloseHandle(h);
      DeleteFileA(path);
      h = INVALID_HANDLE_VALUE;
    } else CloseHandle(h);
  }
  FILE* f = NULL;
  fopen_s(&f, path, "a");
  if (!f) return;
  SYSTEMTIME st;
  GetLocalTime(&st);
  fprintf(f, "[%02d:%02d:%02d build " __DATE__ " " __TIME__ "] ",
          st.wHour, st.wMinute, st.wSecond);
  va_list va;
  va_start(va, fmt);
  vfprintf(f, fmt, va);
  va_end(va);
  fputc('\n', f);
  fclose(f);
}

void tray_install(HWND wnd, UINT cb) {
  memset(&ni, 0, sizeof ni);
  ni.cbSize = sizeof ni; ni.hWnd = wnd; ni.uID = 1; ni.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  ni.uCallbackMessage = cb;
  ni.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_APP));
  if (!ni.hIcon) ni.hIcon = LoadIconW(NULL, IDI_APPLICATION);
  wcscpy_s(ni.szTip, 128, L"Window Sweaters");
  BOOL add = Shell_NotifyIconW(NIM_ADD, &ni);
  tray_log("NIM_ADD hwnd=%p icon=%p ret=%d err=%u", (void*)wnd, (void*)ni.hIcon,
           (int)add, (unsigned)GetLastError());
  ni.uVersion = NOTIFYICON_VERSION_4;
  BOOL ver = Shell_NotifyIconW(NIM_SETVERSION, &ni);
  tray_log("NIM_SETVERSION ret=%d err=%u", (int)ver, (unsigned)GetLastError());
  // First launch only: point the user at the icon, since stock Windows hides
  // new tray icons in the overflow popup.
  if (prefs_take_welcome()) {
    NOTIFYICONDATAW b;
    memset(&b, 0, sizeof b);
    b.cbSize = sizeof b; b.hWnd = wnd; b.uID = 1;
    b.uFlags = NIF_INFO; b.dwInfoFlags = NIIF_INFO;
    wcscpy_s(b.szInfoTitle, 64, L"Window Sweaters");
    wcscpy_s(b.szInfo, 256, L"Running — click the yarn icon for patterns, apps, startup, and quit.");
    Shell_NotifyIconW(NIM_MODIFY, &b);
  }
}
void tray_remove(HWND wnd) { (void)wnd; Shell_NotifyIconW(NIM_DELETE, &ni); }
void tray_update_tip(HWND wnd) { (void)wnd; Shell_NotifyIconW(NIM_MODIFY, &ni); }

struct menu_app { char names[48][64]; int n; };
static void collect(HWND h, const char* app, DWORD pid, void* ctx) {
  (void)h; (void)pid;
  struct menu_app* m = ctx;
  for (int i = 0; i < m->n; i++) if (!_stricmp(m->names[i], app)) return;
  if (m->n < 48) snprintf(m->names[m->n++], 64, "%s", app);
}

void tray_show_menu(HWND wnd) {
  // De-dupe: a single click can arrive as both NIN_SELECT and a button-up.
  // Without this the menu opens, closes, and immediately reopens.
  static ULONGLONG last = 0;
  ULONGLONG now = GetTickCount64();
  if (now - last < 800) return;
  last = now;
  g_menu_open = 1;
  struct settings* st = tracker_settings();
  HMENU menu = CreatePopupMenu();
  AppendMenuA(menu, MF_STRING | (st->enabled ? MF_CHECKED : 0), IDM_ONOFF, st->enabled ? "Sweaters: On" : "Sweaters: Off");
  // Pattern submenu with radio semantics (macOS lists patterns the same way).
  // The old single "By App" toggle was a trap: clicking it while in By App
  // mode silently flipped every window to one global zigzag.
  HMENU pat = CreatePopupMenu();
  AppendMenuA(pat, MF_STRING | (g_knit_pattern_by_app ? MF_CHECKED : 0), IDM_PAT_BYAPP, "By App");
  AppendMenuA(pat, MF_STRING | (!g_knit_pattern_by_app && g_chart_active < 0 ? MF_CHECKED : 0), IDM_PAT_NONE, "None (plain)");
  AppendMenuA(pat, MF_SEPARATOR, 0, NULL);
  for (int i = 0; i < g_chart_count && i < 128; i++) {
    int checked = (!g_knit_pattern_by_app && g_chart_active == i) ? MF_CHECKED : 0;
    AppendMenuA(pat, MF_STRING | checked, IDM_PAT_BASE + i, g_charts[i].name);
  }
  AppendMenuA(menu, MF_POPUP, (UINT_PTR)pat, "Pattern");
  // Stitch size (maps to knit gauge rows, like the Mac menu).
  HMENU st2 = CreatePopupMenu();
  for (int i = 0; i < 3; i++) {
    int checked = fabsf(g_knit.rows - kStitchGauge[i]) < 0.1f ? MF_CHECKED : 0;
    AppendMenuA(st2, MF_STRING | checked, IDM_ST_BASE + i, kStitchName[i]);
  }
  AppendMenuA(menu, MF_POPUP, (UINT_PTR)st2, "Stitch Size");
  // Border style: knitted ring (default) or solid yarn colour.
  HMENU sty = CreatePopupMenu();
  AppendMenuA(sty, MF_STRING | (st->border_style == BORDER_STYLE_KNIT ? MF_CHECKED : 0), IDM_STYLE_KNIT, "Knit");
  AppendMenuA(sty, MF_STRING | (st->border_style != BORDER_STYLE_KNIT ? MF_CHECKED : 0), IDM_STYLE_SOLID, "Solid");
  AppendMenuA(menu, MF_POPUP, (UINT_PTR)sty, "Style");
  AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
  char wbuf[64]; snprintf(wbuf, sizeof wbuf, "Width: %.0fpx", st->border_width);
  AppendMenuA(menu, MF_STRING | MF_DISABLED, 0, wbuf);
  AppendMenuA(menu, MF_STRING, IDM_WIDER, "Wider (+2)");
  AppendMenuA(menu, MF_STRING, IDM_NARROW, "Narrower (-2)");
  AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
  struct menu_app m = {0};
  tracker_each(collect, &m);
  HMENU apps = CreatePopupMenu();
  for (int i = 0; i < m.n; i++) {
    char item[80];
    snprintf(item, sizeof item, "%s%s", knit_app_hidden(m.names[i]) ? "[ ] " : "[x] ", m.names[i]);
    AppendMenuA(apps, MF_STRING, IDM_APP_BASE + i, item);
  }
  if (!m.n) AppendMenuA(apps, MF_STRING | MF_DISABLED, 0, "(no windows)");
  AppendMenuA(apps, MF_SEPARATOR, 0, NULL);
  AppendMenuA(apps, MF_STRING, IDM_ONALL, "Turn On for All Apps");
  AppendMenuA(apps, MF_STRING, IDM_OFFALL, "Turn Off for All Apps");
  AppendMenuA(menu, MF_POPUP, (UINT_PTR)apps, "Apps");
  AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
  AppendMenuA(menu, MF_STRING | (startup_is_enabled() ? MF_CHECKED : 0), IDM_STARTUP, "Run on Startup");
  AppendMenuA(menu, MF_STRING, IDM_QUIT, "Quit Window Sweaters");
  POINT p; GetCursorPos(&p);
  SetForegroundWindow(wnd);
  int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, p.x, p.y, 0, wnd, NULL);
  PostMessageW(wnd, WM_NULL, 0, 0); // required so the next menu activates
  tray_log("menu result cmd=%d", cmd);
  DestroyMenu(menu);
  g_menu_open = 0;
  if (cmd == IDM_QUIT) PostMessageW(wnd, WM_CLOSE, 0, 0);
  else if (cmd == IDM_STARTUP) { startup_set(!startup_is_enabled()); }
  else if (cmd == IDM_ONOFF) { st->enabled = !st->enabled; tracker_repaint_all(); }
  else if (cmd == IDM_PAT_BYAPP) { knit_pattern_select("by-app"); knit_flush_cache(); tracker_repaint_all(); }
  else if (cmd == IDM_PAT_NONE) { knit_pattern_select("none"); knit_flush_cache(); tracker_repaint_all(); }
  else if (cmd >= IDM_PAT_BASE && cmd < IDM_PAT_BASE + g_chart_count) {
    knit_pattern_select(g_charts[cmd - IDM_PAT_BASE].name);
    knit_flush_cache(); tracker_repaint_all();
  }
  else if (cmd >= IDM_ST_BASE && cmd < IDM_ST_BASE + 3) {
    g_knit.rows = kStitchGauge[cmd - IDM_ST_BASE];
    knit_flush_cache(); tracker_repaint_all();
  }
  else if (cmd == IDM_STYLE_KNIT) { st->border_style = BORDER_STYLE_KNIT; tracker_repaint_all(); }
  else if (cmd == IDM_STYLE_SOLID) { st->border_style = BORDER_STYLE_SQUARE; tracker_repaint_all(); }
  else if (cmd == IDM_WIDER) { st->border_width += 2; if (st->border_width > 40) st->border_width = 40; tracker_repaint_all(); }
  else if (cmd == IDM_NARROW) { st->border_width -= 2; if (st->border_width < 4) st->border_width = 4; tracker_repaint_all(); }
  else if (cmd == IDM_ONALL) { knit_apps_set_all(true); tracker_apply_filter(); }
  else if (cmd == IDM_OFFALL) { knit_apps_set_all(false); tracker_apply_filter(); }
  else if (cmd >= IDM_APP_BASE && cmd < IDM_APP_BASE + m.n) {
    const char* app = m.names[cmd - IDM_APP_BASE];
    knit_app_set_hidden(app, !knit_app_hidden(app));
    tracker_apply_filter();
  }
  if (cmd && cmd != IDM_QUIT) prefs_save(); // persist every menu change
}
