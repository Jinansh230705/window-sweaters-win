// Portable knit core: palettes, color assignment, noise, CPU fabric shader.
// Reused from macOS knit.c math; rendering backend (CG) replaced by raw ARGB
// buffers consumed by the Win32 GDI painter (src/win/knit_gdi.c).
#pragma once
#include <stdint.h>
#include <stdbool.h>

enum knit_stitch { KNIT_STOCKINETTE = 0, KNIT_RIB, KNIT_GARTER, KNIT_STITCH_COUNT };
enum knit_anchor { KNIT_ANCHOR_CORNER = 0, KNIT_ANCHOR_CENTRE };

struct knit_gauge {
  float rows, aspect, row_overlap, yarn, bow, jitter;
  float ground, shadow, light, ambient, depth, relief, sheen;
  float tuck;
};
struct knit_basket { const char* name; const uint32_t* colors; int len; };

extern struct knit_gauge g_knit;
extern int g_knit_stitch, g_knit_basket, g_knit_anchor;
extern float g_knit_dim;
extern bool g_knit_on;
extern const struct knit_basket g_knit_baskets[];
extern const int g_knit_basket_count;
extern const char* g_knit_stitch_names[];

#define KNIT_CREAM 0xfff6f0deu
#define KNIT_PATCH_CHART (-2)

uint32_t knit_mix(uint32_t h);
uint32_t knit_color_for_window(uint32_t wid);
uint32_t knit_color_for_app(const char* app);
uint32_t knit_zigzag_contrast(uint32_t base);
float knit_noise01(int a, int b);

// Raw-ARGB tile cache (portable). Key: band + color + chart.
struct knit_tile { float band; uint32_t color; int chart; uint32_t* px; int w, h, pw, ph; };
void knit_flush_cache(void);
// Returns a tile with caller-owned reference (must call knit_tile_release).
struct knit_tile knit_get_tile(float band, uint32_t color, int chart);
void knit_tile_release(void* t);

static inline void knit_argb_split(uint32_t c, float* a, float* r, float* g, float* b) {
  *a = ((c >> 24) & 255) / 255.f; *r = ((c >> 16) & 255) / 255.f;
  *g = ((c >> 8) & 255) / 255.f;  *b = (c & 255) / 255.f;
}
