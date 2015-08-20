#include "ctm.h"
#include "ctm_grid.h"
#include "io/ctm_fs.h"

struct ctm_grid ctm_grid={0};
uint32_t ctm_tileprop_interior[256]={0};
uint32_t ctm_tileprop_exterior[256]={0};

/* Rebuild ctm_grid.fountains.
 */

static void ctm_grid_locate_fountains() {
  int fountainc=0,col,row;
  const struct ctm_grid_cell *cell=ctm_grid.cellv;
  for (row=0;row<ctm_grid.rowc;row++) {
    for (col=0;col<ctm_grid.colc;col++,cell++) {
      uint32_t prop=ctm_tileprop_interior[cell->itile];
      prop|=ctm_tileprop_exterior[cell->tile];
      if (prop&CTM_TILEPROP_YOUTH) {
        ctm_grid.fountains[fountainc].x=col;
        ctm_grid.fountains[fountainc].y=row;
        if (++fountainc>=CTM_FOUNTAINS_LIMIT) return;
      }
    }
  }
  for (;fountainc<CTM_FOUNTAINS_LIMIT;fountainc++) {
    ctm_grid.fountains[fountainc].x=-1;
    ctm_grid.fountains[fountainc].y=-1;
  }
}

/* Decode.
 * SERIAL FORMAT
 *   0000   2 Width.
 *   0002   2 Height.
 *   0006 ... Cells, 2 bytes each.
 */

static int ctm_grid_decode(const uint8_t *src,int srcc) {
  int srcp=0,i;
  
  if (srcp>srcc-4) return -1;
  ctm_grid.colc=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
  ctm_grid.rowc=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
  if (!ctm_grid.colc||!ctm_grid.rowc||(ctm_grid.colc>INT_MAX/ctm_grid.rowc)) return -1;

  int cellc=ctm_grid.colc*ctm_grid.rowc;
  if (cellc>INT_MAX/sizeof(struct ctm_grid_cell)) return -1;
  if (srcp>srcc-(cellc<<1)) return -1;
  if (!(ctm_grid.cellv=calloc(sizeof(struct ctm_grid_cell),ctm_grid.colc*ctm_grid.rowc))) return -1;
  memcpy(ctm_grid.cellv,src+srcp,cellc<<1);

  return 0;
}

/* Encode.
 */

static int ctm_grid_encode(void *dstpp) {
  if (!dstpp) return -1;
  int cellc=ctm_grid.colc*ctm_grid.rowc;
  int dstc=4+(cellc*sizeof(struct ctm_grid_cell));
  uint8_t *dst=malloc(dstc);
  if (!dst) return -1;

  dst[0]=ctm_grid.colc>>8; dst[1]=ctm_grid.colc;
  dst[2]=ctm_grid.rowc>>8; dst[3]=ctm_grid.rowc;
  memcpy(dst+4,ctm_grid.cellv,sizeof(struct ctm_grid_cell)*cellc);

  *(void**)dstpp=dst;
  return dstc;
}

int ctm_grid_save() {
  void *src=0;
  int srcc=ctm_grid_encode(&src);
  if ((srcc<0)||!src) return -1;
  int err=ctm_file_write(ctm_data_path("grid"),src,srcc);
  free(src);
  return err;
}

/* Load tileprops from file.
 */

static int ctm_tileprop_load(uint32_t *dst,const char *path) {
  uint8_t *src=0;
  int srcc;
  if ((srcc=ctm_file_read(&src,path))<0) return -1;
  if (srcc<1024) { free(src); return -1; }
  memcpy(dst,src,1024);
  free(src);
  #if BYTE_ORDER==LITTLE_ENDIAN
    int i; for (i=0;i<256;i++) {
      uint32_t n=dst[i];
      dst[i]=(n>>24)|((n&0x00ff0000)>>8)|((n&0x0000ff00)<<8)|(n<<24);
    }
  #endif
  return 0;
}

/* Init.
 */

int ctm_grid_init() {
  memset(&ctm_grid,0,sizeof(struct ctm_grid));

  if (sizeof(struct ctm_grid_cell)!=2) {
    printf("BAD GRID UNIT SIZE: %d\n",(int)sizeof(struct ctm_grid_cell));
    return -1;
  }

  void *src=0;
  int srcc=ctm_file_read(&src,ctm_data_path("grid"));
  if ((srcc<0)||!src) return -1;
  if (ctm_grid_decode(src,srcc)<0) { free(src); return -1; }
  free(src);

  if (ctm_tileprop_load(ctm_tileprop_exterior,ctm_data_path("exterior.tileprop"))<0) return -1;
  if (ctm_tileprop_load(ctm_tileprop_interior,ctm_data_path("interior.tileprop"))<0) return -1;

  ctm_grid_locate_fountains();
  
  return 0;
}

/* Quit.
 */

void ctm_grid_quit() {
  if (ctm_grid.cellv) free(ctm_grid.cellv);
  memset(&ctm_grid,0,sizeof(struct ctm_grid));
}

/* Get tileprops for a cell, with defaults and everything.
 */

static uint32_t ctm_tileprop_oob=CTM_TILEPROP_SOLID;
 
uint32_t ctm_grid_tileprop_for_cell(int col,int row,int interior) {
  if ((col<0)||(row<0)||(col>=ctm_grid.colc)||(row>=ctm_grid.rowc)) return ctm_tileprop_oob;
  if (interior) return ctm_tileprop_interior[ctm_grid.cellv[row*ctm_grid.colc+col].itile];
  return ctm_tileprop_exterior[ctm_grid.cellv[row*ctm_grid.colc+col].tile];
}

uint32_t ctm_grid_tileprop_for_pixel(int x,int y,int interior) {
  if ((x<0)||(y<0)) return ctm_tileprop_oob;
  return ctm_grid_tileprop_for_cell(x/CTM_TILESIZE,y/CTM_TILESIZE,interior);
}

uint32_t ctm_grid_tileprop_for_rect(int x,int y,int w,int h,int interior) {
  uint32_t prop=0;
  if (x<0) prop|=ctm_tileprop_oob;
  int cola=x/CTM_TILESIZE; if (cola<0) cola=0;
  int colz=(x+w-1)/CTM_TILESIZE; if (colz>=ctm_grid.colc) { colz=ctm_grid.colc-1; prop|=ctm_tileprop_oob; }
  if (y<0) prop|=ctm_tileprop_oob;
  int rowa=y/CTM_TILESIZE; if (rowa<0) rowa=0;
  int rowz=(y+h-1)/CTM_TILESIZE; if (rowz>=ctm_grid.rowc) { rowz=ctm_grid.rowc-1; prop|=ctm_tileprop_oob; }
  if (interior) {
    int row; for (row=rowa;row<=rowz;row++) {
      int col; for (col=cola;col<=colz;col++) {
        prop|=ctm_tileprop_interior[ctm_grid.cellv[row*ctm_grid.colc+col].itile];
      }
    }
  } else {
    int row; for (row=rowa;row<=rowz;row++) {
      int col; for (col=cola;col<=colz;col++) {
        prop|=ctm_tileprop_exterior[ctm_grid.cellv[row*ctm_grid.colc+col].tile];
      }
    }
  }
  return prop;
}

/* Get the nearest fountain of youth.
 */
 
int ctm_grid_nearest_fountain(int *fountainx,int *fountainy,int qx,int qy) {
  qx/=CTM_TILESIZE;
  qy/=CTM_TILESIZE;
  int x=0,y=0,distance=INT_MAX,i;
  for (i=0;i<CTM_FOUNTAINS_LIMIT;i++) {
    if (ctm_grid.fountains[i].x<0) break; // end of list
    int dx=qx-ctm_grid.fountains[i].x; if (dx<0) dx=-dx;
    int dy=qy-ctm_grid.fountains[i].y; if (dy<0) dy=-dy;
    int d=dx+dy; // Manhattan distance is adequate, I think.
    if (d<distance) { distance=d; x=ctm_grid.fountains[i].x; y=ctm_grid.fountains[i].y; }
  }
  if (distance==INT_MAX) return 0;
  if (fountainx) *fountainx=x*CTM_TILESIZE+(CTM_TILESIZE>>1);
  if (fountainy) *fountainy=y*CTM_TILESIZE+(CTM_TILESIZE>>1);
  return 1;
}
