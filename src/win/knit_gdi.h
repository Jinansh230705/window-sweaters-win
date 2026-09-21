// knit_gdi.h — paints the sweater ring into a 32bpp premultiplied DIB.
#pragma once
#include <stdint.h>
#include <windows.h>
// win: target window rect in overlay-local coords. radius: corner radius px.
// style 'k' = knitted ring; anything else = solid yarn colour (dim applies).
void knit_paint_ring(uint32_t* bits, int fw, int fh, RECT win, float radius,
  float band, uint32_t color, int chart, float dim, float tuck, int anchor,
  char style);
