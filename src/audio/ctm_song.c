#include "ctm_audio_internal.h"
#include <stdarg.h>
#include <math.h>

/* Allocation.
 */

struct ctm_song *ctm_song_new() {
  struct ctm_song *song=calloc(1,sizeof(struct ctm_song));
  if (!song) return 0;
  song->tempo=10000;
  song->drumlevel=0x40;
  song->synthlevel=0x40;
  return song;
}

void ctm_song_del(struct ctm_song *song) {
  if (!song) return;
  if (song->synthchanv) free(song->synthchanv);
  if (song->drumchanv) free(song->drumchanv);
  if (song->synthv) {
    while (song->synthc-->0) if (song->synthv[song->synthc].samplev) free(song->synthv[song->synthc].samplev);
    free(song->synthv);
  }
  if (song->cmdv) free(song->cmdv);
  free(song);
}

/* Reset.
 */

int ctm_song_reset(struct ctm_song *song) {
  if (!song) return -1;
  song->delay=0;
  song->cmdp=0;
  int i;
  for (i=0;i<song->synthchanc;i++) song->synthchanv[i].samplev=0;
  for (i=0;i<song->drumchanc;i++) song->drumchanv[i].samplev=0;
  return 0;
}

/* Frequency from musical note.
 */

static const double ctm_musical_intervals[12]={
  1.0,
  16.0/15.0,
  9.0/8.0,
  6.0/5.0,
  5.0/4.0,
  4.0/3.0,
  13.0/9.0,
  3.0/2.0,
  8.0/5.0,
  5.0/3.0,
  9.0/5.0,
  15.0/8.0,
};

static double ctm_song_freq_from_note(uint8_t note) {
  double freq=440.0;
  while (note<0x80) { freq*=0.5; note+=12; }
  while (note>=0x8c) { freq*=2.0; note-=12; }
  return freq*ctm_musical_intervals[note-0x80];
}

/* Execute command.
 */

static void ctm_song_command(struct ctm_song *song,const struct ctm_song_command *cmd) {
  switch (cmd->opcode) {
  
    case CTM_SONG_OPCODE_DELAY: {
        song->delay=(cmd->argv[0]<<16)|(cmd->argv[1]<<8)|cmd->argv[2];
      } break;

    case CTM_SONG_OPCODE_DRUM: {
        if (!cmd->argv[2]) return; // level
        int chix=cmd->argv[0];
        if (chix>=song->drumchanc) return;
        int fxp=ctm_audio_effect_search(cmd->argv[1]);
        if (fxp<0) return;
        struct ctm_audio_effect *fx=ctm_audio.fxv+fxp;
        struct ctm_audio_channel_verbatim *ch=song->drumchanv+chix;
        ch->samplev=fx->samplev;
        ch->samplec=fx->samplec;
        ch->samplep=0;
        ch->level=cmd->argv[2];
      } break;

    case CTM_SONG_OPCODE_SYNTH: {
        if (cmd->argv[0]>=song->synthchanc) return;
        if (cmd->argv[1]>=song->synthc) return;
        struct ctm_audio_channel_synth *ch=song->synthchanv+cmd->argv[0];
        struct ctm_audio_effect *synth=song->synthv+cmd->argv[1];
        if (synth->samplec<1) return;
        ch->samplev=synth->samplev;
        ch->freq=ctm_song_freq_from_note(cmd->argv[2]);
        ch->trim=0x00;
        ch->trimc=0;
        ch->freqc=0;
        ch->trimend=0;
      } break;

    case CTM_SONG_OPCODE_LEVEL: {
        if (cmd->argv[0]>=song->synthchanc) return;
        struct ctm_audio_channel_synth *ch=song->synthchanv+cmd->argv[0];
        if (cmd->argv[2]>0) {
          ch->trimsrc=ch->trim;
          ch->trimdst=cmd->argv[1];
          ch->trimp=0;
          ch->trimc=cmd->argv[2]*1000;
        } else {
          ch->trim=cmd->argv[1];
          ch->trimc=0;
        }
        ch->trimend=0;
      } break;

    case CTM_SONG_OPCODE_NOTE: {
        if (cmd->argv[0]>=song->synthchanc) return;
        struct ctm_audio_channel_synth *ch=song->synthchanv+cmd->argv[0];
        double freq=ctm_song_freq_from_note(cmd->argv[1]);
        if (cmd->argv[2]>0) {
          ch->freqsrc=ch->freq;
          ch->freqdst=freq;
          ch->freqp=0;
          ch->freqc=cmd->argv[2]*1000;
        } else {
          ch->freq=freq;
          ch->freqc=0;
        }
      } break;

    case CTM_SONG_OPCODE_QUIET: {
        if (cmd->argv[0]>=song->synthchanc) return;
        struct ctm_audio_channel_synth *ch=song->synthchanv+cmd->argv[0];
        ch->trimsrc=ch->trim;
        ch->trimdst=0;
        ch->trimp=0;
        ch->trimc=100;
        ch->trimend=1;
      } break;
      
  }
}

/* Update. Get next sample.
 */

int16_t ctm_song_update(struct ctm_song *song) {
  if (!song) return 0;
  int sample=0,i;

  /* Update synth channels. */
  for (i=0;i<song->synthchanc;i++) {
    struct ctm_audio_channel_synth *ch=song->synthchanv+i;
    if (!ch->samplev) continue;

    // Read the current sample. Just like verbatim channels.
    int samplep=(int)ch->samplep;
    if ((samplep<0)||(samplep>=CTM_AUDIO_SYNTH_SIZE)) samplep=0;
    if (ch->trim==0xff) sample+=ch->samplev[samplep];
    else sample+=(ch->samplev[samplep]*ch->trim)>>8;

    // Step sample position and wrap it around if it exceeds the buffer size.
    ch->samplep+=ch->freq;
    if (ch->samplep<0.0) ch->samplep=0.0;
    else while (ch->samplep>=CTM_AUDIO_SYNTH_SIZE) ch->samplep-=CTM_AUDIO_SYNTH_SIZE;

    // Update frequency slider.
    if (ch->freqc>0) {
      if ((ch->freqp<0)||(ch->freqp>=ch->freqc)) {
        ch->freq=ch->freqdst;
        ch->freqc=0;
      } else {
        ch->freq=ch->freqsrc+((ch->freqdst-ch->freqsrc)*ch->freqp)/ch->freqc;
        ch->freqp++;
      }
    }

    // Update trim slider.
    if (ch->trimc>0) {
      if ((ch->trimp<0)||(ch->trimp>=ch->trimc)) {
        ch->trim=ch->trimdst;
        ch->trimc=0;
        if (ch->trimend) ch->samplev=0;
      } else {
        ch->trim=ch->trimsrc+((ch->trimdst-ch->trimsrc)*ch->trimp)/ch->trimc;
        ch->trimp++;
      }
    }
  }

  /* Update drum channels. */
  for (i=0;i<song->drumchanc;i++) {
    struct ctm_audio_channel_verbatim *ch=song->drumchanv+i;
    if (!ch->samplev) continue;
    if (ch->level==0xff) sample+=ch->samplev[ch->samplep++];
    else sample+=(ch->samplev[ch->samplep++]*ch->level)>>8;
    if (ch->samplep>=ch->samplec) ch->samplev=0;
  }

  /* Advance command position or sleep counter. */
  if (song->delay>0) song->delay--;
  else if (song->cmdc>0) {
    if ((song->cmdp<0)||(song->cmdp>=song->cmdc)) song->cmdp=0;
    int cmdp0=song->cmdp;
    while (!song->delay) {
      ctm_song_command(song,song->cmdv+song->cmdp);
      if (++(song->cmdp)>=song->cmdc) song->cmdp=0;
      if (song->cmdp==cmdp0) {
        if (!song->delay) song->delay=INT_MAX; // Song has no delays. Panic a little bit.
      }
    }
  }

  /* Clip sample and we're out of here. */
  if (sample<-32768) return -32768;
  if (sample>32767) return 32767;
  return sample;
}

/* Text helpers.
 */

struct ctm_song_line {
  const char *src;
  int srcc;
  int lineno;
  int chid; // see synthid
  int synthid; // <0 for drum channels
  int level;
  int hint;
};

static int ctm_decuint_eval(const char *src,int srcc) {
  if (!src||(srcc<1)) return -1;
  int dst=0;
  int srcp=0; while (srcp<srcc) {
    int digit=src[srcp++]-'0';
    if ((digit<0)||(digit>9)) return -1;
    if (dst>INT_MAX/10) return -1; dst*=10;
    if (dst>INT_MAX-digit) return -1; dst+=digit;
  }
  return dst;
}

/* Allocate channel lists.
 */

static int ctm_song_build_drum_channels(struct ctm_song *song,int c) {
  if (!song||(c<0)||(c>256)) return -1;
  if (c>song->drumchanc) {
    void *nv=realloc(song->drumchanv,sizeof(struct ctm_audio_channel_verbatim)*c);
    if (!nv) return -1;
    song->drumchanv=nv;
    memset(song->drumchanv+song->drumchanc,0,sizeof(struct ctm_audio_channel_verbatim)*(c-song->drumchanc));
  }
  song->drumchanc=c;
  return 0;
}

static int ctm_song_build_synth_channels(struct ctm_song *song,int c) {
  if (!song||(c<0)||(c>256)) return -1;
  if (c>song->synthchanc) {
    void *nv=realloc(song->synthchanv,sizeof(struct ctm_audio_channel_synth)*c);
    if (!nv) return -1;
    song->synthchanv=nv;
    memset(song->synthchanv+song->synthchanc,0,sizeof(struct ctm_audio_channel_synth)*(c-song->synthchanc));
  }
  song->synthchanc=c;
  return 0;
}

/* Add global synthesizer scratch to our synth store.
 */

static int ctm_song_add_synth(struct ctm_song *song,int id) {
  if ((id<0)||(id>255)) return -1;
  if (id>=song->syntha) {
    int na=(id+16)&~15;
    void *nv=realloc(song->synthv,sizeof(struct ctm_audio_effect)*na);
    if (!nv) return -1;
    song->synthv=nv;
    song->syntha=na;
  }
  if (id>=song->synthc) {
    memset(song->synthv+song->synthc,0,sizeof(struct ctm_audio_effect)*(id-song->synthc+1));
    song->synthc=id+1;
  }
  if (song->synthv[id].samplev) return -1;
  if (!ctm_audio.synthscratch) return -1;
  if (!(song->synthv[id].samplev=malloc(CTM_AUDIO_SYNTH_SIZE<<1))) return -1;
  int16_t *dst=song->synthv[id].samplev;
  int i; for (i=0;i<CTM_AUDIO_SYNTH_SIZE;i++) {
    double sample=ctm_audio.synthscratch[i]*32767.0;
    if (sample>=32767.0) dst[i]=32767;
    else if (sample<=-32768.0) dst[i]=-32768;
    else dst[i]=sample;
  }
  song->synthv[id].samplec=CTM_AUDIO_SYNTH_SIZE;
  return 0;
}

/* Decode synthesizer definition.
 * Input is: ID COEFFICIENTS POSTPROCESSING
 * ID is one decimal integer, 0..255.
 * COEFFICIENTS are decimal integers for the level for successive harmonics, 100 = full size.
 * POSTPROCESSING are any combination of:
 *   "[LEVEL]" to clip at a given level.
 *   "(LEVEL)" to gain to a given level.
 * where LEVEL is a decimal integer, 100 = full size.
 * All synth processing is done in the floating-point domain, so samples can be out of range until the very end.
 */

static int ctm_song_decode_synth(struct ctm_song *song,const char *src,int srcc) {
  int i,srcp=0,id,tokc;

  // Read synth ID.
  tokc=0; while ((srcp+tokc<srcc)&&(src[srcp+tokc]>='0')&&(src[srcp+tokc]<='9')) tokc++;
  if ((id=ctm_decuint_eval(src+srcp,tokc))<0) return -1;
  if (id>255) return -1;
  srcp+=tokc; while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;

  // Create reference wave and scratch space if they don't exist yet.
  if (!ctm_audio.refwave) {
    if (!(ctm_audio.refwave=malloc(sizeof(double)*CTM_AUDIO_SYNTH_SIZE))) return -1;
    for (i=0;i<CTM_AUDIO_SYNTH_SIZE;i++) ctm_audio.refwave[i]=sin((i*M_PI*2.0)/CTM_AUDIO_SYNTH_SIZE);
  }
  if (!ctm_audio.synthscratch&&!(ctm_audio.synthscratch=malloc(sizeof(double)*CTM_AUDIO_SYNTH_SIZE))) return -1;
  for (i=0;i<CTM_AUDIO_SYNTH_SIZE;i++) ctm_audio.synthscratch[i]=0.0;

  // Read and apply harmonics.
  int step=1;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    tokc=0; while ((srcp+tokc<srcc)&&(src[srcp+tokc]>='0')&&(src[srcp+tokc]<='9')) tokc++;
    int level=ctm_decuint_eval(src+srcp,tokc);
    if (level<0) return -1;
    srcp+=tokc; while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    if (level) {
      double coef=level/100.0;
      int srcp=0;
      for (i=0;i<CTM_AUDIO_SYNTH_SIZE;i++,srcp+=step) {
        if (srcp>=CTM_AUDIO_SYNTH_SIZE) srcp-=CTM_AUDIO_SYNTH_SIZE;
        ctm_audio.synthscratch[i]+=ctm_audio.refwave[srcp]*coef;
      }
    }
    if (++step>=CTM_AUDIO_SYNTH_SIZE) return -1;
  }

  // Read and apply gain and clip instructions.
  while (srcp<srcc) {
    char term=0; switch (src[srcp++]) {
      case '(': term=')'; break;
      case '[': term=']'; break;
      default: return -1;
    }
    tokc=0; while ((srcp+tokc<srcc)&&(src[srcp+tokc]!=term)) tokc++;
    int level=ctm_decuint_eval(src+srcp,tokc);
    if (level<0) return -1;
    srcp+=tokc+1; while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    double flevel=level/100.0;
    switch (term) {
      case ')': if (level!=100) for (i=0;i<CTM_AUDIO_SYNTH_SIZE;i++) ctm_audio.synthscratch[i]*=flevel; break;
      case ']': for (i=0;i<CTM_AUDIO_SYNTH_SIZE;i++) {
          if (ctm_audio.synthscratch[i]>flevel) ctm_audio.synthscratch[i]=flevel;
          else if (ctm_audio.synthscratch[i]<-flevel) ctm_audio.synthscratch[i]=-flevel;
        } break;
    }
  }

  // Add to synth store.
  if (ctm_song_add_synth(song,id)<0) return -1;

  return 0;
}

/* Decode header line.
 * These are made of whitespace-delimited tokens.
 * First token is a keyword.
 */

static int ctm_song_decode_header(struct ctm_song *song,const char *src,int srcc,const char *refname,int lineno) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *kw=src+srcp; int kwc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; kwc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;

  if ((kwc==5)&&!memcmp(kw,"tempo",5)) {
    int n=ctm_decuint_eval(src+srcp,srcc-srcp);
    if (n<1) { fprintf(stderr,"%s:%d: expected positive integer\n",refname,lineno); return -1; }
    n=(44100*60)/n;
    if (n<1) { fprintf(stderr,"%s:%d: tempo too fast\n",refname,lineno); return -1; }
    song->tempo=n;

  } else if ((kwc==5)&&!memcmp(kw,"synth",5)) {
    if (ctm_song_decode_synth(song,src+srcp,srcc-srcp)<0) {
      fprintf(stderr,"%s:%d: failed to decode synth definition\n",refname,lineno);
      return -1;
    }

  } else if ((kwc==9)&&!memcmp(kw,"drumchanc",9)) {
    int n=ctm_decuint_eval(src+srcp,srcc-srcp);
    if (n<0) { fprintf(stderr,"%s:%d: expected integer\n",refname,lineno); return -1; }
    if (n>256) { fprintf(stderr,"%s:%d: too many drum channels, limit 256\n",refname,lineno); return -1; }
    if (ctm_song_build_drum_channels(song,n)<0) return -1;

  } else if ((kwc==10)&&!memcmp(kw,"synthchanc",10)) {
    int n=ctm_decuint_eval(src+srcp,srcc-srcp);
    if (n<0) { fprintf(stderr,"%s:%d: expected integer\n",refname,lineno); return -1; }
    if (n>256) { fprintf(stderr,"%s:%d: too many synth channels, limit 256\n",refname,lineno); return -1; }
    if (ctm_song_build_synth_channels(song,n)<0) return -1;

  } else {
    fprintf(stderr,"%s:%d: Unexpected header command '%.*s'.\n",refname,lineno,kwc,kw);
    return -1;
  }
  return 0;
}

/* Append command.
 */

static int ctm_song_append_command(struct ctm_song *song,uint8_t opcode,...) {

  uint8_t argv[3];
  va_list vargs;
  va_start(vargs,opcode);
  int i; for (i=0;i<3;i++) argv[i]=va_arg(vargs,int);

  // Try to merge DELAY commands.
  if ((opcode==CTM_SONG_OPCODE_DELAY)&&(song->cmdc>0)&&(song->cmdv[song->cmdc-1].opcode==CTM_SONG_OPCODE_DELAY)) {
    uint8_t *av=song->cmdv[song->cmdc-1].argv;
    int a=(av[0]<<16)|(av[1]<<8)|av[2];
    int b=(argv[0]<<16)|(argv[1]<<8)|argv[2];
    if (a<=0xffffff-b) {
      a+=b;
      av[0]=a>>16;
      av[1]=a>>8;
      av[2]=a;
      return 0;
    }
  }

  if (song->cmdc>=song->cmda) {
    int na=song->cmda+8;
    if (na>INT_MAX/sizeof(struct ctm_song_command)) return -1;
    void *nv=realloc(song->cmdv,sizeof(struct ctm_song_command)*na);
    if (!nv) return -1;
    song->cmdv=nv;
    song->cmda=na;
  }
    
  song->cmdv[song->cmdc].opcode=opcode;
  memcpy(song->cmdv[song->cmdc].argv,argv,3);
  song->cmdc++;
  return 0;
}

/* Read note line prefix (synthid).
 */

static int ctm_song_notes_read_prefix(int *synthid,int *level,const char *src,int srcc) {
  *synthid=*level=-1;
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }
  if (srcc<1) return 0;
  if ((srcc<2)||(src[0]!='[')||(src[srcc-1]!=']')) return -1;
  src++; srcc-=2;
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return 0;
  if (src[srcp]!=';') {
    *synthid=0;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      *synthid*=10; *synthid+=src[srcp++]-'0';
      if (*synthid>255) return -1;
    }
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    if (srcp>=srcc) return 0;
  }
  if (src[srcp++]!=';') return -1;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return 0;
  *level=0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    *level*=10; *level+=src[srcp++]-'0';
    if (*level>255) return -1;
  }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp<srcc) return -1;
  return 0;
}

/* Decode and output single note.
 */

static int ctm_song_decode_instruction(struct ctm_song *song,int chid,int synthid,int level,const char *src,int srcc) {
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }
  if (srcc<1) return 0;

  if (src[0]=='!') return 0; // silence (handled by preceding note)

  if (synthid<0) { // drum...
    if ((chid<0)||(chid>=song->drumchanc)) return -1;
    int effectid=ctm_decuint_eval(src,srcc);
    if (effectid<0) return -1;
    if (ctm_song_append_command(song,CTM_SONG_OPCODE_DRUM,chid,effectid,level)<0) return -1;
    return 0;

  } else { // synth...
    if ((chid<0)||(chid>=song->synthchanc)||(synthid>=song->synthc)||!song->synthv[synthid].samplev) return -1;
    if (srcc<1) return 0;
    int srcp=1,note;
    switch (src[0]) {
      case 'a': case 'A': note=0; break;
      case 'b': case 'B': note=2; break;
      case 'c': case 'C': note=3; break;
      case 'd': case 'D': note=5; break;
      case 'e': case 'E': note=7; break;
      case 'f': case 'F': note=8; break;
      case 'g': case 'G': note=10; break;
      default: return -1;
    }
    if (srcp>=srcc) return -1;
    if (src[srcp]=='#') { note++; srcp++; }
    else if (src[srcp]=='b') { note--; srcp++; }
    int octave=ctm_decuint_eval(src+srcp,srcc-srcp);
    if (octave<0) return -1;
    octave-=4;
    note=0x74+octave*12+note; // Should be 0x80, but that sounds too high. Maybe I fucked something up elsewhere...
    if (note<0) note=0x00; else if (note>0xff) note=0xff;
    if (ctm_song_append_command(song,CTM_SONG_OPCODE_SYNTH,chid,synthid,note)<0) return -1;
    if (ctm_song_append_command(song,CTM_SONG_OPCODE_LEVEL,chid,level,1)<0) return -1;
    return 1;

  }
}

/* Decode block of notes.
 * First line is the sync track. Starts with '@' followed by a '>' for each beat.
 * Remaining lines begin "[SYNTH;LEVEL]" for synth, or "[;LEVEL]" for drums.
 * Without prefix, makes a drum track.
 */

static int ctm_song_decode_block(struct ctm_song *song,struct ctm_song_line *linev,int linec,const char *refname) {
  if (linec<1) return 0;
  if (linev[0].src[0]!='@') {
    fprintf(stderr,"%s:%d: notes block must begin with sync track (which begins with '@')\n",refname,linev[0].lineno);
    return -1;
  }
  const char *sync=linev[0].src;
  int syncc=linev[0].srcc;
  
  const int quiettime=1000;
  int notetime=song->tempo-quiettime;

  // Read prefix of each line. This is the part before the first beat.
  int srcp=1,i;
  int synthchanc=0,drumchanc=0;
  while ((srcp<syncc)&&((unsigned char)sync[srcp]<=0x20)) srcp++;
  for (i=1;i<linec;i++) {
    if (ctm_song_notes_read_prefix(&linev[i].synthid,&linev[i].level,linev[i].src,srcp)<0) {
      fprintf(stderr,"%s:%d: failed to parse line prefix\n",refname,linev[i].lineno);
      return -1;
    }
    if (linev[i].synthid>=0) {
      if (linev[i].level<0) linev[i].level=song->synthlevel;
      if (synthchanc>=song->synthchanc) { fprintf(stderr,"%s:%d: more synth channels in block than allocated by header\n",refname,linev[i].lineno); return -1; }
      linev[i].chid=synthchanc++;
    } else {
      if (linev[i].level<0) linev[i].level=song->drumlevel;
      if (drumchanc>=song->drumchanc) { fprintf(stderr,"%s:%d: more drum channels in block than allocated by header\n",refname,linev[i].lineno); return -1; }
      linev[i].chid=drumchanc++;
    }
  }

  // Emit QUIET for unused synth channels.
  for (i=synthchanc;i<song->synthchanc;i++) {
    if (ctm_song_append_command(song,CTM_SONG_OPCODE_QUIET,i)<0) return -1;
  }

  // Read horizontally. Each '>' in sync track marks the beginning of a single instruction in all other tracks.
  while (srcp<syncc) {
    if (sync[srcp]!='>') {
      fprintf(stderr,"%s:%d: unexpected character '%c' in sync track\n",refname,linev[0].lineno,sync[srcp]);
      return -1;
    }
    int beatp=srcp;
    srcp++;
    while ((srcp<syncc)&&((unsigned char)sync[srcp]<=0x20)) srcp++;
    int beatc=srcp-beatp;
    for (i=1;i<linec;i++) {
      if (beatp>=linev[i].srcc) continue;
      const char *notesrc=linev[i].src+beatp;
      int notesrcc;
      if (srcp>=syncc) notesrcc=linev[i].srcc-beatp;
      else notesrcc=beatc;
      if (beatp>linev[i].srcc-notesrcc) notesrcc=linev[i].srcc-beatp;
      if (ctm_song_decode_instruction(song,linev[i].chid,linev[i].synthid,linev[i].level,notesrc,notesrcc)<0) {
        fprintf(stderr,"%s:%d: Failed to decode instruction '%.*s'\n",refname,linev[i].lineno,notesrcc,notesrc);
        return -1;
      }
    }
    if (ctm_song_append_command(song,CTM_SONG_OPCODE_DELAY,notetime>>16,notetime>>8,notetime)<0) return -1;
    for (i=1;i<linec;i++) {
      if (linev[i].synthid<0) continue;
      if ((srcp>=syncc)||((srcp<linev[i].srcc)&&((unsigned char)linev[i].src[srcp]>0x20))) {
        if (ctm_song_append_command(song,CTM_SONG_OPCODE_QUIET,linev[i].chid)<0) return -1;
      }
    }
    if (ctm_song_append_command(song,CTM_SONG_OPCODE_DELAY,quiettime>>16,quiettime>>8,quiettime)<0) return -1;
  }
  
  return 0;
}

/* Decode song.
 */
 
int ctm_song_decode(struct ctm_song *song,const char *src,int srcc,const char *refname) {
  if (!song) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!refname) refname="(unknown)";

  /* Header, through "BEGIN". */
  int srcp=0,lineno=1;
  while (srcp<srcc) {
    if ((src[srcp]==0x0a)||(src[srcp]==0x0d)) {
      lineno++;
      if (++srcp>=srcc) break;
      if (((src[srcp]==0x0a)||(src[srcp]==0x0d))&&(src[srcp]!=src[srcp-1])) srcp++;
      continue;
    }
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    const char *line=src+srcp;
    int linec=0,cmt=0;
    while ((srcp<srcc)&&(src[srcp]!=0x0a)&&(src[srcp]!=0x0d)) {
      if (src[srcp]=='#') cmt=1;
      else if (!cmt) linec++;
      srcp++;
    }
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    if (!linec) continue;
    if ((linec==5)&&!memcmp(line,"BEGIN",5)) break;
    if (ctm_song_decode_header(song,line,linec,refname,lineno)<0) return -1;
  }

  /* Body. Composed of line blocks separated by blank lines. */
  int chanc=song->synthchanc+song->drumchanc;
  if (chanc>0) {
    chanc++;
    struct ctm_song_line *linev=malloc(sizeof(struct ctm_song_line)*chanc);
    if (!linev) return -1;
    int linec=0;
    while (srcp<srcc) {
      const char *linesrc=src+srcp;
      int linesrcc=0,linespace=1,comment=0;
      while ((srcp<srcc)&&(src[srcp]!=0x0a)&&(src[srcp]!=0x0d)) { 
        if (linespace&&(src[srcp]=='#')) comment=1;
        else if ((unsigned char)src[srcp]>0x20) linespace=0;
        srcp++;
        linesrcc++;
      }
      if (comment) linespace=1;
      if (srcp<srcc) {
        if (++srcp<srcc) {
          if (((src[srcp]==0x0a)||(src[srcp]==0x0d))&&(src[srcp]!=src[srcp-1])) srcp++;
        }
      }
      if (linespace) {
        if (ctm_song_decode_block(song,linev,linec,refname)<0) { free(linev); return -1; }
        linec=0;
      } else if ((linesrcc==3)&&!memcmp(linesrc,"END",3)) {
        break;
      } else if (linec>=chanc) {
        fprintf(stderr,"%s:%d: block size exceeds stated channel counts\n",refname,lineno);
        free(linev);
        return -1;
      } else {
        linev[linec].src=linesrc;
        linev[linec].srcc=linesrcc;
        linev[linec].lineno=lineno;
        linec++;
      }
      lineno++;
    }
    if (ctm_song_decode_block(song,linev,linec,refname)<0) { free(linev); return -1; }
    free(linev);
  }

  return 0;
}
