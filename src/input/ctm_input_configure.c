#include "ctm_input_internal.h"

/* Little text helpers.
 */

static int ctm_memcasecmp(const void *a,const void *b,int c) {
  if (a==b) return 0;
  if (c<1) return 0;
  if (!a) return -1;
  if (!b) return 1;
  while (c--) {
    uint8_t cha=*(uint8_t*)a; a=(char*)a+1; if ((cha>=0x41)&&(cha<=0x5a)) cha+=0x20;
    uint8_t chb=*(uint8_t*)b; b=(char*)b+1; if ((chb>=0x41)&&(chb<=0x5a)) chb+=0x20;
    if (cha<chb) return -1;
    if (cha>chb) return 1;
  }
  return 0;
}

static inline int ctm_hexdigit_eval(char src) {
  if ((src>='0')&&(src<='9')) return src-'0';
  if ((src>='a')&&(src<='f')) return src-'a'+10;
  if ((src>='A')&&(src<='F')) return src-'A'+10;
  return -1;
}

static int ctm_int_eval(int *dst,const char *src,int srcc) {
  if (!dst||!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int positive=1,srcp=0;
  if (src[srcp]=='-') { positive=0; if (++srcp>=srcc) return -1; }
  else if (src[srcp]=='+') { if (++srcp>=srcc) return -1; }
  if ((srcp<=srcc-3)&&(src[srcp]=='0')&&((src[srcp+1]=='x')||(src[srcp+1]=='X'))) {
    srcp+=2;
    *dst=0;
    while (srcp<srcc) {
      int digit=ctm_hexdigit_eval(src[srcp++]);
      if (digit<0) return -1;
      if (*dst&~(UINT_MAX>>4)) return -1;
      *dst<<=4; *dst|=digit;
    }
    if (!positive) *dst=-*dst;
  } else {
    *dst=0;
    while (srcp<srcc) {
      int digit=src[srcp++]-'0';
      if ((digit<0)||(digit>9)) return -1;
      if (positive) {
        if (*dst>INT_MAX/10) return -1; *dst*=10;
        if (*dst>INT_MAX-digit) return -1; *dst+=digit;
      } else {
        if (*dst<INT_MIN/10) return -1; *dst*=10;
        if (*dst<INT_MIN+digit) return -1; *dst-=digit;
      }
    }
  }
  return 0;
}

/* "INPUTID" or "QUALIFIER.INPUTID"
 */

static int ctm_input_parse_inputid(struct ctm_input_field *fld,const char *src,int srcc) {
  int i,dotp=-1;
  for (i=0;i<srcc;i++) if (src[i]=='.') {
    if (ctm_int_eval(&fld->srcscope,src,i)<0) return -1;
    if (ctm_int_eval(&fld->srcbtnid,src+i+1,srcc-i-1)<0) return -1;
    return 0;
  }
  fld->srcscope=0;
  return ctm_int_eval(&fld->srcbtnid,src,srcc);
}

/* Evaluate output button name.
 */

static int ctm_output_eval(uint16_t *dst,const char *src,int srcc) {
  // Users should normally provide a symbolic name:
  switch (srcc) {
    #define _(tag) if (!ctm_memcasecmp(src,#tag,srcc)) { *dst=CTM_BTNID_##tag; return 0; }
    case 2: {
        _(UP)
      } break;
    case 4: {
        _(DOWN)
        _(LEFT)
        _(QUIT)
      } break;
    case 5: {
        _(RIGHT)
        _(PAUSE)
        _(RESET)
      } break;
    case 6: {
	_(UNUSED)
      } break;
    case 7: {
        _(PRIMARY)
      } break;
    case 8: {
        _(TERTIARY)
      } break;
    case 9: {
        _(SECONDARY)
      } break;
    case 10: {
        _(SCREENSHOT)
        _(FULLSCREEN)
      } break;
    #undef _
  }
  // But they can use numbers too. Why? Who the fuck knows.
  int n=0;
  if (ctm_int_eval(&n,src,srcc)<0) return -1;
  if (n&~0xffff) return -1;
  *dst=n;
  if (n&CTM_BTNID_SIGNAL) return 0; // Any value OK in the SIGNAL domain.
  // It's not a signal, so it must have exactly one bit set.
  if (!n) return -1;
  while (!(n&1)) n>>=1;
  if (n!=1) return -1;
  return 0;
}

/* Add line to device definition.
 */

static int ctm_input_configure_line(struct ctm_input_definition *def,const char *src,int srcc,const char *refname,int lineno) {
  #define FAIL(fmt,...) { if (refname) fprintf(stderr,"%s:%d: "fmt"\n",refname,lineno,##__VA_ARGS__); return -1; }
  
  // Line may contain up to five space-delimited words: [QUALIFIER.]INPUTID [OUTPUTLOW] OUTPUTHIGH [@THRESHHOLD] [+BIAS]
  const char *wordv[5]={0};
  int lenv[5]={0};
  int wordc=0;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (wordc>=5) FAIL("Too many words on line.")
    wordv[wordc]=src+srcp;
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; lenv[wordc]++; }
    wordc++;
  }
  if (!wordc) return 0;

  struct ctm_input_field fld={0};
  int wordp=0;

  // First word is "INPUTID" or "QUALIFIER.INPUTID".
  if (wordp>=wordc) FAIL("Expected source button name.")
  if (ctm_input_parse_inputid(&fld,wordv[wordp],lenv[wordp])<0) FAIL("Failed to parse input button '%.*s'.",lenv[wordp],wordv[wordp])
  wordp++;

  // Next, we have "@THRESHHOLD" "+BIAS" "-BIAS" or naked output names.
  int outputc=0,have_threshhold=0,have_bias=0;
  while (wordp<wordc) {
    switch (wordv[wordp][0]) {
      case '@': {
          if (have_threshhold) FAIL("Multiple threshholds.")
          if (ctm_int_eval(&fld.threshhold,wordv[wordp]+1,lenv[wordp]-1)<0) FAIL("Failed to parse threshhold '%.*s'.",lenv[wordp],wordv[wordp])
          have_threshhold=1;
        } break;
      case '+': case '-': {
          if (have_bias) goto _output_;
          if (ctm_int_eval(&fld.bias,wordv[wordp],lenv[wordp])<0) FAIL("Failed to parse bias '%.*s'.",lenv[wordp],wordv[wordp])
          have_bias=1;
        } break;
      default: _output_: switch (outputc++) {
          case 0: if (ctm_output_eval(&fld.dstbtnidhi,wordv[wordp],lenv[wordp])<0) FAIL("Failed to parse output button '%.*s'.",lenv[wordp],wordv[wordp]) break;
          case 1: {
              fld.dstbtnidlo=fld.dstbtnidhi;
              if (ctm_output_eval(&fld.dstbtnidhi,wordv[wordp],lenv[wordp])<0) FAIL("Failed to parse output button '%.*s'.",lenv[wordp],wordv[wordp])
            } break;
          default: FAIL("Unexpected value '%.*s'.",lenv[wordp],wordv[wordp])
        } break;
    }
    wordp++;
  }
  if (!outputc) FAIL("Expected output button name.")

  if (ctm_input_definition_add_field(def,&fld)<0) return -1;
  #undef FAIL
  return 0;
}

/* Configure, main entry point.
 */

int ctm_input_configure(const char *src,int srcc,const char *refname) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,lineno=1;
  struct ctm_input_definition *def=0; // If set, we are in the middle of a definition block.
  while (srcp<srcc) {

    // Newlines and leading whitespace.
    if ((src[srcp]==0x0a)||(src[srcp]==0x0d)) {
      lineno++;
      if (++srcp>=srcc) break;
      if (((src[srcp]==0x0a)||(src[srcp]==0x0d))&&(src[srcp]!=src[srcp-1])) srcp++;
      continue;
    }
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }

    // Measure line, strip comments.
    const char *line=src+srcp;
    int linec=0,cmt=0;
    while ((srcp<srcc)&&(src[srcp]!=0x0a)&&(src[srcp]!=0x0d)) {
      if (src[srcp]=='#') cmt=1;
      if (!cmt) linec++;
      srcp++;
    }
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    if (!linec) continue;

    // Begin block, end block, or add line to block.
    if (def) {
      if ((linec==3)&&!ctm_memcasecmp(line,"end",3)) {
        def=0;
      } else {
        if (ctm_input_configure_line(def,line,linec,refname,lineno)<0) return -1;
      }
    } else {
      if (!(def=ctm_input_add_definition(line,linec))) return -1;
    }
  }
  return 0;
}
