#ifndef CTM_H
#define CTM_H

#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

/* Establish target architecture.
 */

#define CTM_ARCH_linux 1
#define CTM_ARCH_raspi 2
#define CTM_ARCH_macos 3
#define CTM_ARCH_mswin 4

#if CTM_ARCH==CTM_ARCH_linux
  #define CTM_ARCH_NAME "Linux"
  #include <endian.h>

#elif CTM_ARCH==CTM_ARCH_raspi
  #define CTM_ARCH_NAME "Raspberry Pi"
  #include <endian.h>

#elif CTM_ARCH==CTM_ARCH_macos
  #define CTM_ARCH_NAME "MacOS"
  #include <machine/endian.h>

#elif CTM_ARCH==CTM_ARCH_mswin
  #define CTM_ARCH_NAME "Microsoft Windows"
  #define LITTLE_ENDIAN 1234
  #define BIG_ENDIAN    4321
  #define BYTE_ORDER LITTLE_ENDIAN

  // Fucking SDL redirects our output to a text file for some reason.
  // Adding fflush() after every printf() helps ensure that the text is preserved even after a crash.
  // This being Windows, we do need to be ready to crash. :)
  #define printf(fmt,...) ({ printf(fmt,##__VA_ARGS__); fflush(stdout); })
  #define fprintf(f,fmt,...) ({ fprintf(f,fmt,##__VA_ARGS__); fflush(f); })
  
#else
  #error "Illegal value for CTM_ARCH."
#endif

/* Finalize compile-time configuration.
 */

/* Common defines.
 */

#define CTM_TILESIZE 32

// CTM_TILESIZE was originally fixed at 16.
// This macro helps replace the magic numbers strewn all about our code.
#define CTM_RESIZE(k) ((CTM_TILESIZE*(k))/16)

struct ctm_bounds { int l,r,t,b; };
struct ctm_rgb { uint8_t r,g,b; };
struct ctm_rgba { uint8_t r,g,b,a; };

#endif
