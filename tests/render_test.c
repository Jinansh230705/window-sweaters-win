// render_test.c — headless sanity check: builds one knit tile per representative
// chart and paints a small ring into a BMP. Fails loudly on regression.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../src/core/knit_core.h"
#include "../src/core/charts.h"
#include "../src/core/apps.h"
#include "../src/win/knit_gdi.h"
#include "../src/win/autoyarn.h"

static int write_bmp(const char* path, uint32_t* px, int w, int h) {
  BITMAPFILEHEADER fh = {0}; BITMAPINFOHEADER ih = {0};
  fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof ih;
  fh.bfSize = fh.bfOffBits + w * h * 4;
  ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = -h;
  ih.biPlanes = 1; ih.biBitCount = 32; ih.biCompression = BI_RGB;
  FILE* f = fopen(path, "wb");
  if (!f) return 0;
  fwrite(&fh, 1, sizeof fh, f); fwrite(&ih, 1, sizeof ih, f);
  // BMP wants BGRA; our buffer is ARGB premultiplied-opaque: convert
  for (int i = 0; i < w * h; i++) {
    uint32_t p = px[i];
    uint32_t o = (p & 0xff000000u) | ((p & 255) << 16) | (p & 0xff00) | ((p >> 16) & 255);
    fwrite(&o, 1, 4, f);
  }
  fclose(f); return 1;
}

int main(void) {
  CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  int n = knit_charts_load(knit_charts_dir());
  printf("charts: %d\n", n);
  if (n < 40) { printf("FAIL: expected >=40 built-in charts\n"); return 1; }
  const char* probes[] = { "atelier-claude", "zigzag", "checker", "braid", NULL };
  for (int i = 0; probes[i]; i++) {
    int idx = knit_chart_index(probes[i]);
    if (idx < 0) { printf("FAIL: missing chart %s\n", probes[i]); return 1; }
    struct knit_tile t = knit_get_tile(12.f, 0xffd58561u, idx);
    if (!t.px || t.pw <= 0) { printf("FAIL: tile gen %s\n", probes[i]); return 1; }
    printf("tile %-16s %dx%dpt %dx%dpx ok\n", probes[i], t.w, t.h, t.pw, t.ph);
  }
  // By-App resolution on Windows exe basenames (ports apps.c matching).
  const struct app_rule* r = knit_app_rule("chrome");
  if (!r || strcmp(r->chart, "atelier-chrome") != 0) { printf("FAIL: chrome rule\n"); return 1; }
  int ci = knit_pattern_for_app("chrome");
  if (ci != knit_chart_index("atelier-chrome")) { printf("FAIL: chrome pattern %d\n", ci); return 1; }
  if (knit_pattern_for_app("some-random-app-xyz") != -1) { printf("FAIL: unknown app should be plain\n"); return 1; }
  printf("app matching ok (chrome -> atelier-chrome, unknown -> plain)\n");
  // Unmatched apps must get a stable macOS-style fallback pattern
  // (zigzag/picnic/twinkle), never plain, stable across calls.
  {
    int a1 = -2, a2 = -2;
    autoyarn_color("some-random-app-xyz", 12345, &a1);
    autoyarn_color("some-random-app-xyz", 12345, &a2);
    int z = knit_chart_index("zigzag"), p = knit_chart_index("picnic"), t = knit_chart_index("twinkle");
    if (a1 != a2 || ((a1 != z) && (a1 != p) && (a1 != t))) {
      printf("FAIL: autoyarn fallback chart %d/%d\n", a1, a2); return 1;
    }
    char name[64] = {0};
    snprintf(name, sizeof name, "%s", g_charts[a1].name);
    printf("autoyarn fallback ok (%s, stable)\n", name);
  }
  // Sculpted charts must render their contrast yarn, not just the base colour.
  // atelier-slack yarns: blue 63c5df, green 82c5a6... on a red base the green
  // stitches must show up.
  {
    int slack = knit_chart_index("atelier-slack");
    struct knit_tile t = knit_get_tile(12.f, 0xffd58561u, slack);
    int green = 0, total = t.pw * t.ph;
    for (int i = 0; i < total; i++) {
      uint32_t p = t.px[i];
      int rr = (p >> 16) & 255, gg = (p >> 8) & 255, bb = p & 255;
      if (gg > 100 && gg > rr + 20 && gg > bb + 20) green++;
    }
    printf("slack contrast stitches: %d/%d pixels\n", green, total);
    if (green < total / 50) { printf("FAIL: sculpted chart renders no contrast yarn\n"); return 1; }
  }
  // ring paint smoke test: 320x240 overlay, window 200x140
  int fw = 320, fh = 240;
  uint32_t* bits = calloc((size_t)fw * fh, 4);
  RECT win = { 60, 50, 260, 190 };
  int z = knit_chart_index("zigzag");
  knit_paint_ring(bits, fw, fh, win, 9.f, 12.f, 0xffd58561u, z, 0.f, 14.f, 0, 'k');
  int opaque = 0;
  for (int i = 0; i < fw * fh; i++) if (bits[i] >> 24) opaque++;
  printf("ring opaque pixels: %d\n", opaque);
  if (opaque < 1000) { printf("FAIL: ring nearly empty\n"); return 1; }
  if (!write_bmp("out\\tile_test.bmp", bits, fw, fh)) { printf("FAIL: bmp write\n"); return 1; }
  printf("PASS: out\\tile_test.bmp written\n");
  // Solid style: every ring pixel is exactly the yarn colour.
  {
    memset(bits, 0, (size_t)fw * fh * 4);
    knit_paint_ring(bits, fw, fh, win, 9.f, 12.f, 0xff112233u, -1, 0.f, 14.f, 0, 's');
    int n = 0, bad = 0;
    for (int i = 0; i < fw * fh; i++) if (bits[i] >> 24) { n++; if (bits[i] != 0xff112233u) bad++; }
    if (!n || bad) { printf("FAIL: solid style n=%d bad=%d\n", n, bad); return 1; }
    printf("solid style ok (%d pixels)\n", n);
  }
  // Cuff chart (atelier-notes) paints without crashing and is non-empty.
  {
    memset(bits, 0, (size_t)fw * fh * 4);
    int notes = knit_chart_index("atelier-notes");
    knit_paint_ring(bits, fw, fh, win, 9.f, 12.f, 0xfff6f0d9u, notes, 0.f, 14.f, 0, 'k');
    int n = 0;
    for (int i = 0; i < fw * fh; i++) if (bits[i] >> 24) n++;
    if (n < 1000) { printf("FAIL: cuff ring n=%d\n", n); return 1; }
    printf("cuff ring ok (%d pixels)\n", n);
  }
  free(bits);
  return 0;
}
