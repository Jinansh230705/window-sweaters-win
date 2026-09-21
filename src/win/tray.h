// tray.h — Shell_NotifyIconW tray + popup menu (replaces NSStatusItem menu).
#pragma once
#include <windows.h>
void tray_install(HWND wnd, UINT cb_msg);
void tray_remove(HWND wnd);
void tray_show_menu(HWND wnd);
void tray_update_tip(HWND wnd);
void tray_log(const char* fmt, ...);
// Nonzero while a popup menu is modal. Background sync must pause then:
// TrackPopupMenu blocks the thread, and repainting behind it makes the menu
// feel stuck (clicks queue behind hundred-ms paint storms).
int tray_menu_open(void);
