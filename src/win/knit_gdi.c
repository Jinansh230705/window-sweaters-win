// knit_gdi.c — ring painter: rounded-rect ring mask + tiled knit sampling.
// Ports the geometry of macOS knit_draw (ring clip, mitred sides, anchor,
// fitted_repeat, solid corner patches, opaque dim) onto a GDI DIB.
#include "knit_gdi.h"
#include "../core/knit_core.h"
#include "../core/charts.h"
#include <math.h>
#include <string.h>

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

// Signed distance: negative inside rounded rect.
static float sd_round_rect(float px, float py, float cx, float cy, float hw, float hh, float r) {
  float qx = fabsf(px - cx) - (hw - r), qy = fabsf(py - cy) - (hh - r);
  float ax = qx > 0 ? qx : 0, ay = qy > 0 ? qy : 0;
  return sqrtf(ax * ax + ay * ay) + fminf(fmaxf(qx, qy), 0.f) - r;
}

struct tile_view { uint32_t* px; int tw, th, pw, ph; int ok; };
static struct tile_view view_for(float band, uint32_t color, int chart) {
  struct tile_view v = {0};
  if (chart < -2 || chart >= g_chart_count) { if (chart != -1) return v; }
  struct knit_tile t = knit_get_tile(band, color, chart);
  if (!t.px || t.pw <= 0 || t.ph <= 0 || t.w <= 0 || t.h <= 0) return v;
  v.px = t.px; v.tw = t.w; v.th = t.h; v.pw = t.pw; v.ph = t.ph; v.ok = 1;
  return v;
}
// Nearest sample of tile at logical (lx in [0,tw), ly in [0,th)).
static uint32_t sample_tile(struct tile_view* v, float lx, float ly) {
  float fx = lx - floorf(lx / v->tw) * v->tw;
  float fy = ly - floorf(ly / v->th) * v->th;
  int sx = (int)(fx / v->tw * v->pw), sy = (int)(fy / v->th * v->ph);
  if (sx < 0) sx = 0; if (sx >= v->pw) sx = v->pw - 1;
  if (sy < 0) sy = 0; if (sy >= v->ph) sy = v->ph - 1;
  // tile buffers are stored flipped vertically (fabric path); plain path is not.
  // Both read correctly via direct index since each generator is self-consistent
  // with its own orientation; no extra flip here.
  return v->px[(size_t)sy * v->pw + sx];
}

void knit_paint_ring(uint32_t* bits, int fw, int fh, RECT win, float radius,
    float band, uint32_t color, int chart, float dim, float tuck, int anchor,
    char style) {
  if (!bits || fw <= 0 || fh <= 0) return;
  if (band < 2.f) band = 2.f;
  if (tuck < 1.f) tuck = 1.f;
  float ww = (float)(win.right - win.left), wh = (float)(win.bottom - win.top);
  radius = clampf(radius, 0.f, fminf(ww, wh) * 0.5f);
  float half = fminf(ww, wh) * 0.5f - 2.f;
  if (tuck > half) tuck = half > 1.f ? half : 1.f;

  float ol = win.left - band, ot = win.top - band;
  float or_ = win.right + band, ob = win.bottom + band;
  float il = win.left + tuck, it = win.top + tuck;
  float ir = win.right - tuck, ib = win.bottom - tuck;
  float orad = radius + band;
  float irad = radius > tuck ? radius - tuck : 0.f;
  float ocx = (ol + or_) / 2, ocy = (ot + ob) / 2, ohw = (or_ - ol) / 2, ohh = (ob - ot) / 2;
  float icx = (il + ir) / 2, icy = (it + ib) / 2, ihw = (ir - il) / 2, ihh = (ib - it) / 2;

  int valid_chart = (chart >= 0 && chart < g_chart_count);
  int knitted = (style == 'k');
  struct tile_view main = knitted ? view_for(band, color, valid_chart ? chart : -1)
                                  : (struct tile_view){0};
  if (knitted && !main.ok) { memset(bits, 0, (size_t)fw * fh * 4); return; }
  uint32_t corner_col = color;
  int solid = valid_chart && g_charts[chart].solid_corners;
  if (solid && g_charts[chart].corner_color) corner_col = g_charts[chart].corner_color;
  struct tile_view patch = (knitted && solid) ? view_for(band, corner_col, KNIT_PATCH_CHART) : main;
  if (!patch.ok) patch = main;
  // Outer cuff (atelier-notes/calendar): the outer third of the band wears a
  // solid cuff tile, like the Mac's curved cuff ring.
  uint32_t cuff_col = (knitted && valid_chart) ? g_charts[chart].cuff_color : 0;
  struct tile_view cuff = cuff_col ? view_for(band, cuff_col, KNIT_PATCH_CHART) : main;
  int has_cuff = (cuff_col && cuff.ok) ? 1 : 0;
  if (!cuff.ok) cuff = main;
  float cuff_w = band / 3.f;
  int fitted = valid_chart && g_charts[chart].fitted_repeat;
  float repeat_w = (float)main.tw;
  float ow = or_ - ol;
  if (fitted && ow > 0) { float reps = floorf(ow / main.tw + 0.5f); if (reps < 1) reps = 1; repeat_w = ow / reps; }
  float ax_off = 0.f; // anchor=corner: steady under resize
  float band_phase = band * 0.5f - main.th * 0.5f;

  float dimk = 1.f - clampf(dim, 0.f, 1.f);
  uint32_t cb = corner_col;
  uint8_t cbr = (cb >> 16) & 255, cbg = (cb >> 8) & 255, cbb = cb & 255;

  for (int y = 0; y < fh; y++) {
    for (int x = 0; x < fw; x++) {
      float px = x + 0.5f, py = y + 0.5f;
      // Fast paths (pixel-identical, no sqrt): outside the outer rect is never
      // in the ring; strictly inside the inner rect eroded by the corner
      // radius never escapes the hole. Only the edge band needs the SDF.
      int has_hole = (ir > il && ib > it);
      if (px <= ol || px >= or_ || py <= ot || py >= ob) { bits[y * fw + x] = 0; continue; }
      if (has_hole && px > il + irad && px < ir - irad && py > it + irad && py < ib - irad) {
        bits[y * fw + x] = 0; continue;
      }
      float d_out = sd_round_rect(px, py, ocx, ocy, ohw, ohh, orad);
      float in_ring = d_out;
      float in_hole = has_hole ? sd_round_rect(px, py, icx, icy, ihw, ihh, irad) : 1.f;
      if (!(in_ring <= 0.f && in_hole > 0.f)) { bits[y * fw + x] = 0; continue; }
      uint8_t sr, sg, sb;
      if (!knitted) {
        // Solid yarn style: flat colour, darkened (not faded) when dimmed.
        sr = (color >> 16) & 255; sg = (color >> 8) & 255; sb = color & 255;
      } else {
      // side select by nearest outer edge (mitre approx: diagonal split)
      float dl = px - ol, dt = py - ot, dr = or_ - px, db = ob - py;
      float m = fminf(fminf(dl, dt), fminf(dr, db));
      float along, across;
      if (m == dt) { along = px - ol; across = py - ot; }
      else if (m == db) { along = px - ol; across = ob - py; }
      else if (m == dl) { along = py - ot; across = px - ol; }
      else { along = py - ot; across = or_ - px; }
      // corner patch zone
      float cap = fminf(orad, ow * 0.5f);
      int in_cap = (along < cap || along > ((m == dt || m == db) ? ow : (ob - ot)) - cap);
      struct tile_view* vv = (solid && in_cap) ? &patch : &main;
      if (!(solid && in_cap) && has_cuff && across < cuff_w) vv = &cuff;
      float lx, ly;
      if (solid && !in_cap && !fitted) { lx = along - cap; ly = across + band_phase; }
      else if (fitted && !in_cap && vv == &main) {
        float edge = (m == dt || m == db) ? ow : (ob - ot);
        float reps = floorf(edge / vv->tw + 0.5f); if (reps < 1) reps = 1;
        float rw = edge / reps;
        float cell = along / rw; float fr = cell - floorf(cell);
        lx = fr * vv->tw; ly = across + band_phase;
      } else {
        float edge = (m == dt || m == db) ? ow : (ob - ot);
        float ox = (anchor == 1) ? edge * 0.5f - vv->tw * 0.5f : ax_off;
        lx = along - ox; ly = across + band_phase;
      }
      (void)repeat_w;
      uint32_t s = sample_tile(vv, lx, ly);
      // opaque backing (covers sampling slivers) then dim (darken, stay opaque)
      sr = (s >> 16) & 255; sg = (s >> 8) & 255; sb = s & 255;
      if (in_cap && solid) { sr = (uint8_t)((sr + cbr) / 2); sg = (uint8_t)((sg + cbg) / 2); sb = (uint8_t)((sb + cbb) / 2); }
      }
      sr = (uint8_t)(sr * dimk); sg = (uint8_t)(sg * dimk); sb = (uint8_t)(sb * dimk);
      // premultiplied DIB (alpha 255): store as 0xFFrrggbb, drawn with AC_SRC_ALPHA
      bits[y * fw + x] = 0xff000000u | (sr << 16) | (sg << 8) | sb;
    }
  }
  knit_tile_release(&main);
}
