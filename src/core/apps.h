// Per-app colourways — logic ported from macOS src/apps.c; paths + exe helper
// adapted for Windows (%APPDATA%, *.exe basename).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define KNIT_APP_RULES_MAX 64
struct app_rule { char match[64]; uint32_t color; char chart[64]; };
extern struct app_rule g_app_rules[KNIT_APP_RULES_MAX];
extern int g_app_rule_count;
extern bool g_knit_pattern_by_app;
const char* knit_apps_path(void);
int knit_apps_load(void);
const struct app_rule* knit_app_rule(const char* app_name);
bool knit_app_rule_personal(const struct app_rule* rule);
int knit_pattern_for_app(const char* app_name);
bool knit_pattern_select(const char* name);
// Windows: "C:\...\ChatGPT.exe" -> "ChatGPT"; also handles bare exe names.
bool knit_app_name_from_executable(const char* path, char* output, size_t capacity);
// Visibility gate (ports hidden.m model; keyed by exe basename, insertion-ordered).
bool knit_apps_on_by_default(void);
bool knit_apps_set_all(bool on);
bool knit_app_hidden(const char* app);
bool knit_app_set_hidden(const char* app, bool hidden);
int knit_app_exception_count(void);
const char* knit_app_exception(int index);
bool knit_pid_hidden(int pid);
