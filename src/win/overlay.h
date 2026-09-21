// overlay.h — one layered topmost HWND per tracked window.
#pragma once
#include <windows.h>
#include "../core/config.h"
struct sweater {
  HWND target, overlay;
  DWORD pid; char app[64];
  RECT target_rect; RECT overlay_rect;
  int focused, visible, topmost;
  ULONGLONG last_paint_ms;
  float radius;
  uint32_t color; int chart;
  uint32_t* bits; int bw, bh; // DIB bits (premultiplied)
  HBITMAP hbm; HDC hdc;
  int needs_paint;
};
void overlay_register(void);
struct sweater* sweater_create(HWND target, const char* app, DWORD pid);
void sweater_destroy(struct sweater* s);
// Returns 1 if overlay visible after sync.
int sweater_sync(struct sweater* s, struct settings* st, int focused);
void sweater_hide(struct sweater* s);
// Pin overlay directly below its target (call top-to-bottom for interleave).
void sweater_place_below(struct sweater* s);
