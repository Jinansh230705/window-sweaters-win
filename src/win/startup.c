// startup.c — per-user autostart through the Run registry key.
#include "startup.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

static const char* kValue = "WindowSweaters";
static const char* kRunKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";

int startup_is_enabled(void) {
  HKEY k;
  if (RegOpenKeyExA(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
  char cur[MAX_PATH] = {0}; DWORD n = sizeof cur;
  LONG r = RegQueryValueExA(k, kValue, NULL, NULL, (LPBYTE)cur, &n);
  RegCloseKey(k);
  if (r != ERROR_SUCCESS || !cur[0]) return 0;
  char me[MAX_PATH] = {0};
  GetModuleFileNameA(NULL, me, sizeof me);
  return strstr(cur, me) != NULL;
}

int startup_set(int on) {
  HKEY k;
  if (RegOpenKeyExA(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return 0;
  int ok = 0;
  if (on) {
    char me[MAX_PATH] = {0}, cmd[MAX_PATH + 8] = {0};
    GetModuleFileNameA(NULL, me, sizeof me);
    snprintf(cmd, sizeof cmd, "\"%s\"", me);
    ok = RegSetValueExA(k, kValue, 0, REG_SZ, (const BYTE*)cmd, (DWORD)strlen(cmd) + 1) == ERROR_SUCCESS;
  } else {
    LONG r = RegDeleteValueA(k, kValue);
    ok = (r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND);
  }
  RegCloseKey(k);
  return ok;
}
