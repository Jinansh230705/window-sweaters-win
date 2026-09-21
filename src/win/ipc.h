// ipc.h — single instance (mutex) + named-pipe CLI forwarding (replaces Mach).
#pragma once
#include <windows.h>
int ipc_claim_single(void); // 1 = we own it, 0 = another instance runs
void ipc_forward_args(int argc, char** argv);
void ipc_serve_begin(void); // background thread applying width=/chart=/etc
void ipc_set_quit_window(HWND wnd); // "quit" arg posts WM_CLOSE here
