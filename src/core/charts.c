// charts.c — built-in collection (ported data from macOS chart.c) + user PNG
// overrides via WIC. Built-ins need no files.
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include "charts.h"
#include <shlobj.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct knit_chart g_charts[KNIT_CHART_MAX];
int g_chart_count = 0;
unsigned g_charts_generation = 0;
int g_chart_active = -1;

const char* knit_charts_dir(void) {
  static char dir[MAX_PATH];
  if (dir[0]) return dir;
  wchar_t* base = NULL;
  if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &base))) {
    WideCharToMultiByte(CP_UTF8, 0, base, -1, dir, sizeof dir, NULL, NULL);
    CoTaskMemFree(base);
    strncat(dir, "\\WindowSweaters", sizeof(dir) - strlen(dir) - 1);
    CreateDirectoryA(dir, NULL);
    strncat(dir, "\\charts", sizeof(dir) - strlen(dir) - 1);
    CreateDirectoryA(dir, NULL);
  }
  return dir;
}

struct collection_chart { const char* name; const char* rows[12]; uint32_t yarn[6]; };
// Full collection ported from upstream src/chart.c k_collection.
static const struct collection_chart k_collection[] = {
  { "atelier-finder", { "..aaaa....aaaa..","..aaaa....aaaa..","..aaaa....aaaa..","aa....aaaa....aa","aa....aaaa....aa","b...b......b...b" }, { 0xffb7e4f7u, 0xff24558bu } },
  { "atelier-terminal", { "aa....aa","aa....aa","aa....aa","aa....aa","aa....aa","aa....aa" }, { 0xffa5bda0u } },
  { "atelier-grok", { "aaa.b..b.aaa","aaa.b..b.aaa","aaa.b..b.aaa","aaa.b..b.aaa","aaa.b..b.aaa","aaa.b..b.aaa" }, { 0xffeeece4u, 0xff868a88u } },
  { "atelier-teams", { "a....aa....a","aa........aa",".aa......aa.","..aa....aa..","...aa..aa...","....aaaa...." }, { 0xfff4efeeu } },
  { "atelier-claude", { "............","..a.....a...",".aaa...aaa..","..a.....a...","............","............" }, { 0xfff7e8c5u } },
  { "atelier-codex", { "aa.....b....","aa.....b....","aa.....b....","aa.....b....","aa.....b....","aa.....b...." }, { 0xffe6e6ceu, 0xffe9bfcbu } },
  { "atelier-spotify", { "...aaa...aaa","...aaa...aaa","...aaa...aaa","aaa...aaa...","aaa...aaa...","aaa...aaa..." }, { 0xffe5d586u } },
  { "atelier-notion", { "...aaa...aaa","...aaa...aaa","...aaa...aaa","aaa...aaa...","aaa...aaa...","aaa...aaa..." }, { 0xff494947u } },
  { "atelier-whatsapp", { "aaaa....b...","aaaa....b...","aaaa....b...","aaaa....b...","aaaa....b...","aaaa....b..." }, { 0xffdde9bcu, 0xffe3a2b8u } },
  { "atelier-figma", { "aabb....cc..","aabb....cc..","aabb....cc..","....ddee....","....ddee....","....ddee...." }, { 0xffeaaf96u, 0xffdcd092u, 0xff8dbccfu, 0xffafc5a1u, 0xffded3e9u } },
  { "atelier-chrome", { "....aaaaaaaa........bbbbbbbb........dddddddd........aaaaaaaa........bbbbbbbb........dddddddd........cccccccc....","....aaaaaaaa........bbbbbbbb........dddddddd........aaaaaaaa........bbbbbbbb........dddddddd........cccccccc....","....aaaaaaaa........bbbbbbbb........dddddddd........aaaaaaaa........bbbbbbbb........dddddddd........cccccccc....","aaaa........bbbbbbbb........dddddddd........aaaaaaaa........bbbbbbbb........dddddddd........cccccccc........aaaa","aaaa........bbbbbbbb........dddddddd........aaaaaaaa........bbbbbbbb........dddddddd........cccccccc........aaaa","aaaa........bbbbbbbb........dddddddd........aaaaaaaa........bbbbbbbb........dddddddd........cccccccc........aaaa" }, { 0xffd8675bu, 0xff6ba776u, 0xff4285f4u, 0xffedcc70u } },
  { "atelier-paper", { "..aaaa..",".a...a..",".a.b.a..",".aaaaa..",".aaa....","........" }, { 0xfff5f5f2u, 0xff5f8bceu } },
  { "atelier-safari", { "..a.....a...",".aaa...aaa..","aabaa.aabaa.",".aaa...aaa..","..a.....a...","............" }, { 0xfff6f2e8u, 0xffed7066u } },
  { "atelier-firefox", { "a.....a.....","aa....aa....",".aa....aa...","..bb....bb..","...bb....bb.","....b.....b." }, { 0xffff873eu, 0xffffc167u } },
  { "atelier-cursor", { "a....aa....a","aa........aa",".aa......aa.","..aa....aa..","...aa..aa...","....aaaa...." }, { 0xffc4c2b9u } },
  { "atelier-slack", { ".a......b...","aaa....bbb..",".a......b...","....c......d","...ccc....dd","....c......d" }, { 0xff63c5dfu, 0xff82c5a6u, 0xffedc45eu, 0xffe986a3u } },
  { "atelier-zoom", { ".aaaa...aaaa",".abba...abba",".abba...abba",".aaaa...aaaa","............","bbbbbbbbbbbb" }, { 0xfff6f8f4u, 0xff9ec7f5u } },
  { "atelier-telegram", { "a.....a.....","aa....aa....","aba...aba...",".aba...aba..","..aa....aa..","...a.....a.." }, { 0xfff5f5ecu, 0xffaddde8u } },
  { "atelier-messages", { "................","..aa......aa....","..aa......aa....","................","......bb......bb","......bb......bb" }, { 0xfff8f7e8u, 0xffb7db8du } },
  { "atelier-mail", { "a.....a.....",".a...a.a...a","..a.a...a.a.","...a.....a..","............","bbbbbbbbbbbb" }, { 0xfff5f5f0u, 0xffb3d8eeu } },
  { "atelier-notes", { "aaaaaaaaaaaa","aaaaaaaaaaaa","............","............","............","bbbbbbbbbbbb" }, { 0xffefc852u, 0xfffffcf3u } },
  { "atelier-calendar", { "aaaaaaaaaaaa","aaaaaaaaaaaa","............","............","............","............" }, { 0xffe55c52u } },
  { "atelier-reminders", { "a.dd..b.dd..","............","c.dd..a.dd..","............","b.dd..c.dd..","............" }, { 0xff579fdeu, 0xffe5787au, 0xffeba451u, 0xffccc6bau } },
  { "atelier-music", { "a.....a.....","aa....aa....",".aa....aa...","..bb....bb..","...bb....bb.","....b.....b." }, { 0xfff7acc0u, 0xfffff0dbu } },
  { "atelier-photos", { ".a...c...e..","aaa.ccc.eee.",".a...c...e..","...b...d...f","..bbb.ddd.ff","...b...d...f" }, { 0xffefa470u, 0xffeccb67u, 0xff95ba78u, 0xff7bbbc9u, 0xff9991c1u, 0xffd98bacu } },
  { "atelier-preview", { "bbbbbbbbbbbb","b.....b.....","b..a..b..a..","b.aaa.b.aaa.","baaaaabaaaaa","bbbbbbbbbbbb" }, { 0xfff2f4efu, 0xffaacfddu } },
  { "atelier-word", { "a....aa....a","aa........aa",".aa......aa.","..bb....bb..","...bb..bb...","....bbbb...." }, { 0xfff3f1e6u, 0xff82a8deu } },
  { "atelier-excel", { "aa...baa...b","aa...baa...b","bbbbbbbbbbbb","..aa.b..aa.b","..aa.b..aa.b","bbbbbbbbbbbb" }, { 0xff82b99au, 0xffdbead0u } },
  { "atelier-powerpoint", { ".aaaa...aaaa",".abba...abba",".abba...abba",".aaaa...aaaa","............","..b.....b..." }, { 0xffeea58au, 0xfff5d9b8u } },
  { "atelier-outlook", { "aaa...aaa...","aba...aba...","aab...aab...","...aaa...aaa","...aba...aba","...aab...aab" }, { 0xff8ac6ecu, 0xfff5f5eau } },
  { "atelier-vscode", { "a....ba....b",".a..ab.a..ab","..aa.b..aa.b","..aa.b..aa.b",".a..ab.a..ab","a....ba....b" }, { 0xffb6e1edu, 0xff17374fu } },
  { "atelier-photoshop", { "aaaa..aaaa..","a..a..a..a..","aaaa..aaaa..","...bbb...bbb","...b.b...b.b","...bbb...bbb" }, { 0xff58b7e9u, 0xff8cbad0u } },
  { "atelier-illustrator", { "...baaaab...","...baaaab...","...baaaab...","...baaaab...","...baaaab...","...baaaab..." }, { 0xfff5a13du, 0xfff8d2a0u } },
  { "atelier-granola", { "...aaaaaa...","...aaaaaa...","...aaaaaa...","...aaaaaa...","...aaaaaa...","...aaaaaa..." }, { 0xff8ba66au } },
  { "atelier-chatgpt", { "a....aa....a","aa........aa",".aa......aa.","..aa....aa..","...aa..aa...","....aaaa...." }, { 0xfff6f0deu } },
  { "atelier-ghostty", { "aa..........","aa..........","aa..........","aa..........","aa..........","aa.........." }, { 0xffecebe4u } },
  { "braid", { "abb........a","a.bb......aa","...bb....aaa","....bb..aaa.",".....bbaaa..","......bba...",".....aabb...","....aaa.bb..","...aaa...bb.","..aaa.....bb","baaa.......b","bba........." }, { 0xfff6f0deu, 0xfff078aau } },
  { "blockstripe", { "...aaa" }, { 0xffffffffu } },
  { "checker", { "..aa","..aa","aa..","aa.." }, { 0xfff2eee4u } },
  { "seedling", { "....a.","....a.","...a.a",".a....",".a....","a.a..." }, { 0xff96d6a0u } },
  { "trim", { "...a....a.","..........","bb...bb...","..........","c...cc...c","cc...cc..." }, { 0xffffffffu, 0xfff58220u, 0xffd61e6eu } },
  { "picnic", { "...aaa...aaa","...aaa...aaa","...aaa...aaa","aaa...aaa...","aaa...aaa...","aaa...aaa..." }, { 0xfff4e6bfu } },
  { "ribbon", { "aa.....b....","aa.....b....","aa.....b....","aa.....b....","aa.....b....","aa.....b...." }, { 0xfff5e8cfu, 0xffe3a1b3u } },
  { "posy", { "............",".aa.aa......",".aaaaa......","..aba.......",".a...a......","............" }, { 0xfff0d1dbu, 0xfff2dd9du } },
  { "twinkle", { "............","..a.....a...",".aaa...aaa..","..a.....a...","............","............" }, { 0xfff5e6bfu } },
  { "candy-stripe", { "aaa...bbb...","aaa...bbb...","aaa...bbb...","aaa...bbb...","aaa...bbb...","aaa...bbb..." }, { 0xfff5e8cfu, 0xffe3a1b3u } },
  { "zigzag", { "a....aa....a","aa........aa",".aa......aa.","..aa....aa..","...aa..aa...","....aaaa...." }, { 0xfff6f0deu } },
};

static void load_collection(void) {
  for (size_t i = 0; i < sizeof(k_collection)/sizeof(k_collection[0]); i++) {
    const struct collection_chart* spec = &k_collection[i];
    int w = (int)strlen(spec->rows[0]), h = 0;
    while (h < 12 && spec->rows[h]) h++;
    uint32_t* pixels = (uint32_t*)calloc((size_t)w * h, 4);
    if (!pixels) continue;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      char yarn = spec->rows[y][x];
      if (yarn >= 'a' && yarn <= 'f') pixels[y*w+x] = spec->yarn[yarn-'a'];
    }
    struct knit_chart* c = &g_charts[g_chart_count++];
    snprintf(c->name, sizeof c->name, "%s", spec->name);
    c->w = w; c->h = h; c->px = pixels;
    if (strcmp(spec->name, "atelier-whatsapp") == 0) { c->solid_corners = true; c->corner_color = 0; }
    if (strcmp(spec->name, "atelier-notes") == 0 || strcmp(spec->name, "atelier-calendar") == 0) {
      c->cuff_color = spec->yarn[0];
      for (int k = 0; k < w*h; k++) if (pixels[k] == c->cuff_color) pixels[k] = 0;
    }
    c->round_dots = strcmp(spec->name, "atelier-messages") == 0;
    c->fitted_repeat = strcmp(spec->name,"atelier-finder")==0 || strcmp(spec->name,"atelier-terminal")==0
      || strcmp(spec->name,"atelier-grok")==0 || strcmp(spec->name,"atelier-granola")==0
      || strcmp(spec->name,"atelier-illustrator")==0 || strcmp(spec->name,"atelier-chrome")==0;
    c->sculpted_yarn = !c->round_dots;
    c->defined_yarn = true;
  }
}

// Load one user PNG via WIC into ARGB (row 0 = top). Returns 1 on success.
static int load_one_wic(const char* path, const char* name, struct knit_chart* out) {
  wchar_t wpath[MAX_PATH];
  MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH);
  IWICImagingFactory* fac = NULL;
  if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
      &IID_IWICImagingFactory, (void**)&fac))) return 0;
  IWICBitmapDecoder* dec = NULL;
  int ok = 0;
  if (SUCCEEDED(IWICImagingFactory_CreateDecoderFromFilename(fac, wpath, NULL,
      GENERIC_READ, WICDecodeMetadataCacheOnLoad, &dec))) {
    IWICBitmapFrameDecode* fr = NULL;
    if (SUCCEEDED(IWICBitmapDecoder_GetFrame(dec, 0, &fr))) {
      UINT w = 0, h = 0;
      IWICBitmapSource_GetSize(fr, &w, &h);
      if (w >= 1 && h >= 1 && w <= 256 && h <= 256) {
        IWICFormatConverter* cv = NULL;
        if (SUCCEEDED(IWICImagingFactory_CreateFormatConverter(fac, &cv))) {
          if (SUCCEEDED(IWICFormatConverter_Initialize(cv, (IWICBitmapSource*)fr,
              &GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.f,
              WICBitmapPaletteTypeCustom))) {
            uint32_t* px = (uint32_t*)malloc((size_t)w * h * 4);
            if (px && SUCCEEDED(IWICFormatConverter_CopyPixels(cv, NULL,
                (UINT)(w * 4), (UINT)(w * h * 4), (BYTE*)px))) {
              // WIC gives BGRA bottom-up? CopyPixels is top-down; convert BGRA->ARGB
              for (UINT k = 0; k < w * h; k++) {
                uint32_t bgra = px[k];
                uint32_t b = bgra & 255, g = (bgra >> 8) & 255, r = (bgra >> 16) & 255, a = (bgra >> 24) & 255;
                px[k] = (a << 24) | (r << 16) | (g << 8) | b;
              }
              out->px = px; out->w = (int)w; out->h = (int)h;
              out->sculpted_yarn = true; out->defined_yarn = true;
              snprintf(out->name, sizeof out->name, "%s", name);
              ok = 1;
            } else free(px);
          }
          IWICFormatConverter_Release(cv);
        }
      }
      IWICBitmapFrameDecode_Release(fr);
    }
    IWICBitmapDecoder_Release(dec);
  }
  IWICImagingFactory_Release(fac);
  return ok;
}

static int cmp_str(const void* a, const void* b) {
  return strcmp(*(const char**)a, *(const char**)b);
}

int knit_charts_load(const char* dir) {
  g_charts_generation++;
  char active[64] = {0};
  if (g_chart_active >= 0 && g_chart_active < g_chart_count)
    snprintf(active, sizeof active, "%s", g_charts[g_chart_active].name);
  for (int i = 0; i < g_chart_count; i++) free(g_charts[i].px);
  memset(g_charts, 0, sizeof g_charts);
  g_chart_count = 0;
  load_collection();
  if (dir && *dir) {
    char pat[MAX_PATH];
    snprintf(pat, sizeof pat, "%s\\*.png", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    char* names[KNIT_CHART_MAX]; int nn = 0;
    while (h != INVALID_HANDLE_VALUE) {
      size_t L = strlen(fd.cFileName);
      if (L > 4 && _stricmp(fd.cFileName + L - 4, ".png") == 0 && nn < KNIT_CHART_MAX) {
        names[nn] = _strdup(fd.cFileName); nn++;
      }
      if (!FindNextFileA(h, &fd)) break;
    }
    if (h != INVALID_HANDLE_VALUE) FindClose(h);
    qsort(names, nn, sizeof(char*), cmp_str);
    for (int i = 0; i < nn; i++) {
      char* dot = strrchr(names[i], '.'); *dot = 0;
      char path[MAX_PATH];
      snprintf(path, sizeof path, "%s\\%s.png", dir, names[i]);
      int target = knit_chart_index(names[i]);
      struct knit_chart c = {0};
      if (load_one_wic(path, names[i], &c)) {
        c.custom = true;
        if (target < 0) { if (g_chart_count < KNIT_CHART_MAX) target = g_chart_count++; else { free(c.px); } }
        else {
          c.solid_corners = g_charts[target].solid_corners; c.corner_color = g_charts[target].corner_color;
          c.cuff_color = g_charts[target].cuff_color; c.round_dots = g_charts[target].round_dots;
          c.fitted_repeat = g_charts[target].fitted_repeat;
          c.sculpted_yarn = g_charts[target].sculpted_yarn; c.defined_yarn = g_charts[target].defined_yarn;
          free(g_charts[target].px);
        }
        if (target >= 0) g_charts[target] = c;
      }
      free(names[i]);
    }
  }
  g_chart_active = active[0] ? knit_chart_index(active) : -1;
  return g_chart_count;
}

int knit_chart_index(const char* name) {
  if (!name) return -1;
  for (int i = 0; i < g_chart_count; i++) if (strcmp(g_charts[i].name, name) == 0) return i;
  return -1;
}
