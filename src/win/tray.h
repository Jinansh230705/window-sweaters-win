// tray.h — Shell_NotifyIconW tray + popup menu (replaces NSStatusItem menu).
#pragma once
#include <windows.h>
void tray_install(HWND wnd, UINT cb_msg);
void tray_remove(HWND wnd);
void tray_show_menu(HWND wnd);
void tray_update_tip(HWND wnd);
