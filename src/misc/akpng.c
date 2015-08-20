#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include "akpng.h"

#define Z ((z_stream*)png->z)

/* Cleanup.
 */

void akpng_cleanup(struct akpng *png) {
  if (!png) return;
  if (png->pixels) free(png->pixels);
  if (png->chunkv) {
    while (png->chunkc-->0) if (png->chunkv[png->chunkc].v) free(png->chunkv[png->chunkc].v);
    free(png->chunkv);
  }
  if (png->z) {
    inflateEnd(png->z);
    free(png->z);
  }
  memset(png,0,sizeof(struct akpng));
}

/* Miscellaneous decode helpers.
 */

static int akpng_channel_count(int colortype) {
  switch (colortype) {
    case 0: return 1;
    case 2: return 3;
    case 3: return 1;
    case 4: return 2;
    case 6: return 4;
    default: return 0;
  }
}

/* Simple property access.
 */
 
void *akpng_get_pixels(const struct akpng *png) {
  if (!png) return 0;
  return png->pixels;
}

void *akpng_steal_pixels(struct akpng *png) {
  if (!png) return 0;
  void *rtn=png->pixels;
  png->pixels=0;
  return rtn;
}

int akpng_get_width(const struct akpng *png) {
  if (!png) return 0;
  return png->w;
}

int akpng_get_height(const struct akpng *png) {
  if (!png) return 0;
  return png->h;
}

int akpng_get_colortype(const struct akpng *png) {
  if (!png) return 0;
  return png->colortype;
}

int akpng_get_depth(const struct akpng *png) {
  if (!png) return 0;
  return png->depth;
}

int akpng_get_stride(const struct akpng *png) {
  if (!png) return 0;
  int chanc=akpng_channel_count(png->colortype);
  if (chanc<1) return 0;
  int pixelsize=chanc*png->depth;
  if (pixelsize<1) return 0;
  if (png->w>(INT_MAX-7)/pixelsize) return 0;
  return (pixelsize*png->w+7)>>3;
}

/* Decode single row.
 */

static inline uint8_t akpng_paeth(uint8_t a,uint8_t b,uint8_t c) {
  int p=a+b-c;
  int pa=a-p; if (pa<0) pa=-pa;
  int pb=b-p; if (pb<0) pb=-pb;
  int pc=c-p; if (pc<0) pc=-pc;
  if ((pa<=pb)&&(pa<=pc)) return a;
  if (pb<=pc) return b;
  return c;
}

static int akpng_decode_row(struct akpng *png) {
  int i;
  switch (png->rowfilter) {

    case 0: break;

    case 1: {
        for (i=png->xstride;i<png->stride;i++) png->row[i]+=png->row[i-png->xstride];
      } break;

    case 2: if (png->pvrow) {
        for (i=0;i<png->stride;i++) png->row[i]+=png->pvrow[i];
      } break;

    case 3: if (png->pvrow) {
        for (i=0;i<png->xstride;i++) png->row[i]+=png->pvrow[i]>>1;
        for (;i<png->stride;i++) png->row[i]+=(png->row[i-png->xstride]+png->pvrow[i])>>1;
      } else {
        for (i=png->xstride;i<png->stride;i++) png->row[i]+=png->row[i-png->xstride]>>1;
      } break;

    case 4: if (png->pvrow) {
        for (i=0;i<png->xstride;i++) png->row[i]+=png->pvrow[i];
        for (;i<png->stride;i++) png->row[i]+=akpng_paeth(png->row[i-png->xstride],png->pvrow[i],png->pvrow[i-png->xstride]);
      } else { // Paeth at the first row is the same as SUB.
        for (;i<png->stride;i++) png->row[i]+=png->row[i-png->xstride];
      } break;

    default: return -1;
  }
  return 0;
}

/* IDAT.
 */

static int akpng_decode_idat(struct akpng *png,const uint8_t *src,int srcc) {
  if (!png->pixels) return -1; // IDAT before IHDR -- the only sequencing issue we actually care about.

  Z->next_in=(Bytef*)src;
  Z->avail_in=srcc;
  while (Z->avail_in&&(png->y<png->h)) {
  
    if (!Z->avail_out) {
      if (png->phase) {
        Z->next_out=(Bytef*)png->row;
        Z->avail_out=png->stride;
      } else {
        Z->next_out=(Bytef*)&png->rowfilter;
        Z->avail_out=1;
      }
    }

    int err=inflate(png->z,Z_NO_FLUSH);
    if (err<0) return -1;

    if (!Z->avail_out) {
      if (png->phase) {
        if (akpng_decode_row(png)<0) return -1;
        png->pvrow=png->row;
        png->row+=png->stride;
        png->y++;
        png->phase=0;
      } else {
        png->phase=1;
        if (err==Z_STREAM_END) { // Stream ended at filter byte. Must unfilter the all-zeroes row.
          if (akpng_decode_row(png)<0) return -1;
        }
      }
    }

    if (err==Z_STREAM_END) png->y=png->h; // Force finish.
  }

  return 0;
}

/* IHDR.
 */

static int akpng_decode_ihdr(struct akpng *png,const uint8_t *src,int srcc) {
  if (srcc<13) return -1;
  if (png->pixels) return -1; // Multiple IHDR.

  png->w=(src[0]<<24)|(src[1]<<16)|(src[2]<<8)|src[3];
  png->h=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  png->depth=src[8];
  png->colortype=src[9];
  png->compression=src[10];
  png->filter=src[11];
  png->interlace=src[12];

  // We go off-spec here by refusing interlaced images.
  // Interlacing would add quite a bit of complexity to the decoder.
  // And since we decode completely in one pass, there would never be a need for it.
  // It's anachronistic in the 21st century anyway. Fuck it.
  // On-spec, no seriously, is our refusal of dimensions >=2^31.
  // They're stored in 32 bits but the spec very sensibly defines it as a 31-bit integer.
  // (This is assuming that 'int' is 32 bits, which it pretty much always is).
  if ((png->w<1)||(png->h<1)) return -1;
  if (png->compression||png->filter||png->interlace) return -1;

  // Just a little bit off-spec, we allow pixel sizes at any factor or multiple of 8, up to 64.
  // This includes all of the formats mandated by the spec.
  // Also (off-spec? I'm not sure.) we forbid any dimensions which would overflow int.
  int chanc=akpng_channel_count(png->colortype);
  if (chanc<1) return -1;
  png->pixelsize=chanc*png->depth;
  switch (png->pixelsize) {
    case 1: case 2: case 4: case 8: case 16: case 24: case 32: case 40: case 48: case 56: case 64: break;
    default: return -1;
  }
  if (png->pixelsize<8) png->xstride=1;
  else png->xstride=png->pixelsize>>3;
  if (png->w>(INT_MAX-7)/png->pixelsize) return -1;
  png->stride=(png->w*png->pixelsize+7)>>3;
  if (png->stride>INT_MAX/png->h) return -1;

  // Allocate buffers.
  if (!(png->pixels=calloc(png->stride,png->h))) return -1;
  if (!(png->z=calloc(1,sizeof(z_stream)))) return -1;
  if (inflateInit(png->z)<0) return -1;
  png->row=png->pixels;
  png->pvrow=0;
  png->y=0;
  png->phase=0;
  
  return 0;
}

/* Verbatim chunks.
 */

static int akpng_append_chunk(struct akpng *png,const char *chunkid,const void *src,int srcc) {
  if (png->chunkc>=png->chunka) {
    int na=png->chunka+4;
    if (na>INT_MAX/sizeof(struct akpng_chunk)) return -1;
    void *nv=realloc(png->chunkv,sizeof(struct akpng_chunk)*na);
    if (!nv) return -1;
    png->chunkv=nv;
    png->chunka=na;
  }
  struct akpng_chunk *chunk=png->chunkv+png->chunkc;
  memcpy(chunk->chunkid,chunkid,4);
  if (srcc>0) {
    if (!(chunk->v=malloc(srcc))) return -1;
    memcpy(chunk->v,src,srcc);
    chunk->c=srcc;
  } else {
    chunk->v=0;
    chunk->c=0;
  }
  png->chunkc++;
  return 0;
}

static int akpng_decode_plte(struct akpng *png,const void *src,int srcc) {
  if (png->preservation&AKPNG_PRESERVE_NO_PLTE) return 0;
  return akpng_append_chunk(png,"PLTE",src,srcc);
}

static int akpng_decode_trns(struct akpng *png,const void *src,int srcc) {
  if (png->preservation&AKPNG_PRESERVE_NO_TRNS) return 0;
  return akpng_append_chunk(png,"tRNS",src,srcc);
}

static int akpng_decode_other(struct akpng *png,const void *src,int srcc,const char *chunkid) {
  if (!(png->preservation&AKPNG_PRESERVE_OTHER)) return 0;
  return akpng_append_chunk(png,chunkid,src,srcc);
}

/* Finish decoding.
 */

static int akpng_decode_finish(struct akpng *png) {
  if (!png->pixels||(png->w<1)||(png->h<1)) return -1;
  if (png->y<png->h) return -1;
  if (png->z) { inflateEnd(png->z); free(png->z); png->z=0; }
  return 0;
}

/* Decode, main entry point.
 */
 
int akpng_decode(struct akpng *png,const void *src,int srcc) {
  if (!png||!src||(srcc<8)) return -1;
  if (memcmp(src,"\x89PNG\r\n\x1a\n",8)) return -1;

  // Reset decoder. These should all be zero anyway, but let's be sure.
  if (png->pixels) { free(png->pixels); png->pixels=0; }
  png->w=png->h=0;
  if (png->z) { inflateEnd(png->z); free(png->z); png->z=0; }
  png->row=0;
  png->pvrow=0;
  png->y=0;

  // Decode chunks.
  const uint8_t *SRC=src;
  int srcp=8;
  while (srcp<srcc-8) {
    int chunklen=(SRC[srcp]<<24)|(SRC[srcp+1]<<16)|(SRC[srcp+2]<<8)|SRC[srcp+3];
    if (chunklen<0) return -1;
    const char *chunkid=SRC+srcp+4;
    srcp+=8;
    if (srcp>srcc-chunklen) return -1;
         if (!memcmp(chunkid,"IEND",4)) break;
    else if (!memcmp(chunkid,"IHDR",4)) { if (akpng_decode_ihdr(png,SRC+srcp,chunklen)<0) return -1; }
    else if (!memcmp(chunkid,"IDAT",4)) { if (akpng_decode_idat(png,SRC+srcp,chunklen)<0) return -1; }
    else if (!memcmp(chunkid,"PLTE",4)) { if (akpng_decode_plte(png,SRC+srcp,chunklen)<0) return -1; }
    else if (!memcmp(chunkid,"tRNS",4)) { if (akpng_decode_trns(png,SRC+srcp,chunklen)<0) return -1; }
    else if (akpng_decode_other(png,SRC+srcp,chunklen,chunkid)<0) return -1;
    srcp+=chunklen;
    if (srcp>srcc-4) return -1;
    srcp+=4; // Got your CRC check right here, buddy.
  }
  
  if (akpng_decode_finish(png)<0) return -1;
  return 0;
}

/* Decode entire file.
 */
 
#if WIN32
  #define OPENMODE O_RDONLY|O_BINARY
#else
  #define OPENMODE O_RDONLY
#endif

int akpng_decode_file(struct akpng *png,const char *path) {
  if (!png||!path) return -1;
  int fd=open(path,OPENMODE);
  if (fd<0) return -1;
  off_t flen=lseek(fd,0,SEEK_END);
  if (flen<0) { close(fd); return -1; }
  if ((flen>INT_MAX)||lseek(fd,0,SEEK_SET)) { close(fd); return -1; }
  unsigned char *dst=malloc(flen);
  if (!dst) { close(fd); return -1; }
  int dstc=0,err;
  while (dstc<flen) {
    if ((err=read(fd,dst+dstc,flen-dstc))<=0) { close(fd); return -1; }
    dstc+=err;
  }
  close(fd);
  err=akpng_decode(png,dst,dstc);
  free(dst);
  return err;
}

/* Convert to RGB or RGBA.
 */

static uint32_t akpng_read_literal(const uint8_t *src,int x,uint8_t depth) {
  switch (depth) {
    case 1: return (src[x>>3]&(0x80>>(x&7)))?1:0;
    case 2: switch (x&3) {
        case 0: return src[x>>2]>>6;
        case 1: return (src[x>>2]>>4)&3;
        case 2: return (src[x>>2]>>2)&3;
        case 3: return src[x>>2]&3;
      }
    case 4: if (x&1) return src[x>>1]&15; return src[x>>1]>>4;
    case 8: return src[x];
    case 16: x<<=1; return (src[x]<<8)|src[x+1];
    case 24: x*=3; return (src[x]<<16)|(src[x+1]<<8)|src[x+2];
    case 32: x<<=2; return (src[x]<<24)|(src[x+1]<<16)|(src[x+2]<<8)|src[x+3];
    case 40: x*=5; x+=1; return (src[x]<<24)|(src[x+1]<<16)|(src[x+2]<<8)|src[x+3];
    case 48: x*=6; x+=2; return (src[x]<<24)|(src[x+1]<<16)|(src[x+2]<<8)|src[x+3];
    case 56: x*=7; x+=3; return (src[x]<<24)|(src[x+1]<<16)|(src[x+2]<<8)|src[x+3];
    case 64: x<<=3; x+=4; return (src[x]<<24)|(src[x+1]<<16)|(src[x+2]<<8)|src[x+3];
  }
  return 0;
}

static uint32_t akpng_read_rgba(const uint8_t *src,int x,uint8_t depth,uint8_t colortype) {
  switch (colortype) {
    case 3:
    case 0: switch (depth) {
        case 1: return (src[x>>3]&(0x80>>(x&7)))?0xffffffff:0x000000ff;
        case 2: {
            uint8_t y=src[x>>2];
            switch (x&3) {
              case 0: y&=0xc0; y|=y>>2; y|=y>>4; break;
              case 1: y&=0x30; y|=y<<2; y|=y>>4; break;
              case 2: y&=0x0c; y|=y>>2; y|=y<<4; break;
              case 3: y&=0x03; y|=y<<2; y|=y<<4; break;
            }
            return (y<<24)|(y<<16)|(y<<8)|0xff;
          }
        case 4: {
            uint8_t y=src[x>>1];
            if (x&1) y=(y&0xf0)|(y>>4); else y=(y&0x0f)|(y<<4);
            return (y<<24)|(y<<16)|(y<<8)|0xff;
          }
        case 8: return (src[x]<<24)|(src[x]<<16)|(src[x]<<8)|0xff;
        case 16: x<<=1; return (src[x]<<24)|(src[x]<<16)|(src[x]<<8)|0xff;
        case 24: x*=3; return (src[x]<<24)|(src[x]<<16)|(src[x]<<8)|0xff;
        case 32: x<<=2; return (src[x]<<24)|(src[x]<<16)|(src[x]<<8)|0xff;
        case 40: x*=5; return (src[x]<<24)|(src[x]<<16)|(src[x]<<8)|0xff;
        case 48: x*=6; return (src[x]<<24)|(src[x]<<16)|(src[x]<<8)|0xff;
        case 56: x*=7; return (src[x]<<24)|(src[x]<<16)|(src[x]<<8)|0xff;
        case 64: x<<=3; return (src[x]<<24)|(src[x]<<16)|(src[x]<<8)|0xff;
      } break;
    case 2: switch (depth) {
        case 8: x*=3; return (src[x]<<24)|(src[x+1]<<16)|(src[x+2]<<8)|0xff;
        case 16: x*=6; return (src[x]<<24)|(src[x+2]<<16)|(src[x+4]<<8)|0xff;
      } break;
    case 4: switch (depth) {
        case 1: {
            uint8_t y=src[x>>2],a;
            switch (x&3) {
              case 0: a=(y&0x40)?0xff:0x00; y=(y&0x80)?0xff:0x00; break;
              case 1: a=(y&0x10)?0xff:0x00; y=(y&0x20)?0xff:0x00; break;
              case 2: a=(y&0x04)?0xff:0x00; y=(y&0x08)?0xff:0x00; break;
              case 3: a=(y&0x01)?0xff:0x00; y=(y&0x02)?0xff:0x00; break;
            }
            return (y<<24)|(y<<16)|(y<<8)|a;
          }
        case 2: {
            uint8_t y=src[x>>1],a;
            if (x&1) { a=y&0x03; a|=a<<2; a|=a<<4; y&=0x0c; y|=y>>2; y|=y<<4; }
            else { a=y&0x30; a|=a<<2; a|=a>>4; y&=0xc0; y|=y>>2; y|=y>>4; }
            return (y<<24)|(y<<16)|(y<<8)|a;
          }
        case 4: {
            uint8_t y=src[x];
            uint8_t a=y&0x0f; a|=a<<4;
            y&=0xf0; y|=y>>4;
            return (y<<24)|(y<<16)|(y<<8)|a;
          }
        case 8: x<<=1; return (src[x]<<24)|(src[x]<<16)|(src[x]<<8)|src[x+1];
        case 16: x<<=2; return (src[x]<<24)|(src[x]<<16)|(src[x]<<8)|src[x+2];
        case 24: x*=6; return (src[x]<<24)|(src[x]<<16)|(src[x]<<8)|src[x+3];
        case 32: x<<=3; return (src[x]<<24)|(src[x]<<16)|(src[x]<<8)|src[x+4];
      } break;
    case 6: switch (depth) {
        case 1: if (x&1) {
            x>>=1; return ((src[x]&0x08)?0xff000000:0)|((src[x]&0x04)?0x00ff0000:0)|((src[x]&0x02)?0x0000ff00:0)|((src[x]&0x01)?0x000000ff:0);
          } else {
            x>>=1; return ((src[x]&0x80)?0xff000000:0)|((src[x]&0x40)?0x00ff0000:0)|((src[x]&0x20)?0x0000ff00:0)|((src[x]&0x10)?0x000000ff:0);
          }
        case 2: {
            uint8_t r=src[x]&0xc0; r|=r>>2; r|=r>>4;
            uint8_t g=src[x]&0x30; g|=g<<2; g|=g>>4;
            uint8_t b=src[x]&0x0c; b|=b>>2; b|=b<<4;
            uint8_t a=src[x]&0x03; a|=a<<2; a|=b<<4;
            return (r<<24)|(g<<16)|(b<<8)|a;
          }
        case 4: {
            x<<=1;
            uint8_t r=src[x]&0xf0; r|=r>>4;
            uint8_t g=src[x]&0x0f; g|=g<<4;
            uint8_t b=src[x+1]&0xf0; b|=b>>4;
            uint8_t a=src[x+1]&0x0f; a|=a<<4;
            return (r<<24)|(g<<16)|(b<<8)|a;
          }
        case 8: x<<=2; return (src[x]<<24)|(src[x+1]<<16)|(src[x+2]<<8)|src[x+3];
        case 16: x<<=3; return (src[x]<<24)|(src[x+2]<<16)|(src[x+4]<<8)|src[x+6];
      } break;
  }
  return 0; 
}
 
int akpng_force_rgb(struct akpng *png) {
  if (!png) return -1;
  if (!png->pixels||(png->w<1)||(png->h<1)) return 0;
  if ((png->depth==8)&&(png->colortype==2)) return 0;
  if (png->w>INT_MAX/3) return -1;
  int dststride=png->w*3;
  if (dststride>INT_MAX/png->h) return -1;
  uint8_t *dst=malloc(dststride*png->h);
  if (!dst) return -1;
  uint8_t *DST=dst;
  const uint8_t *SRC=png->pixels;

  /* If a palette is present and colortype is 3, use it. */
  if (png->colortype==3) {
    const uint8_t *plte=0;
    int pltec=0,i;
    for (i=0;i<png->chunkc;i++) if (!memcmp(png->chunkv[i].chunkid,"PLTE",4)) {
      plte=png->chunkv[i].v;
      pltec=png->chunkv[i].c/3;
      break;
    }
    if (plte) {
      int y; for (y=0;y<png->h;y++) {
        int dstp,x;
        for (x=dstp=0;x<png->w;x++) {
          uint32_t index=akpng_read_literal(SRC,x,png->depth);
          if (index>=pltec) {
            DST[dstp++]=0x00;
            DST[dstp++]=0x00;
            DST[dstp++]=0x00;
          } else {
            index*=3;
            DST[dstp++]=plte[index+0];
            DST[dstp++]=plte[index+1];
            DST[dstp++]=plte[index+2];
          }
        }
        DST+=dststride;
        SRC+=png->stride;
      }
      goto _ready_;
    }
  }

  /* From 48-bit RGB. */
  if ((png->depth==16)&&(png->colortype==2)) {
    int pixelc=png->w*png->h;
    while (pixelc-->0) {
      *DST=*SRC; DST++; SRC+=2;
      *DST=*SRC; DST++; SRC+=2;
      *DST=*SRC; DST++; SRC+=2;
    }

  /* From RGBA. */
  } else if ((png->depth==8)&&(png->colortype==6)) {
    int pixelc=png->w*png->h;
    while (pixelc-->0) {
      *DST=*SRC; DST++; SRC++;
      *DST=*SRC; DST++; SRC++;
      *DST=*SRC; DST++; SRC+=2;
    }

  /* From 64-bit RGBA. */
  } else if ((png->depth==16)&&(png->colortype==6)) {
    int pixelc=png->w*png->h;
    while (pixelc-->0) {
      *DST=*SRC; DST++; SRC+=2;
      *DST=*SRC; DST++; SRC+=2;
      *DST=*SRC; DST++; SRC+=4;
    }

  /* General case. */
  } else {
    int y; for (y=0;y<png->h;y++) {
      int dstp,x;
      for (x=dstp=0;x<png->w;x++) {
        uint32_t rgba=akpng_read_rgba(SRC,x,png->depth,png->colortype);
        DST[dstp++]=rgba>>24;
        DST[dstp++]=rgba>>16;
        DST[dstp++]=rgba>>8;
      }
      DST+=dststride;
      SRC+=png->stride;
    }
  }

 _ready_:
  free(png->pixels);
  png->pixels=dst;
  png->depth=8;
  png->colortype=2;
  png->stride=dststride;
  return 0;
}

int akpng_force_rgba(struct akpng *png) {
  if (!png) return -1;
  if (!png->pixels||(png->w<1)||(png->h<1)) return 0;
  if ((png->depth==8)&&(png->colortype==6)) return 0;
  if (png->w>INT_MAX>>2) return -1;
  int dststride=png->w<<2;
  if (dststride>INT_MAX/png->h) return -1;
  uint8_t *dst=malloc(dststride*png->h);
  if (!dst) return -1;
  uint8_t *DST=dst;
  const uint8_t *SRC=png->pixels;

  /* If a palette is present and colortype is 3, use it. */
  if (png->colortype==3) {
    const uint8_t *plte=0;
    int pltec=0,i;
    for (i=0;i<png->chunkc;i++) if (!memcmp(png->chunkv[i].chunkid,"PLTE",4)) {
      plte=png->chunkv[i].v;
      pltec=png->chunkv[i].c/3;
      break;
    }
    if (plte) {
      int y; for (y=0;y<png->h;y++) {
        int dstp,x;
        for (x=dstp=0;x<png->w;x++) {
          uint32_t index=akpng_read_literal(SRC,x,png->depth);
          if (index>=pltec) {
            DST[dstp++]=0x00;
            DST[dstp++]=0x00;
            DST[dstp++]=0x00;
            DST[dstp++]=0x00;
          } else {
            index*=3;
            DST[dstp++]=plte[index+0];
            DST[dstp++]=plte[index+1];
            DST[dstp++]=plte[index+2];
            DST[dstp++]=0xff;
          }
        }
        DST+=dststride;
        SRC+=png->stride;
      }
      goto _ready_;
    }
  }

  /* From RGB. */
  if ((png->depth==8)&&(png->colortype==2)) {
    int pixelc=png->w*png->h;
    while (pixelc-->0) {
      *DST=*SRC; DST++; SRC++;
      *DST=*SRC; DST++; SRC++;
      *DST=*SRC; DST++; SRC++;
      *DST=0xff; DST++;
    }

  /* General case. */
  } else {
    int y; for (y=0;y<png->h;y++) {
      int dstp,x;
      for (x=dstp=0;x<png->w;x++) {
        uint32_t rgba=akpng_read_rgba(SRC,x,png->depth,png->colortype);
        DST[dstp++]=rgba>>24;
        DST[dstp++]=rgba>>16;
        DST[dstp++]=rgba>>8;
        DST[dstp++]=rgba;
      }
      DST+=dststride;
      SRC+=png->stride;
    }
  }

 _ready_:
  free(png->pixels);
  png->pixels=dst;
  png->depth=8;
  png->colortype=6;
  png->stride=dststride;
  return 0;
}
