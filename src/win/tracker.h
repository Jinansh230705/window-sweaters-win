// tracker.h — owns HWND -> sweater* table (ports macOS windows.c state machine).
#pragma once
#include <windows.h>
#include "../core/config.h"
void tracker_init(void);
void tracker_shutdown(void); // destroy all overlays, clear table
void tracker_refresh_full(void);   // EnumWindows + prune dead + focus
void tracker_on_hint(void);        // coalesced refresh after WinEvent
void tracker_apply_filter(void);   // drop disallowed, add missing
void tracker_repaint_all(void);
void tracker_reorder(void); // pin every overlay directly below its target
void tracker_set_focus(HWND fg);
HWND tracker_focused(void);
// iteration for tray menu
typedef void (*tracker_each_fn)(HWND hwnd, const char* app, DWORD pid, void* ctx);
void tracker_each(tracker_each_fn fn, void* ctx);
int tracker_count(void);
struct settings* tracker_settings(void);
DWORD tracker_self_pid(void);
