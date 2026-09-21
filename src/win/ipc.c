// ipc.c — Global mutex + \\.\pipe\WindowSweaters line protocol.
#include "ipc.h"
#include "tracker.h"
#include "../core/knit_core.h"
#include <stdio.h>
#include <string.h>
static HANDLE g_mutex = NULL;
static HWND g_quit_wnd = NULL;
void ipc_set_quit_window(HWND w) { g_quit_wnd = w; }
int ipc_claim_single(void) {
  g_mutex = CreateMutexW(NULL, TRUE, L"Global\\WindowSweaters");
  return GetLastError() != ERROR_ALREADY_EXISTS;
}
void ipc_forward_args(int argc, char** argv) {
  if (argc < 2) return;
  char buf[2048] = {0}; size_t o = 0;
  for (int i = 1; i < argc && o + strlen(argv[i]) + 2 < sizeof buf; i++) {
    size_t n = strlen(argv[i]); memcpy(buf + o, argv[i], n); o += n; buf[o++] = '\n';
  }
  HANDLE p = CreateFileW(L"\\\\.\\pipe\\WindowSweaters", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (p != INVALID_HANDLE_VALUE) { DWORD w = 0; WriteFile(p, buf, (DWORD)o, &w, NULL); CloseHandle(p); }
}
static DWORD WINAPI serve(LPVOID p) {
  (void)p;
  for (;;) {
    HANDLE pipe = CreateNamedPipeW(L"\\\\.\\pipe\\WindowSweaters", PIPE_ACCESS_INBOUND,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) { Sleep(1000); continue; }
    if (ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
      char buf[2048] = {0}; DWORD r = 0;
      if (ReadFile(pipe, buf, sizeof buf - 1, &r, NULL) && r) {
        // split lines -> parse_settings
        char* lines[64]; int n = 0;
        for (char* s = buf; *s && n < 64;) {
          while (*s == '\n' || *s == '\r') *s++ = 0;
          if (!*s) break;
          lines[n++] = s;
          while (*s && *s != '\n' && *s != '\r') s++;
        }
        // Scriptable close: "quit" goes through the same teardown as the
        // tray menu instead of killing the overlays mid-paint.
        int quit = 0;
        for (int i = 0; i < n; i++) if (strcmp(lines[i], "quit") == 0) quit = 1;
        if (quit) {
          if (g_quit_wnd) PostMessageW(g_quit_wnd, WM_CLOSE, 0, 0);
        } else {
          uint32_t mask = parse_settings(tracker_settings(), n, lines);
          if (mask & BORDER_UPDATE_MASK_RECREATE_ALL) tracker_apply_filter();
          else if (mask) tracker_repaint_all();
        }
      }
    }
    DisconnectNamedPipe(pipe); CloseHandle(pipe);
  }
  return 0;
}
void ipc_serve_begin(void) {
  CreateThread(NULL, 0, serve, NULL, 0, NULL);
}
