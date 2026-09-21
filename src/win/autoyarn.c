// autoyarn.c — sample the app icon, soften to yarn (ports autoyarn.m logic).
#include "autoyarn.h"
#include "../core/knit_core.h"
#include "../core/charts.h"
#include <shellapi.h>
#include <math.h>
#include <stdio.h>

#define CACHE_N 64
static struct { DWORD pid; uint32_t color; char app[64]; } cache[CACHE_N];
static int cache_n = 0;
static CRITICAL_SECTION lk; static int lk_init = 0;

static uint32_t soften(uint8_t r, uint8_t g, uint8_t b) {
  double R = r/255., G = g/255., B = b/255.;
  double mx = fmax(R, fmax(G, B)), mn = fmin(R, fmin(G, B));
  double l = (mx+mn)/2, h = 0, s = 0;
  if (mx != mn) {
    double d = mx - mn;
    s = l > 0.5 ? d/(2-mx-mn) : d/(mx+mn);
    h = mx==R ? (G-B)/d + (G<B?6:0) : mx==G ? (B-R)/d+2 : (R-G)/d+4; h /= 6;
  }
  s *= 0.48; if (s < 0.22) s = 0.22; if (s > 0.60) s = 0.60;
  if (l < 0.30) l = 0.30; if (l > 0.72) l = 0.72;
  double q = l < 0.5 ? l*(1+s) : l+s-l*s, p = 2*l-q, t[3] = {h+1./3,h,h-1./3}, o[3];
  for (int i = 0; i < 3; i++) {
    double c = t[i]; if (c < 0) c += 1; if (c > 1) c -= 1;
    o[i] = c<1./6 ? p+(q-p)*6*c : c<0.5 ? q : c<2./3 ? p+(q-p)*(2./3-c)*6 : p;
  }
  return 0xff000000u | ((uint32_t)(o[0]*255)<<16) | ((uint32_t)(o[1]*255)<<8) | (uint32_t)(o[2]*255);
}

static int icon_avg(DWORD pid, uint8_t* R, uint8_t* G, uint8_t* B) {
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h) return 0;
  char path[MAX_PATH] = {0}; DWORD n = sizeof path;
  int ok = 0;
  if (QueryFullProcessImageNameA(h, 0, path, &n)) {
    SHFILEINFOA fi = {0};
    if (SHGetFileInfoA(path, 0, &fi, sizeof fi, SHGFI_ICON | SHGFI_LARGEICON) && fi.hIcon) {
      ICONINFO ii = {0};
      if (GetIconInfo(fi.hIcon, &ii) && ii.hbmColor) {
        BITMAP bm = {0};
        if (GetObjectA(ii.hbmColor, sizeof bm, &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
          int w = bm.bmWidth, hgt = bm.bmHeight;
          HDC dc = CreateCompatibleDC(NULL);
          BITMAPINFO bi = {0};
          bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w;
          bi.bmiHeader.biHeight = -hgt; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
          uint32_t* bits = NULL;
          HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
          if (dib && bits) {
            HGDIOBJ old = SelectObject(dc, dib);
            DrawIconEx(dc, 0, 0, fi.hIcon, w, hgt, 0, NULL, DI_NORMAL);
            SelectObject(dc, old);
            long sr=0, sg=0, sb=0, cnt=0;
            for (int i = 0; i < w*hgt; i++) {
              uint32_t p = bits[i];
              uint8_t a = (p>>24)&255, r=(p>>16)&255, g=(p>>8)&255, b=p&255;
              if (a < 128) continue;
              int mx = r>g?(r>b?r:b):(g>b?g:b), mn = r<g?(r<b?r:b):(g<b?g:b);
              if (mx - mn < 24) continue; // skip greys
              sr+=r; sg+=g; sb+=b; cnt++;
            }
            if (cnt > 8) { *R=(uint8_t)(sr/cnt); *G=(uint8_t)(sg/cnt); *B=(uint8_t)(sb/cnt); ok = 1; }
            DeleteObject(dib);
          }
          DeleteDC(dc);
        }
        DeleteObject(ii.hbmColor); if (ii.hbmMask) DeleteObject(ii.hbmMask);
      }
      DestroyIcon(fi.hIcon);
    }
  }
  CloseHandle(h);
  return ok;
}

uint32_t autoyarn_color(const char* app, DWORD pid, int* chart) {
  // Stable per-app fallback pattern for apps without a curated colourway:
  // macOS deals zigzag/picnic/twinkle by app and keeps it for life.
  static const char* opts[] = { "zigzag", "picnic", "twinkle" };
  uint32_t h = 2166136261u;
  for (const unsigned char* p = (const unsigned char*)(app ? app : ""); *p; p++) {
    unsigned char c = *p;
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    h = (h ^ c) * 16777619u;
  }
  if (chart) *chart = knit_chart_index(opts[h % 3]);
  if (!lk_init) { InitializeCriticalSection(&lk); lk_init = 1; }
  EnterCriticalSection(&lk);
  for (int i = 0; i < cache_n; i++) if (cache[i].pid == pid) { uint32_t c = cache[i].color; LeaveCriticalSection(&lk); return c; }
  LeaveCriticalSection(&lk);
  uint8_t r=0,g=0,b=0;
  uint32_t c = 0;
  if (icon_avg(pid, &r, &g, &b)) c = soften(r, g, b);
  EnterCriticalSection(&lk);
  if (cache_n == CACHE_N) { memmove(cache, cache+1, sizeof(cache[0])*(CACHE_N-1)); cache_n--; }
  cache[cache_n].pid = pid; cache[cache_n].color = c;
  snprintf(cache[cache_n].app, 64, "%s", app ? app : "");
  cache_n++;
  LeaveCriticalSection(&lk);
  return c;
}
