// prefs.c — INI persistence via Get/WritePrivateProfileStringA.
#include "prefs.h"
#include "tracker.h"
#include "../core/knit_core.h"
#include "../core/charts.h"
#include "../core/apps.h"
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static void ini_path(char* out, size_t n) {
  out[0] = 0;
  wchar_t* base = NULL;
  if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &base))) {
    char tmp[MAX_PATH] = {0};
    WideCharToMultiByte(CP_UTF8, 0, base, -1, tmp, sizeof tmp, NULL, NULL);
    CoTaskMemFree(base);
    snprintf(out, n, "%s\\WindowSweaters", tmp);
    CreateDirectoryA(out, NULL);
    snprintf(out, n, "%s\\WindowSweaters\\settings.ini", tmp);
  }
}

void prefs_load(struct settings* st) {
  char ini[MAX_PATH];
  ini_path(ini, sizeof ini);
  if (!*ini) return;
  st->enabled = GetPrivateProfileIntA("sweaters", "enabled", st->enabled ? 1 : 0, ini) != 0;
  int w = GetPrivateProfileIntA("sweaters", "width", (int)st->border_width, ini);
  if (w >= 4 && w <= 40) st->border_width = (float)w;
  char buf[256] = {0};
  GetPrivateProfileStringA("sweaters", "pattern", "by-app", buf, sizeof buf, ini);
  knit_pattern_select(buf);
  GetPrivateProfileStringA("sweaters", "basket", "", buf, sizeof buf, ini);
  for (int k = 0; k < g_knit_basket_count; k++)
    if (strcmp(buf, g_knit_baskets[k].name) == 0) g_knit_basket = k;
  GetPrivateProfileStringA("sweaters", "stitch", "", buf, sizeof buf, ini);
  for (int k = 0; k < KNIT_STITCH_COUNT; k++)
    if (strcmp(buf, g_knit_stitch_names[k]) == 0) g_knit_stitch = k;
  GetPrivateProfileStringA("sweaters", "style", "", buf, sizeof buf, ini);
  if (buf[0] == 'k' || buf[0] == 's') st->border_style = buf[0];
  GetPrivateProfileStringA("sweaters", "dim", "", buf, sizeof buf, ini);
  if (sscanf_s(buf, "%f", &g_knit_dim) != 1) {}
  GetPrivateProfileStringA("sweaters", "gauge", "", buf, sizeof buf, ini);
  { float g = 0; if (sscanf_s(buf, "%f", &g) == 1 && g >= 1.5f && g <= 12.f) g_knit.rows = g; }
  g_knit_on = GetPrivateProfileIntA("sweaters", "knit", g_knit_on ? 1 : 0, ini) != 0;
  int on_by_default = GetPrivateProfileIntA("apps", "on_by_default", 1, ini) != 0;
  knit_apps_set_all(on_by_default ? true : false);
  GetPrivateProfileStringA("apps", "exceptions", "", buf, sizeof buf, ini);
  char* ctx = NULL;
  for (char* t = strtok_s(buf, ",", &ctx); t; t = strtok_s(NULL, ",", &ctx)) {
    while (*t == ' ') t++;
    if (*t) knit_app_set_hidden(t, on_by_default ? true : false);
  }
  knit_flush_cache();
}

void prefs_save(void) {
  char ini[MAX_PATH];
  ini_path(ini, sizeof ini);
  if (!*ini) return;
  struct settings* st = tracker_settings();
  char buf[64];
  WritePrivateProfileStringA("sweaters", "enabled", st->enabled ? "1" : "0", ini);
  snprintf(buf, sizeof buf, "%d", (int)st->border_width);
  WritePrivateProfileStringA("sweaters", "width", buf, ini);
  if (g_knit_pattern_by_app) snprintf(buf, sizeof buf, "by-app");
  else if (g_chart_active >= 0 && g_chart_active < g_chart_count) snprintf(buf, sizeof buf, "%s", g_charts[g_chart_active].name);
  else snprintf(buf, sizeof buf, "none");
  WritePrivateProfileStringA("sweaters", "pattern", buf, ini);
  WritePrivateProfileStringA("sweaters", "basket", g_knit_baskets[g_knit_basket].name, ini);
  WritePrivateProfileStringA("sweaters", "stitch", g_knit_stitch_names[g_knit_stitch], ini);
  snprintf(buf, sizeof buf, "%c", st->border_style);
  WritePrivateProfileStringA("sweaters", "style", buf, ini);
  snprintf(buf, sizeof buf, "%.3f", g_knit_dim);
  WritePrivateProfileStringA("sweaters", "dim", buf, ini);
  snprintf(buf, sizeof buf, "%.2f", g_knit.rows);
  WritePrivateProfileStringA("sweaters", "gauge", buf, ini);
  WritePrivateProfileStringA("sweaters", "knit", g_knit_on ? "1" : "0", ini);
  int def = knit_apps_on_by_default() ? 1 : 0;
  WritePrivateProfileStringA("apps", "on_by_default", def ? "1" : "0", ini);
  char exc[2048] = {0}; size_t o = 0;
  for (int i = 0; i < knit_app_exception_count(); i++) {
    const char* e = knit_app_exception(i);
    size_t n = strlen(e);
    if (o + n + 2 < sizeof exc) { if (o) exc[o++] = ','; memcpy(exc + o, e, n); o += n; }
  }
  WritePrivateProfileStringA("apps", "exceptions", exc, ini);
}
