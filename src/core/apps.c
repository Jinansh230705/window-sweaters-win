#include "apps.h"
#include "charts.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct app_rule g_app_rules[KNIT_APP_RULES_MAX];
int g_app_rule_count = 0;
bool g_knit_pattern_by_app = true;

// Built-in collection: macOS names kept + Windows exe-basenames added so the
// same yarn finds the app on either OS.
static const struct app_rule k_collection[] = {
  { "Finder",          0xff258de0u, "atelier-finder" },
  { "explorer",        0xff258de0u, "atelier-finder" },
  { "Microsoft Teams", 0xff9283c1u, "atelier-teams" },
  { "ms-teams",        0xff9283c1u, "atelier-teams" },
  { "Teams",           0xff9283c1u, "atelier-teams" },
  { "Claude",          0xffd58561u, "atelier-claude" },
  { "Codex",           0xff0a84ffu, "atelier-chatgpt" },
  { "Spotify",         0xff497641u, "atelier-spotify" },
  { "Notion",          0xfff5f5f2u, "atelier-notion" },
  { "WhatsApp",        0xff6ea77bu, "atelier-whatsapp" },
  { "Figma",           0xffad7197u, "atelier-figma" },
  { "Google Chrome",   0xfff4f0e6u, "atelier-chrome" },
  { "chrome",          0xfff4f0e6u, "atelier-chrome" },
  { "Paper",           0xff83ade8u, "atelier-paper" },
  { "Safari", 0xff268ed8u, "atelier-safari" },
  { "firefox", 0xff643a9au, "atelier-firefox" },
  { "Cursor", 0xff26251eu, "atelier-cursor" },
  { "Slack", 0xff542a52u, "atelier-slack" },
  { "slack", 0xff542a52u, "atelier-slack" },
  { "zoom", 0xff2877ebu, "atelier-zoom" },
  { "Zoom", 0xff2877ebu, "atelier-zoom" },
  { "Telegram", 0xff389eceu, "atelier-telegram" },
  { "telegram", 0xff389eceu, "atelier-telegram" },
  { "Messages", 0xff55af51u, "atelier-messages" },
  { "Mail", 0xff2986ceu, "atelier-mail" },
  { "outlook", 0xff176bb7u, "atelier-outlook" },
  { "Notes", 0xfff6f0d9u, "atelier-notes" },
  { "notepad", 0xfff6f0d9u, "atelier-notes" },
  { "Calendar", 0xfff7f3e9u, "atelier-calendar" },
  { "Reminders", 0xfff6f3ebu, "atelier-reminders" },
  { "Music", 0xffe64e70u, "atelier-music" },
  { "Photos", 0xfff8f0dbu, "atelier-photos" },
  { "mspaint", 0xfff8f0dbu, "atelier-photos" },
  { "Preview", 0xff597bafu, "atelier-preview" },
  { "WINWORD", 0xff2855a1u, "atelier-word" },
  { "EXCEL", 0xff28674fu, "atelier-excel" },
  { "POWERPNT", 0xffb9573du, "atelier-powerpoint" },
  { "OUTLOOK", 0xff176bb7u, "atelier-outlook" },
  { "Code", 0xff237cafu, "atelier-vscode" },
  { "Photoshop", 0xff182f45u, "atelier-photoshop" },
  { "Illustrator", 0xff4e3029u, "atelier-illustrator" },
  { "ChatGPT", 0xff0a84ffu, "atelier-chatgpt" },
  { "Discord", 0xff5865f2u, "checker" },
  { "Granola", 0xff292e2au, "atelier-granola" },
  { "WindowsTerminal", 0xff303c35u, "atelier-terminal" },
  { "wt", 0xff303c35u, "atelier-terminal" },
  { "ghostty", 0xff2b3350u, "atelier-ghostty" },
};

static const char* k_default_conf =
  "# Window Sweaters (Windows) - per-app colourways\n"
  "# <app name> = #RRGGBB [chart]\n"
  "# Matched as case-insensitive prefix of the exe basename, longest wins.\n"
  "# Example:\n"
  "# Claude = #D58561 atelier-claude\n";

const char* knit_apps_path(void) {
  static char path[MAX_PATH];
  if (path[0]) return path;
  wchar_t* base = NULL;
  if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &base))) {
    char tmp[MAX_PATH] = {0};
    WideCharToMultiByte(CP_UTF8, 0, base, -1, tmp, sizeof tmp, NULL, NULL);
    CoTaskMemFree(base);
    snprintf(path, sizeof path, "%s\\WindowSweaters", tmp);
    CreateDirectoryA(path, NULL);
    snprintf(path, sizeof path, "%s\\WindowSweaters\\apps.conf", tmp);
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); return path; }
    f = fopen(path, "w");
    if (f) { fputs(k_default_conf, f); fclose(f); }
  }
  return path;
}

static char* trim(char* s) {
  while (*s == ' ' || *s == '\t') s++;
  char* e = s + strlen(s);
  while (e > s && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n'||e[-1]=='\r')) e--;
  *e = 0; return s;
}

int knit_apps_load(void) {
  g_app_rule_count = 0;
  FILE* f = fopen(knit_apps_path(), "r");
  if (!f) return 0;
  char line[512];
  while (fgets(line, sizeof line, f) && g_app_rule_count < KNIT_APP_RULES_MAX) {
    if (!strchr(line, '\n') && !feof(f)) { int ch; while ((ch=fgetc(f))!='\n' && ch!=EOF) {} continue; }
    char* s = trim(line);
    if (!*s || *s == '#') continue;
    char* eq = strchr(s, '=');
    if (!eq) continue;
    *eq = 0;
    char* name = trim(s); char* val = trim(eq + 1);
    if (!*name || strlen(name) >= sizeof g_app_rules[0].match || *val != '#') continue;
    size_t len = strlen(val);
    if (len < 7 || (val[7] && !isspace((unsigned char)val[7]))) continue;
    int valid = 1;
    for (int i = 1; i <= 6; i++) if (!isxdigit((unsigned char)val[i])) valid = 0;
    if (!valid) continue;
    unsigned long rgb = strtoul(val + 1, NULL, 16);
    char chart[64] = {0};
    char* tail = trim(val + 7);
    if (*tail && *tail != '#') {
      size_t n = strcspn(tail, " \t\r\n");
      if (n >= sizeof chart) continue;
      memcpy(chart, tail, n);
      tail = trim(tail + n);
      if (*tail && *tail != '#') continue;
    }
    struct app_rule* r = &g_app_rules[g_app_rule_count++];
    snprintf(r->match, sizeof r->match, "%s", name);
    r->color = 0xff000000u | (uint32_t)rgb;
    snprintf(r->chart, sizeof r->chart, "%s", chart);
  }
  fclose(f);
  return g_app_rule_count;
}

// strncasecmp replacement for MSVC
static int prefix_ncasecmp(const char* a, const char* b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    unsigned char ca = (unsigned char)a[i], cb = (unsigned char)b[i];
    if (ca >= 'A' && ca <= 'Z') ca += 'a'-'A';
    if (cb >= 'A' && cb <= 'Z') cb += 'a'-'A';
    if (ca != cb) return (int)ca - (int)cb;
    if (!ca) return 0;
  }
  return 0;
}

const struct app_rule* knit_app_rule(const char* app_name) {
  if (!app_name || !*app_name) return NULL;
  const struct app_rule* best = NULL; size_t best_len = 0;
  for (int i = 0; i < g_app_rule_count; i++) {
    size_t n = strlen(g_app_rules[i].match);
    if (n && prefix_ncasecmp(app_name, g_app_rules[i].match, n) == 0 && n > best_len) { best = &g_app_rules[i]; best_len = n; }
  }
  if (best) return best;
  for (size_t i = 0; i < sizeof(k_collection)/sizeof(k_collection[0]); i++) {
    size_t n = strlen(k_collection[i].match);
    if (n && prefix_ncasecmp(app_name, k_collection[i].match, n) == 0 && n > best_len) { best = &k_collection[i]; best_len = n; }
  }
  return best;
}
bool knit_app_rule_personal(const struct app_rule* r) {
  for (int i = 0; i < g_app_rule_count; i++) if (r == &g_app_rules[i]) return true;
  return false;
}
int knit_pattern_for_app(const char* app_name) {
  if (!g_knit_pattern_by_app) return g_chart_active >= 0 && g_chart_active < g_chart_count ? g_chart_active : -1;
  const struct app_rule* r = knit_app_rule(app_name);
  return r && r->chart[0] ? knit_chart_index(r->chart) : -1;
}
bool knit_pattern_select(const char* name) {
  if (!name) return false;
  if (strcmp(name, "by-app") == 0) { g_knit_pattern_by_app = true; return true; }
  int idx = strcmp(name, "none") == 0 ? -1 : knit_chart_index(name);
  if (idx < 0 && strcmp(name, "none") != 0) return false;
  g_chart_active = idx; g_knit_pattern_by_app = false; return true;
}
bool knit_app_name_from_executable(const char* path, char* out, size_t cap) {
  if (!path || !out || !cap) return false;
  const char* base = strrchr(path, '\\');
  base = base ? base + 1 : path;
  const char* slash = strrchr(base, '/');
  if (slash) base = slash + 1;
  size_t n = strlen(base);
  if (n > 4 && _stricmp(base + n - 4, ".exe") == 0) n -= 4;
  if (!n || n >= cap) return false;
  memcpy(out, base, n); out[n] = 0;
  return true;
}

// ---- visibility gate (hidden.m model, exe-basename keys) ----
static bool g_on_by_default = true;
static char g_exc[256][64]; static int g_exc_n = 0;
static int pid_app_name(int pid, char* out, size_t cap) {
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
  if (!h) return 0;
  char path[MAX_PATH] = {0}; DWORD n = sizeof path;
  int ok = QueryFullProcessImageNameA(h, 0, path, &n)
    && knit_app_name_from_executable(path, out, cap);
  CloseHandle(h); return ok;
}
bool knit_apps_on_by_default(void) { return g_on_by_default; }
bool knit_apps_set_all(bool on) {
  if (g_on_by_default == on && g_exc_n == 0) return false;
  g_on_by_default = on; g_exc_n = 0; return true;
}
bool knit_app_hidden(const char* app) {
  if (!app || !*app) return !g_on_by_default;
  for (int i = 0; i < g_exc_n; i++) if (_stricmp(g_exc[i], app) == 0) return g_on_by_default;
  return !g_on_by_default;
}
bool knit_app_set_hidden(const char* app, bool hidden) {
  if (!app || !*app) return false;
  bool want_exc = (hidden != !g_on_by_default);
  for (int i = 0; i < g_exc_n; i++) if (_stricmp(g_exc[i], app) == 0) {
    if (!want_exc) { memmove(g_exc[i], g_exc[i+1], sizeof(g_exc[0])*(g_exc_n-i-1)); g_exc_n--; return true; }
    return false;
  }
  if (want_exc && g_exc_n < 256) { snprintf(g_exc[g_exc_n++], 64, "%s", app); return true; }
  return false;
}
int knit_app_exception_count(void) { return g_exc_n; }
const char* knit_app_exception(int i) { return (i>=0&&i<g_exc_n)?g_exc[i]:NULL; }
bool knit_pid_hidden(int pid) {
  char app[64] = {0};
  if (!pid_app_name(pid, app, sizeof app)) return !g_on_by_default;
  return knit_app_hidden(app);
}
