// overlay.c — layered overlay windows (replaces macOS border.c SkyLight path).
#include "overlay.h"
#include "knit_gdi.h"
#include "../core/knit_core.h"
#include "../core/charts.h"
#include "../core/apps.h"
#include <dwmapi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static const wchar_t* kClass = L"WindowSweatersOverlay";
extern uint32_t autoyarn_color(const char* app, DWORD pid, int* chart);

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }
void overlay_register(void) {
  static int done = 0; if (done) return; done = 1;
  WNDCLASSW c = {0};
  c.lpfnWndProc = proc; c.hInstance = GetModuleHandleW(NULL); c.lpszClassName = kClass;
  c.hCursor = LoadCursorW(NULL, IDC_ARROW);
  RegisterClassW(&c);
}

static int target_frame(HWND t, RECT* r) {
  RECT f = {0};
  if (SUCCEEDED(DwmGetWindowAttribute(t, DWMWA_EXTENDED_FRAME_BOUNDS, &f, sizeof f)) && f.right > f.left && f.bottom > f.top) { *r = f; return 1; }
  if (GetWindowRect(t, &f) && f.right > f.left && f.bottom > f.top) { *r = f; return 1; }
  return 0;
}

struct sweater* sweater_create(HWND target, const char* app, DWORD pid) {
  struct sweater* s = (struct sweater*)calloc(1, sizeof *s);
  if (!s) return NULL;
  s->target = target; s->pid = pid;
  snprintf(s->app, sizeof s->app, "%s", app ? app : "");
  s->overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
    kClass, L"", WS_POPUP, 0, 0, 8, 8, NULL, NULL, GetModuleHandleW(NULL), NULL);
  if (!s->overlay) { free(s); return NULL; }
  s->hdc = CreateCompatibleDC(NULL);
  s->radius = 9.f; s->needs_paint = 1;
  return s;
}
void sweater_destroy(struct sweater* s) {
  if (!s) return;
  if (s->overlay) DestroyWindow(s->overlay);
  if (s->hbm) DeleteObject(s->hbm);
  if (s->hdc) DeleteDC(s->hdc);
  free(s);
}
void sweater_hide(struct sweater* s) {
  if (!s || !s->visible) return;
  s->visible = 0;
  ShowWindow(s->overlay, SW_HIDE);
}

static void ensure_dib(struct sweater* s, int w, int h) {
  if (s->hbm && s->bw == w && s->bh == h) return;
  if (s->hbm) { DeleteObject(s->hbm); s->hbm = NULL; s->bits = NULL; }
  BITMAPINFO bi = {0};
  bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  s->hbm = CreateDIBSection(s->hdc, &bi, DIB_RGB_COLORS, (void**)&s->bits, NULL, 0);
  s->bw = w; s->bh = h;
  SelectObject(s->hdc, s->hbm);
}

// Pin the overlay directly BELOW its target in z-order (macOS default is
// BORDER_ORDER_BELOW too). A background window's ring must never cover a
// foreground window. Topmost targets get a topmost overlay so the ring can
// still sit next to them; everything else stays in the normal group so
// fullscreen apps and the foreground window cover stale rings, not vice versa.
void sweater_place_below(struct sweater* s) {
  if (!s || !s->visible) return;
  if (!IsWindow(s->target) || !IsWindow(s->overlay)) return;
  LONG ex = GetWindowLongA(s->target, GWL_EXSTYLE);
  int want_top = (ex & WS_EX_TOPMOST) != 0;
  if (want_top != s->topmost) {
    SetWindowPos(s->overlay, want_top ? HWND_TOPMOST : HWND_NOTOPMOST,
      0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    s->topmost = want_top;
  }
  SetWindowPos(s->overlay, s->target, 0, 0, 0, 0,
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

int sweater_sync(struct sweater* s, struct settings* st, int focused) {
  // Fail closed: a dead target must never leave a stranded ring behind.
  if (!IsWindow(s->target)) { sweater_hide(s); return 0; }
  // Hidden states first: never show or move the overlay for these.
  if (!st->enabled || !g_knit_on) { sweater_hide(s); return 0; }
  if (!IsWindowVisible(s->target) || IsIconic(s->target)) { sweater_hide(s); return 0; }
  // cloaked (virtual desktop / minimized owner). A failed query means DWM
  // has no representation of this window — also hide, never guess.
  int cloaked = 0;
  if (FAILED(DwmGetWindowAttribute(s->target, DWMWA_CLOAKED, &cloaked, sizeof cloaked))) {
    sweater_hide(s); return 0;
  }
  if (cloaked) { sweater_hide(s); return 0; }
  RECT fr;
  if (!target_frame(s->target, &fr)) { sweater_hide(s); return 0; }
  int tw = fr.right - fr.left, th = fr.bottom - fr.top;
  if (tw < 24 || th < 24) { sweater_hide(s); return 0; }

  float band = st->border_width;
  if (band < 4.f) band = 4.f;
  if (band > 40.f) band = 40.f;
  int pad = (int)BORDER_PADDING;
  RECT ov = { fr.left - (int)band - pad, fr.top - (int)band - pad,
              fr.right + (int)band + pad, fr.bottom + (int)band + pad };
  int ow = ov.right - ov.left, oh = ov.bottom - ov.top;
  if (ow <= 0 || oh <= 0 || ow > 10000 || oh > 10000) { sweater_hide(s); return 0; }

  // resolve yarn + chart
  uint32_t color; int chart;
  const struct app_rule* rule = knit_app_rule(s->app);
  if (!g_knit_pattern_by_app) {
    chart = g_chart_active;
    color = knit_color_for_window((uint32_t)(uintptr_t)s->target);
  } else if (rule) {
    color = rule->color; chart = knit_pattern_for_app(s->app);
  } else {
    int ac = -1;
    uint32_t ay = autoyarn_color(s->app, s->pid, &ac);
    color = ay ? ay : knit_color_for_app(s->app);
    chart = knit_pattern_for_app(s->app);
    if (chart < 0 && ac >= 0) chart = ac; // generated icon chart if any
  }
  float dim = (!focused) ? g_knit_dim : 0.f;

  int pos_changed = memcmp(&s->target_rect, &fr, sizeof fr) != 0;
  int size_changed = (s->bw != ow || s->bh != oh);
  int look_changed = (s->color != color || s->chart != chart || s->focused != focused
    || s->needs_paint);

  s->target_rect = fr; s->overlay_rect = ov;
  s->color = color; s->chart = chart; s->focused = focused;

  LONG tex = GetWindowLongA(s->target, GWL_EXSTYLE);
  s->topmost = (tex & WS_EX_TOPMOST) != 0;

  if (!s->visible) {
    // First show (or re-show): place in the right group, paint forced below.
    SetWindowPos(s->overlay, s->topmost ? HWND_TOPMOST : HWND_TOP,
      ov.left, ov.top, ow, oh, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    s->visible = 1;
    size_changed = 1; look_changed = 1;
    sweater_place_below(s); // drop directly under the target right away
  } else if (size_changed) {
    SetWindowPos(s->overlay, NULL, ov.left, ov.top, ow, oh,
      SWP_NOACTIVATE | SWP_NOZORDER);
  } else if (pos_changed) {
    // Pure move: reposition ONLY, no repaint, no z-order change, async so a
    // hung target never stalls our loop. This is the drag hot path.
    SetWindowPos(s->overlay, NULL, ov.left, ov.top, 0, 0,
      SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE | SWP_ASYNCWINDOWPOS);
  }

  // Resize storm throttle: during an active resize, repaint at most ~20fps and
  // keep positions fresh every pump. Focus/look-only changes paint at once.
  ULONGLONG now = GetTickCount64();
  int want_paint = (size_changed || look_changed);
  if (want_paint && size_changed && s->last_paint_ms
      && now - s->last_paint_ms < 50) {
    want_paint = 0;
    s->needs_paint = 1; // repaint on settle via the next event/reconcile
  }

  if (want_paint) {
    ensure_dib(s, ow, oh);
    if (!s->bits) return 1;
    RECT win = { (int)band + pad, (int)band + pad, (int)band + pad + tw, (int)band + pad + th };
    knit_paint_ring(s->bits, ow, oh, win, 9.f, band, color, chart, dim, g_knit.tuck, g_knit_anchor, st->border_style);
    POINT src = {0, 0}; SIZE sz = {ow, oh}; POINT dst = {ov.left, ov.top};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UPDATELAYEREDWINDOWINFO info = {0};
    info.cbSize = sizeof info; info.hdcDst = NULL; info.pptDst = &dst;
    info.psize = &sz; info.hdcSrc = s->hdc; info.pptSrc = &src;
    info.crKey = 0; info.pblend = &bf; info.dwFlags = ULW_ALPHA; info.prcDirty = NULL;
    UpdateLayeredWindowIndirect(s->overlay, &info);
    s->needs_paint = 0;
    s->last_paint_ms = GetTickCount64();
  }
  return 1;
}
