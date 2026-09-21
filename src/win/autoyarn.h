// autoyarn.h — icon-derived yarn for apps without curated colourways.
// Algorithm ported from macOS autoyarn.m; capture rewritten for Win32.
// Always fills *chart with a stable per-app fallback (zigzag/picnic/twinkle,
// like macOS) so unmatched apps never render plain; 0 color = use name hash.
#pragma once
#include <stdint.h>
#include <windows.h>
uint32_t autoyarn_color(const char* app, DWORD pid, int* chart);
