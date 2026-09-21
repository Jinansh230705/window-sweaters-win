// parse.c — CLI/config key=value parser, ported from macOS parse.c.
#include "config.h"
#include "knit_core.h"
#include "charts.h"
#include "apps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool starts(char* s, char* p) {
  if (!s || !p) return false;
  size_t n = strlen(p);
  return strlen(s) >= n && strncmp(s, p, n) == 0;
}
static bool parse_list(struct table* list, char* tok) {
  size_t n = strlen(tok) + 1;
  char* copy = (char*)malloc(n); memcpy(copy, tok, n);
  char* ctx = NULL; char* name = strtok_s(copy, ",", &ctx);
  bool found = false;
  table_clear(list);
  while (name) { if (*name) { _table_add(list, name, (int)strlen(name)+1, (void*)1); found = true; } name = strtok_s(NULL, ",", &ctx); }
  free(copy); return found;
}

uint32_t parse_settings(struct settings* s, int count, char** args) {
  uint32_t mask = 0; char order = 'a';
  for (int i = 0; i < count; i++) {
    if (starts(args[i], "blacklist=")) { s->blacklist_enabled = parse_list(&s->blacklist, args[i]+10); mask |= BORDER_UPDATE_MASK_RECREATE_ALL; }
    else if (starts(args[i], "whitelist=")) { s->whitelist_enabled = parse_list(&s->whitelist, args[i]+10); mask |= BORDER_UPDATE_MASK_RECREATE_ALL; }
    else if (sscanf_s(args[i], "width=%f", &s->border_width) == 1) mask |= BORDER_UPDATE_MASK_ALL;
    else if (sscanf_s(args[i], "order=%c", &order, 1) == 1) { s->border_order = (order=='a')?BORDER_ORDER_ABOVE:BORDER_ORDER_BELOW; mask |= BORDER_UPDATE_MASK_ALL; }
    else if (sscanf_s(args[i], "style=%c", &s->border_style, 1) == 1) mask |= BORDER_UPDATE_MASK_ALL;
    else if (strcmp(args[i], "hidpi=on") == 0) { s->hidpi = true; mask |= BORDER_UPDATE_MASK_RECREATE_ALL; }
    else if (strcmp(args[i], "hidpi=off") == 0) { s->hidpi = false; mask |= BORDER_UPDATE_MASK_RECREATE_ALL; }
    else if (strncmp(args[i], "chart=", 6) == 0) {
      if (knit_pattern_select(args[i]+6)) { knit_flush_cache(); mask |= BORDER_UPDATE_MASK_ALL; }
      else printf("[?] Window Sweaters: Unknown pattern '%s'\n", args[i]+6);
    }
    else if (strcmp(args[i], "apps=reload") == 0) { knit_apps_load(); knit_flush_cache(); mask |= BORDER_UPDATE_MASK_ALL; }
    else if (strcmp(args[i], "charts=reload") == 0) { knit_charts_load(knit_charts_dir()); knit_flush_cache(); mask |= BORDER_UPDATE_MASK_ALL; }
    else if (strcmp(args[i], "anchor=corner") == 0) { g_knit_anchor = KNIT_ANCHOR_CORNER; mask |= BORDER_UPDATE_MASK_ALL; }
    else if (strcmp(args[i], "anchor=centre") == 0 || strcmp(args[i], "anchor=center") == 0) { g_knit_anchor = KNIT_ANCHOR_CENTRE; mask |= BORDER_UPDATE_MASK_ALL; }
    else if (sscanf_s(args[i], "dim=%f", &g_knit_dim) == 1) mask |= BORDER_UPDATE_MASK_ALL;
    else if (sscanf_s(args[i], "ground=%f", &g_knit.ground) == 1) { knit_flush_cache(); mask |= BORDER_UPDATE_MASK_ALL; }
    else if (sscanf_s(args[i], "ambient=%f", &g_knit.ambient) == 1) { knit_flush_cache(); mask |= BORDER_UPDATE_MASK_ALL; }
    else if (sscanf_s(args[i], "relief=%f", &g_knit.relief) == 1) { knit_flush_cache(); mask |= BORDER_UPDATE_MASK_ALL; }
    else if (sscanf_s(args[i], "sheen=%f", &g_knit.sheen) == 1) { knit_flush_cache(); mask |= BORDER_UPDATE_MASK_ALL; }
    else if (sscanf_s(args[i], "tuck=%f", &g_knit.tuck) == 1) mask |= BORDER_UPDATE_MASK_ALL;
    else if (sscanf_s(args[i], "gauge=%f", &g_knit.rows) == 1) { knit_flush_cache(); mask |= BORDER_UPDATE_MASK_ALL; }
    else if (strcmp(args[i], "knit=on") == 0) { g_knit_on = true; mask |= BORDER_UPDATE_MASK_ALL; }
    else if (strcmp(args[i], "knit=off") == 0) { g_knit_on = false; mask |= BORDER_UPDATE_MASK_ALL; }
    else if (strncmp(args[i], "yarn=", 5) == 0) {
      for (int k = 0; k < KNIT_STITCH_COUNT; k++) if (strcmp(args[i]+5, g_knit_stitch_names[k]) == 0) g_knit_stitch = k;
      knit_flush_cache(); mask |= BORDER_UPDATE_MASK_ALL;
    }
    else if (strncmp(args[i], "basket=", 7) == 0) {
      for (int k = 0; k < g_knit_basket_count; k++) if (strcmp(args[i]+7, g_knit_baskets[k].name) == 0) g_knit_basket = k;
      knit_flush_cache(); mask |= BORDER_UPDATE_MASK_ALL;
    }
    else if (strncmp(args[i], "--", 2) == 0) { }
    else printf("[?] Window Sweaters: Invalid argument '%s'\n", args[i]);
  }
  return mask;
}
