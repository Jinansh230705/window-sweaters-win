#pragma once
#include <stdint.h>
#include <stdbool.h>

#define KNIT_CHART_MAX 128
struct knit_chart {
  char name[64];
  int w, h;
  uint32_t* px; // ARGB, row 0 = top as authored; alpha<128 = base yarn
  bool solid_corners; uint32_t corner_color; uint32_t cuff_color;
  bool round_dots, fitted_repeat, sculpted_yarn, defined_yarn, generated, custom;
};
extern struct knit_chart g_charts[KNIT_CHART_MAX];
extern int g_chart_count;
extern unsigned g_charts_generation;
extern int g_chart_active; // -1 = plain

const char* knit_charts_dir(void); // %APPDATA%\WindowSweaters\charts
int knit_charts_load(const char* dir);
int knit_chart_index(const char* name);
