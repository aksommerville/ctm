/* akpng.h
 * Minimal PNG decoder.
 *
 * Deviation from spec:
 *  - Interlaced images are not supported.
 *  - CRCs are never checked.
 *  - IEND not required.
 *  - IHDR not required to be first chunk, and may be >13 bytes.
 *  - Intervening chunks permitted between IDAT.
 *  - PLTE never required nor forbidden, and its placement doesn't matter.
 *  - ditto for tRNS.
 *  - Any pixel format of size (1,2,4,8,16,24,32,40,48,56,64) is permitted.
 *
 * Most of these deviations are trivial.
 * The only likely to bite you is the interlacing.
 * But seriously, don't interlace your graphics.
 *
 */

#ifndef AKPNG_H
#define AKPNG_H

/* Simple interface.
 */

#include <stdint.h>

// Initialize to zeroes. Full definition is here, further down.
struct akpng;

// Clean up context after any akpng call, whether it succeeds or not.
void akpng_cleanup(struct akpng *png);

/* Decode entire file into a fresh context.
 * Behavior undefined if this context has been used for something else.
 * 'decode_file' is just for your convenience.
 * It is no more memory-efficient than reading the file yourself.
 * (Any way you slice it, akpng must have the entire encoded stream and the entire decoded image in memory at once).
 * (Which is really not a big deal on any realistic system!).
 */
int akpng_decode(struct akpng *png,const void *src,int srcc);
int akpng_decode_file(struct akpng *png,const char *path);

/* Convert to 24-bit RGB or 32-bit RGBA.
 * For general conversion, see akimgcvt.h.
 * NB: Indexed images become grayscale; we never check for PLTE.
 */
int akpng_force_rgb(struct akpng *png);
int akpng_force_rgba(struct akpng *png);

/* Organized access to context contents.
 */

void *akpng_get_pixels(const struct akpng *png);
void *akpng_steal_pixels(struct akpng *png); // => handoff; caller must free.
int akpng_get_width(const struct akpng *png);
int akpng_get_height(const struct akpng *png);
int akpng_get_colortype(const struct akpng *png);
int akpng_get_depth(const struct akpng *png);
int akpng_get_stride(const struct akpng *png); // Knowable from width, colortype, and depth.

/* Internals.
 * Feel free to mess around with the context object itself.
 * It helps if you have some clue what you're doing.
 * If not, hey, not my problem.
 */

// The boolean sense of preservation flags was carefully chosen so that zero would be a nice default.
#define AKPNG_PRESERVE_OTHER      0x0001 // Keep unknown chunks (all except: IHDR,IDAT,IEND,PLTE,tRNS).
#define AKPNG_PRESERVE_NO_PLTE    0x0002 // Discard PLTE.
#define AKPNG_PRESERVE_NO_TRNS    0x0004 // Discard tRNS.

struct akpng_chunk {
  char chunkid[4];
  uint8_t *v;
  int c;
};

struct akpng {

  void *pixels;
  int w,h;
  uint8_t colortype;
  uint8_t depth;
  uint8_t compression;
  uint8_t filter;
  uint8_t interlace;

  // AKPNG_PRESERVE_* bits; which non-image chunks should we keep?
  // Default (zero) keeps only PLTE and tRNS.
  int preservation;

  // Whichever chunks you selected in (preservation) are dumped here.
  // Their order relative to each other is preserved.
  // Order relative to IHDR and IDAT is lost.
  struct akpng_chunk *chunkv;
  int chunkc,chunka;

  // Transient decoder state.
  int y;
  uint8_t *row;   // Points into pixels.
  uint8_t *pvrow; // Points into pixels. NULL at the first row.
  uint8_t rowfilter;
  int phase; // 0:filter, 1:row
  void *z;
  int pixelsize;
  int stride;
  int xstride; // Stride column-to-column in pixels, minimum 1, for filter purposes.

};

#endif
