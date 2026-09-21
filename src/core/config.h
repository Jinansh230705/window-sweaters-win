// Shared settings (ports macOS border.h settings without CG/AppKit).
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "table.h"
#define BORDER_ORDER_ABOVE 1
#define BORDER_ORDER_BELOW -1
#define BORDER_STYLE_ROUND 'r'
#define BORDER_STYLE_SQUARE 's'
#define BORDER_STYLE_KNIT 'k'
#define BORDER_PADDING 8.0f
struct settings {
  bool enabled;
  float border_width; // knit band width in px
  char border_style;
  bool hidpi;
  int border_order;
  bool blacklist_enabled; struct table blacklist;
  bool whitelist_enabled; struct table whitelist;
};
#define BORDER_UPDATE_MASK_ACTIVE   (1 << 0)
#define BORDER_UPDATE_MASK_INACTIVE (1 << 1)
#define BORDER_UPDATE_MASK_ALL      (BORDER_UPDATE_MASK_ACTIVE | BORDER_UPDATE_MASK_INACTIVE)
#define BORDER_UPDATE_MASK_RECREATE_ALL (1 << 2)
#define BORDER_UPDATE_MASK_SETTING  (1 << 3)
uint32_t parse_settings(struct settings* s, int count, char** args);
