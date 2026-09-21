// events.c — SetWinEventHook fan-in + 200ms EnumWindows safety net
// (ports events.c debounce + reconcile.c snapshot healing).
#include "events.h"
#include "tracker.h"

static HWINEVENTHOOK hooks[4];
static HWND g_wnd = NULL; static UINT g_msg = 0;
static UINT_PTR g_timer = 0;
// Set when a z-order-affecting event arrives; move-only drags leave it clear
// so the pump can skip the reorder pass while resizing.
static volatile LONG g_order_pending = 0;

static void post_hint(int order) {
  if (!g_wnd || !g_msg) return;
  if (order) InterlockedOr((LONG*)&g_order_pending, 1);
  PostMessageW(g_wnd, g_msg, 0, 0);
}
int events_take_order_flag(void) {
  return InterlockedExchange((LONG*)&g_order_pending, 0) != 0;
}
static void CALLBACK hook_cb(HWINEVENTHOOK h, DWORD ev, HWND hwnd, LONG obj, LONG ch,
    DWORD tid, DWORD tm) {
  (void)h; (void)tid; (void)tm;
  if (!hwnd || obj != OBJID_WINDOW) return;
  switch (ev) {
    case EVENT_OBJECT_LOCATIONCHANGE:
      post_hint(0); break; // pure move/resize: sync positions only
    case EVENT_OBJECT_CREATE: case EVENT_OBJECT_DESTROY:
    case EVENT_OBJECT_SHOW: case EVENT_OBJECT_HIDE:
    case EVENT_OBJECT_CLOAKED: case EVENT_OBJECT_UNCLOAKED:
    case EVENT_OBJECT_NAMECHANGE: case EVENT_OBJECT_REORDER:
    case EVENT_SYSTEM_FOREGROUND: case EVENT_SYSTEM_MINIMIZESTART:
    case EVENT_SYSTEM_MINIMIZEEND: case EVENT_SYSTEM_DESKTOPSWITCH:
      post_hint(1); break;
    default: break;
  }
}
void events_install(HWND wnd, UINT msg) {
  g_wnd = wnd; g_msg = msg;
  hooks[0] = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_DESKTOPSWITCH,
    NULL, hook_cb, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
  hooks[1] = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
    NULL, hook_cb, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
  hooks[2] = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_UNCLOAKED,
    NULL, hook_cb, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
  hooks[3] = SetWinEventHook(EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_REORDER,
    NULL, hook_cb, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
}
void events_uninstall(void) {
  for (int i = 0; i < 4; i++) if (hooks[i]) { UnhookWinEvent(hooks[i]); hooks[i] = NULL; }
}
static VOID CALLBACK timer_cb(HWND h, UINT m, UINT_PTR id, DWORD t) {
  (void)h; (void)m; (void)id; (void)t;
  tracker_refresh_full(); // safety net for missed notifications
}
void reconcile_start(HWND wnd, UINT msg, UINT ms) {
  (void)wnd; (void)msg;
  reconcile_stop();
  g_timer = SetTimer(NULL, 0, ms, timer_cb);
}
void reconcile_stop(void) {
  if (g_timer) { KillTimer(NULL, g_timer); g_timer = 0; }
}
