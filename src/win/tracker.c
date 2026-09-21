// tracker.c — EnumWindows-based window tracking (replaces SLS queries).
#include "tracker.h"
#include "overlay.h"
#include "../core/table.h"
#include "../core/apps.h"
#include "../core/knit_core.h"
#include <dwmapi.h>
#include <stdlib.h>
#include <stdio.h>

struct settings g_settings = { .enabled = 1, .border_width = 12.f,
  .border_style = BORDER_STYLE_KNIT, .hidpi = 1, .border_order = BORDER_ORDER_BELOW };
static struct table g_map; // key: HWND (as uint64), value: sweater*
static DWORD g_self = 0;
static HWND g_focused = NULL;

static unsigned long hash_hwnd(void* k) { return (unsigned long)(uintptr_t)(*(HWND*)k); }
static int cmp_hwnd(void* a, void* b) { return *(HWND*)a == *(HWND*)b; }
static unsigned long hash_str(void* k) { unsigned long h = 5381; char c; char* s = k; while ((c = *s++)) h = ((h<<5)+h)+(unsigned char)c; return h; }
static int cmp_str(void* a, void* b) { return strcmp((char*)a, (char*)b) == 0; }

struct settings* tracker_settings(void) { return &g_settings; }
DWORD tracker_self_pid(void) { return g_self; }
HWND tracker_focused(void) { return g_focused; }

static int is_own_overlay(HWND h) {
  wchar_t cls[64] = {0};
  GetClassNameW(h, cls, 63);
  return wcscmp(cls, L"WindowSweatersOverlay") == 0;
}
static int app_allowed(const char* app, DWORD pid) {
  if (knit_pid_hidden((int)pid)) return 0;
  if (g_settings.whitelist_enabled && !table_find(&g_settings.whitelist, (void*)app)) return 0;
  if (g_settings.blacklist_enabled && table_find(&g_settings.blacklist, (void*)app)) return 0;
  return 1;
}
static int exe_of(HWND h, char* app, size_t cap, DWORD* pid) {
  DWORD p = 0;
  GetWindowThreadProcessId(h, &p);
  if (pid) *pid = p;
  if (p == 0 || p == g_self) return 0;
  HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p);
  if (!hp) return 0;
  char path[MAX_PATH] = {0}; DWORD n = sizeof path;
  int ok = QueryFullProcessImageNameA(hp, 0, path, &n)
    && knit_app_name_from_executable(path, app, cap) && *app;
  CloseHandle(hp);
  return ok;
}
static int suitable(HWND h) {
  if (!IsWindow(h) || !IsWindowVisible(h) || IsIconic(h)) return 0;
  if (is_own_overlay(h)) return 0;
  if (GetWindow(h, GW_OWNER)) return 0;
  LONG ex = GetWindowLongA(h, GWL_EXSTYLE);
  if (ex & WS_EX_TOOLWINDOW) return 0;
  wchar_t cls[64] = {0};
  GetClassNameW(h, cls, 63);
  if (!wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW") || !wcscmp(cls, L"Shell_TrayWnd")
      || !wcscmp(cls, L"Shell_SecondaryTrayWnd") || !wcscmp(cls, L"Button")) return 0;
  int cloaked = 0;
  DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof cloaked);
  if (cloaked) return 0;
  RECT r = {0};
  if (FAILED(DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof r))) GetWindowRect(h, &r);
  if (r.right - r.left < 24 || r.bottom - r.top < 24) return 0;
  return 1;
}

struct add_ctx { int added; };
static BOOL CALLBACK enum_cb(HWND h, LPARAM lp) {
  if (!suitable(h)) return TRUE;
  char app[64] = {0}; DWORD pid = 0;
  if (!exe_of(h, app, sizeof app, &pid)) return TRUE;
  if (!app_allowed(app, pid)) return TRUE;
  if (!table_find(&g_map, &h)) {
    overlay_register();
    struct sweater* s = sweater_create(h, app, pid);
    if (s) { table_add(&g_map, &h, s); }
  } else {
    struct sweater* s = table_find(&g_map, &h);
    if (s && strcmp(s->app, app) != 0) { snprintf(s->app, sizeof s->app, "%s", app); s->pid = pid; s->needs_paint = 1; }
  }
  return TRUE;
}

void tracker_init(void) {
  g_self = GetCurrentProcessId();
  table_init(&g_map, 256, hash_hwnd, cmp_hwnd);
  table_init(&g_settings.blacklist, 64, hash_str, cmp_str);
  table_init(&g_settings.whitelist, 64, hash_str, cmp_str);
  overlay_register();
}

void tracker_shutdown(void) {
  for (int i = 0; i < g_map.capacity; i++)
    for (struct bucket* b = g_map.buckets[i]; b; b = b->next) {
      struct sweater* s = (struct sweater*)b->value;
      if (s) { if (s->visible) sweater_hide(s); sweater_destroy(s); b->value = NULL; }
    }
  table_clear(&g_map);
}

void tracker_set_focus(HWND fg) { g_focused = fg; }

void tracker_refresh_full(void) {
  EnumWindows(enum_cb, 0);
  // prune dead / unsuitable / disallowed
  int n = g_map.count;
  HWND* dead = n ? (HWND*)malloc(sizeof(HWND) * n) : NULL;
  int nd = 0;
  for (int i = 0; i < g_map.capacity; i++)
    for (struct bucket* b = g_map.buckets[i]; b; b = b->next) {
      struct sweater* s = (struct sweater*)b->value;
      HWND h = *(HWND*)b->key;
      if (!IsWindow(h) || !suitable(h)) { dead[nd++] = h; continue; }
      char app[64] = {0}; DWORD pid = 0;
      if (!exe_of(h, app, sizeof app, &pid) || !app_allowed(app, pid)) dead[nd++] = h;
    }
  for (int i = 0; i < nd; i++) {
    struct sweater* s = table_find(&g_map, &dead[i]);
    if (s) sweater_destroy(s);
    table_remove(&g_map, &dead[i]);
  }
  free(dead);
  HWND fg = GetForegroundWindow();
  g_focused = fg;
  for (int i = 0; i < g_map.capacity; i++)
    for (struct bucket* b = g_map.buckets[i]; b; b = b->next) {
      struct sweater* s = (struct sweater*)b->value;
      sweater_sync(s, &g_settings, s->target == fg);
    }
  tracker_reorder();
}
// coalesced: events call this; it just does a light sync (no full enum) then
// the reconcile timer does full enum periodically. No reorder here — the
// caller reorders only when a z-order-affecting event arrived.
void tracker_on_hint(void) {
  HWND fg = GetForegroundWindow();
  g_focused = fg;
  for (int i = 0; i < g_map.capacity; i++)
    for (struct bucket* b = g_map.buckets[i]; b; b = b->next) {
      struct sweater* s = (struct sweater*)b->value;
      if (!IsWindow(s->target)) continue;
      sweater_sync(s, &g_settings, s->target == fg);
    }
}

// Z-order mirror: EnumWindows yields top-to-bottom, so pinning each overlay
// directly below its target in that order interleaves them correctly
// (A, ovA, B, ovB, ...) and a background ring can never cover a foreground
// window. Cheap: one SetWindowPos per visible overlay, no repaint.
static BOOL CALLBACK reorder_cb(HWND h, LPARAM lp) {
  (void)lp;
  struct sweater* s = table_find(&g_map, &h);
  if (s) sweater_place_below(s);
  return TRUE;
}
void tracker_reorder(void) { EnumWindows(reorder_cb, 0); }
void tracker_apply_filter(void) {
  int n = g_map.count;
  HWND* dead = n ? (HWND*)malloc(sizeof(HWND) * n) : NULL;
  int nd = 0;
  for (int i = 0; i < g_map.capacity; i++)
    for (struct bucket* b = g_map.buckets[i]; b; b = b->next) {
      struct sweater* s = (struct sweater*)b->value;
      if (s && !app_allowed(s->app, s->pid)) dead[nd++] = *(HWND*)b->key;
    }
  for (int i = 0; i < nd; i++) {
    struct sweater* s = table_find(&g_map, &dead[i]);
    if (s) sweater_destroy(s);
    table_remove(&g_map, &dead[i]);
  }
  free(dead);
  tracker_refresh_full();
}
void tracker_repaint_all(void) {
  for (int i = 0; i < g_map.capacity; i++)
    for (struct bucket* b = g_map.buckets[i]; b; b = b->next) {
      struct sweater* s = (struct sweater*)b->value;
      if (s) { s->needs_paint = 1; sweater_sync(s, &g_settings, s->target == g_focused); }
    }
}
void tracker_each(tracker_each_fn fn, void* ctx) {
  for (int i = 0; i < g_map.capacity; i++)
    for (struct bucket* b = g_map.buckets[i]; b; b = b->next) {
      struct sweater* s = (struct sweater*)b->value;
      if (s) fn(s->target, s->app, s->pid, ctx);
    }
}
int tracker_count(void) { return g_map.count; }
