// events.h — WinEvent hooks + reconcile timer (replaces SLSRegisterNotifyProc).
#pragma once
#include <windows.h>
void events_install(HWND notify_wnd, UINT msg);
void events_uninstall(void);
void reconcile_start(HWND notify_wnd, UINT msg, UINT ms);
void reconcile_stop(void);
// Returns and clears the pending order flag: 1 if any non-move event arrived
// (focus/reorder/show/hide), meaning z-order may have changed.
int events_take_order_flag(void);
