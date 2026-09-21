// knit_core.c — portable port of macOS src/knit.c (math + CPU shading).
// CGPath/CGBitmapContext/CGImage replaced with raw ARGB buffers.
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include "knit_core.h"
#include "charts.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct knit_gauge g_knit = {
  6.f, 1.35f, 0.12f, 0.48f, 0.28f, 0.035f,
  0.98f, 0.74f, 1.18f, 0.94f, 1.0f, 0.70f, 0.10f, 14.f
};
int g_knit_stitch = KNIT_STOCKINETTE;
int g_knit_basket = 3;
int g_knit_anchor = KNIT_ANCHOR_CORNER;
float g_knit_dim = 0.f;
bool g_knit_on = true;
const char* g_knit_stitch_names[] = { "stockinette", "rib", "garter" };

static const uint32_t basket_wool[] = {
  0xffd1495b, 0xff4e8098, 0xffedae49, 0xff7c9885, 0xff9b6a8f,
  0xffe8846b, 0xff3f7d6e, 0xffc46a4e, 0xff6d7ba8, 0xffb8935f };
static const uint32_t basket_sorbet[] = {
  0xfff08ca4, 0xff7fc6c4, 0xffffc978, 0xffb08ed4, 0xff8fce90,
  0xffff9f7a, 0xff8ab6f0, 0xffe5a3d0 };
static const uint32_t basket_forest[] = {
  0xff4a6b52, 0xff7d8f5c, 0xff3f6b6e, 0xff8a7248, 0xff5c6b8a,
  0xff6b5344, 0xff2f5e4a };
static const uint32_t basket_mono[] = {
  0xff9aa0a6, 0xff7c8288, 0xffb4bac0, 0xff686e74, 0xff8d939a };
static const uint32_t basket_dopamine[] = {
  0xffff2d95, 0xff00d9ff, 0xffffd400, 0xff7c3aff, 0xff00e676,
  0xffff6b00, 0xffff1744, 0xff00b8d4 };
static const uint32_t basket_neon[] = {
  0xfff50057, 0xff00e5ff, 0xffc6ff00, 0xff651fff, 0xff1de9b6, 0xffff9100 };
static const uint32_t basket_punch[] = {
  0xffe8175d, 0xff0fb9b1, 0xfffec230, 0xff5f27cd, 0xff10ac84, 0xffee5a24 };

const struct knit_basket g_knit_baskets[] = {
  { "dopamine", basket_dopamine, sizeof(basket_dopamine)/4 },
  { "neon",     basket_neon,     sizeof(basket_neon)/4 },
  { "punch",    basket_punch,    sizeof(basket_punch)/4 },
  { "wool",     basket_wool,     sizeof(basket_wool)/4 },
  { "sorbet",   basket_sorbet,   sizeof(basket_sorbet)/4 },
  { "forest",   basket_forest,   sizeof(basket_forest)/4 },
  { "mono",     basket_mono,     sizeof(basket_mono)/4 },
};
const int g_knit_basket_count = sizeof(g_knit_baskets)/sizeof(g_knit_baskets[0]);

uint32_t knit_mix(uint32_t h) {
  h ^= h >> 16; h *= 0x85ebca6b; h ^= h >> 13; h *= 0xc2b2ae35; h ^= h >> 16;
  return h;
}
uint32_t knit_color_for_window(uint32_t wid) {
  const struct knit_basket* b = &g_knit_baskets[g_knit_basket];
  return b->colors[knit_mix(wid) % (uint32_t)b->len];
}
uint32_t knit_color_for_app(const char* app) {
  uint32_t h = 2166136261u;
  for (const unsigned char* p = (const unsigned char*)(app ? app : ""); *p; p++) {
    unsigned char c = *p;
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    h = (h ^ c) * 16777619u;
  }
  return knit_color_for_window(h);
}
uint32_t knit_zigzag_contrast(uint32_t base) {
  double r = ((base >> 16) & 255) / 255.0, g = ((base >> 8) & 255) / 255.0, b = (base & 255) / 255.0;
  double mx = fmax(r, fmax(g, b)), mn = fmin(r, fmin(g, b)), l = (mx + mn) / 2, h = 0, s = 0;
  if (l < 0.62) return KNIT_CREAM;
  if (mx != mn) {
    double d = mx - mn;
    s = l > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
    h = mx == r ? (g - b) / d + (g < b ? 6 : 0) : mx == g ? (b - r) / d + 2 : (r - g) / d + 4;
    h /= 6;
  }
  l -= 0.26;
  double q = l < 0.5 ? l * (1 + s) : l + s - l * s, p = 2 * l - q, t[3] = {h + 1.0/3, h, h - 1.0/3}, o[3];
  for (int i = 0; i < 3; i++) {
    double c = t[i]; if (c < 0) c += 1; if (c > 1) c -= 1;
    o[i] = c < 1.0/6 ? p + (q - p) * 6 * c : c < 0.5 ? q : c < 2.0/3 ? p + (q - p) * (2.0/3 - c) * 6 : p;
  }
  return 0xff000000u | ((uint32_t)(o[0]*255) << 16) | ((uint32_t)(o[1]*255) << 8) | (uint32_t)(o[2]*255);
}
float knit_noise01(int a, int b) {
  float s = sinf(a * 12.9898f + b * 78.233f) * 43758.5453f;
  return s - floorf(s);
}

static void knit_blur(float* a, int w, int h, float* tmp) {
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    int l = (x - 1 + w) % w, r = (x + 1) % w;
    tmp[y*w+x] = (a[y*w+l] + 2.f*a[y*w+x] + a[y*w+r]) * 0.25f;
  }
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    int u = (y - 1 + h) % h, d = (y + 1) % h;
    a[y*w+x] = (tmp[u*w+x] + 2.f*tmp[y*w+x] + tmp[d*w+x]) * 0.25f;
  }
}

// Colourwork fabric shader: direct port of knit_make_fabric (pure CPU).
static uint32_t* knit_make_fabric_px(float band, uint32_t color, int chart,
    float sw, float row_step, int tw, int th, int scale, int* outW, int* outH) {
  (void)band;
  const struct knit_chart* ch = &g_charts[chart];
  int W = tw * scale, H = th * scale;
  uint32_t* px = (uint32_t*)malloc((size_t)W * H * 4);
  if (!px) return NULL;
  float stitch_scale = ch->defined_yarn ? 2.f : 1.f;
  float relief = fminf(ch->defined_yarn ? 0.30f : 0.25f,
      fmaxf(0.f, g_knit.relief * (ch->defined_yarn ? 0.32f : 0.20f)));
  for (int y = 0; y < H; y++) {
    float sy = (y + 0.5f) / (scale * row_step);
    float sty = sy / stitch_scale;
    int row = (int)floorf(sty);
    float v = sty - row;
    for (int x = 0; x < W; x++) {
      float sx = (x + 0.5f) / (scale * sw);
      int column = (int)floorf(sx);
      float stx = sx / stitch_scale;
      int yc = (int)floorf(stx);
      float u = stx - yc;
      float warp = ch->round_dots ? 0.f : 0.16f * (1.f - 2.f * fabsf(2.f * u - 1.f));
      int cr = (int)floorf(sy + warp);
      cr = (cr % (ch->h * 2) + ch->h * 2) % (ch->h * 2);
      uint32_t yarn = ch->px[(size_t)(cr / 2) * ch->w + (column / 2) % ch->w];
      if (ch->round_dots && (yarn >> 24) >= 128) {
        int cx = (column / 2) % ch->w, cy = cr / 2;
        bool left = ch->px[(size_t)cy * ch->w + (cx + ch->w - 1) % ch->w] == yarn;
        bool above = ch->px[(size_t)((cy + ch->h - 1) % ch->h) * ch->w + cx] == yarn;
        float dx = sx * 0.5f - floorf(sx * 0.5f) + (left ? 1.f : 0.f) - 1.f;
        float dy = sy * 0.5f - floorf(sy * 0.5f) + (above ? 1.f : 0.f) - 1.f;
        if (dx*dx + dy*dy > 0.94f) yarn = color;
      }
      if ((yarn >> 24) < 128) yarn = color;
      uint32_t seed = knit_mix((uint32_t)yc * 73856093u ^ (uint32_t)row * 19349663u);
      float wobble = ((seed & 255u) / 255.f - 0.5f) * g_knit.jitter;
      float leg = 0.42f * (1.f - v) + 0.08f * v * (1.f - v);
      float dist = fabsf(fabsf(u - 0.5f - wobble) - leg);
      float radius = fmaxf(0.10f, g_knit.yarn * 0.5f);
      float ridge = fmaxf(0.f, 1.f - dist * dist / (radius * radius));
      uint32_t grain = knit_mix((uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u);
      float fibre = ((grain & 255u) / 255.f - 0.5f) * 0.014f;
      float shade = g_knit.ground - relief * (1.f - ridge)
                  + (g_knit.ambient - 0.94f) + g_knit.sheen * 0.08f * (0.5f - u) + fibre;
      shade = fmaxf(0.f, shade);
      uint32_t r = (uint32_t)fminf(255.f, ((yarn >> 16) & 255u) * shade);
      uint32_t g = (uint32_t)fminf(255.f, ((yarn >> 8) & 255u) * shade);
      uint32_t b = (uint32_t)fminf(255.f, (yarn & 255u) * shade);
      px[(size_t)(H - 1 - y) * W + x] = 0xff000000u | (r << 16) | (g << 8) | b;
    }
  }
  *outW = W; *outH = H;
  return px;
}

// Plain/sculpted tile without CG paths: analytic stockinette height field +
// same blur + directional light as macOS knit_make_tile. For sculpted charts
// each stitch takes its yarn from the chart cell (transparent = base yarn),
// mirroring macOS where each yarn gets its own stroked path.
static uint32_t* knit_make_plain_px(uint32_t base_color, int chart, float sw, float row_step,
    int tw, int th, int scale, bool sculpted, int* outW, int* outH) {
  int W = tw * scale, H = th * scale;
  const struct knit_chart* ch = (chart >= 0 && chart < g_chart_count) ? &g_charts[chart] : NULL;
  struct knit_gauge m = g_knit;
  if (sculpted) { m.row_overlap = 0.05f; m.yarn = 0.40f; m.ground = 0.94f;
    m.ambient = 0.84f; m.relief = 1.65f; m.sheen = 0.28f; }
  float* hf = (float*)malloc(sizeof(float) * W * H);
  float* tmp = (float*)malloc(sizeof(float) * W * H);
  uint32_t* cb = (uint32_t*)malloc((size_t)W * H * 4);
  if (!hf || !tmp || !cb) { free(hf); free(tmp); free(cb); return NULL; }
  float sh = row_step / (1.f - m.row_overlap);
  for (int y = 0; y < H; y++) {
    float sy = (y + 0.5f) / scale;
    int row = (int)floorf(sy / row_step);
    float v = sy / row_step - row;
    for (int x = 0; x < W; x++) {
      float sx = (x + 0.5f) / scale;
      int col = (int)floorf(sx / sw);
      float u = sx / sw - col;
      float wob = (knit_noise01((col % 8 + 8) % 8, (row % 8 + 8) % 8) - 0.5f) * m.jitter * 2.f;
      float cu = u - 0.5f - wob;
      float leg = 0.40f * (1.f - v) + 0.10f * v * (1.f - v);
      float d = fabsf(fabsf(cu) - leg * (0.55f + 0.45f * sinf(v * 3.14159f)));
      float radius = fmaxf(0.12f, m.yarn * 0.5f);
      float hgt = fmaxf(0.f, 1.f - d * d / (radius * radius));
      // garter/rib variants adjust the field cheaply
      if (g_knit_stitch == KNIT_RIB && !sculpted) hgt *= (col & 1) ? 0.55f : 1.f;
      if (g_knit_stitch == KNIT_GARTER && !sculpted)
        hgt = 0.5f + 0.5f * sinf(sy / row_step * 3.14159f);
      (void)sh;
      hf[y * W + x] = hgt;
      // Sculpted colourwork: one stitch per chart cell (density 1, same grid
      // macOS strokes per-yarn paths on). Transparent cell = base yarn.
      uint32_t yarn = base_color;
      if (ch) {
        int cx = ((col % ch->w) + ch->w) % ch->w;
        int cy = ((row % ch->h) + ch->h) % ch->h;
        uint32_t cell = ch->px[(size_t)cy * ch->w + cx];
        yarn = ((cell >> 24) < 128) ? base_color : (cell | 0xff000000u);
      }
      float ar, ag, ab, aa;
      knit_argb_split(yarn, &aa, &ar, &ag, &ab);
      cb[y * W + x] = 0xff000000u | ((uint32_t)(ar * m.ground * 255) << 16)
                    | ((uint32_t)(ag * m.ground * 255) << 8) | (uint32_t)(ab * m.ground * 255);
    }
  }
  knit_blur(hf, W, H, tmp); knit_blur(hf, W, H, tmp);
  free(tmp);
  const float lx = -0.45f, ly = -0.55f, lz = 0.70f;
  float relief = scale * m.relief;
  uint32_t* out = (uint32_t*)malloc((size_t)W * H * 4);
  if (!out) { free(hf); free(cb); return NULL; }
  for (int y = 0; y < H; y++) {
    int up = (y - 1 + H) % H, dn = (y + 1) % H;
    for (int x = 0; x < W; x++) {
      int lf = (x - 1 + W) % W, rt = (x + 1) % W;
      float dzdx = (hf[y*W+rt] - hf[y*W+lf]) * relief;
      float dzdy = (hf[dn*W+x] - hf[up*W+x]) * relief;
      float inv = 1.f / sqrtf(dzdx*dzdx + dzdy*dzdy + 1.f);
      float ndl = ((-dzdx)*lx + (-dzdy)*ly + lz) * inv;
      if (ndl < 0) ndl = 0;
      float occ = sculpted ? 0.74f + 0.26f * hf[y*W+x] : 0.86f + 0.14f * hf[y*W+x];
      uint32_t grain = knit_mix((uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u);
      float fibre = sculpted ? 1.f : 0.993f + (grain & 255u) * (0.014f / 255.f);
      float shade = (m.ambient + m.sheen * ndl) * occ * fibre;
      uint32_t p = cb[y*W+x];
      float peak = fmaxf(p & 255, fmaxf((p >> 8) & 255, (p >> 16) & 255));
      float lift = sculpted ? fmaxf(0.f, 1.f - peak / 48.f) * hf[y*W+x] * 10.f : 0.f;
      float cbv = (p & 255) * shade + lift;
      float cgv = ((p >> 8) & 255) * shade + lift;
      float crv = ((p >> 16) & 255) * shade + lift;
      if (crv > 255) crv = 255; if (cgv > 255) cgv = 255; if (cbv > 255) cbv = 255;
      out[y*W+x] = 0xff000000u | ((uint32_t)crv << 16) | ((uint32_t)cgv << 8) | (uint32_t)cbv;
    }
  }
  free(hf); free(cb);
  *outW = W; *outH = H;
  return out;
}

static struct knit_tile make_tile(float band, uint32_t color, int chart) {
  const struct knit_chart* ch = (chart >= 0 && chart < g_chart_count) ? &g_charts[chart] : NULL;
  bool sculpted = (chart == KNIT_PATCH_CHART) || (ch && ch->sculpted_yarn);
  float rows = g_knit.rows; if (rows < 1.5f) rows = 1.5f;
  struct knit_gauge m = g_knit;
  if (sculpted) { m.row_overlap = 0.05f; }
  float row_step = band / rows, sh = row_step / (1.f - m.row_overlap), sw = row_step * m.aspect;
  (void)sh;
  int ncols, nrows;
  if (ch) {
    int density = sculpted ? 1 : 2;
    sw /= density; row_step /= density;
    ncols = ch->w * density; nrows = ch->h * density;
    float fx = 8.f / (sw * ncols), fy = 8.f / (row_step * nrows);
    if (fx > 1) ncols *= (int)ceilf(fx);
    if (fy > 1) nrows *= (int)ceilf(fy);
  } else { ncols = 8; nrows = 8; }
  int tw = (int)lroundf(sw * ncols), th = (int)lroundf(row_step * nrows);
  if (tw < 8) tw = 8; if (th < 8) th = 8;
  sw = (float)tw / ncols; row_step = (float)th / nrows;
  int S = (row_step < 4.f ? 4 : 2);
  uint32_t* px = NULL; int W = 0, H = 0;
  if (ch && !sculpted) px = knit_make_fabric_px(band, color, chart, sw, row_step, tw, th, S, &W, &H);
  else px = knit_make_plain_px(color, chart, sw, row_step, tw, th, S, sculpted, &W, &H);
  struct knit_tile t = { band, color, chart, px, tw, th, W, H };
  return t;
}

#define KNIT_CACHE_LEN 64
static struct knit_tile g_cache[KNIT_CACHE_LEN];
static int g_cache_n = 0;
static CRITICAL_SECTION g_lock;
static int g_lock_init = 0;
static void lock_ensure(void) {
  if (!g_lock_init) { InitializeCriticalSection(&g_lock); g_lock_init = 1; }
}
void knit_flush_cache(void) {
  lock_ensure(); EnterCriticalSection(&g_lock);
  for (int i = 0; i < g_cache_n; i++) free(g_cache[i].px);
  g_cache_n = 0; LeaveCriticalSection(&g_lock);
}
struct knit_tile knit_get_tile(float band, uint32_t color, int chart) {
  lock_ensure();
  band = roundf(band * 2.f) / 2.f;
  EnterCriticalSection(&g_lock);
  for (int i = 0; i < g_cache_n; i++) {
    if (g_cache[i].band == band && g_cache[i].color == color && g_cache[i].chart == chart) {
      struct knit_tile t = g_cache[i]; // shared buffer; release is no-op
      LeaveCriticalSection(&g_lock);
      return t;
    }
  }
  struct knit_tile t = make_tile(band, color, chart);
  if (!t.px) { LeaveCriticalSection(&g_lock); return t; }
  if (g_cache_n == KNIT_CACHE_LEN) { free(g_cache[0].px);
    memmove(g_cache, g_cache + 1, sizeof(g_cache[0]) * (KNIT_CACHE_LEN - 1)); g_cache_n--; }
  g_cache[g_cache_n++] = t; // cache owns one reference (same buffer)
  LeaveCriticalSection(&g_lock);
  return t; // shared buffer; do not free
}
void knit_tile_release(void* t) { (void)t; /* shared cache buffer */ }
