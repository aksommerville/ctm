#ifndef CTM_SONG_H
#define CTM_SONG_H

// Size of a synthesized wave.
// Must match the output frequency, ie 44100.
#define CTM_AUDIO_SYNTH_SIZE 44100

struct ctm_audio_channel_verbatim {
  const int16_t *samplev; // borrowed, presumably from fxv
  int samplec;
  int samplep;
  uint8_t level;
};

struct ctm_audio_channel_synth {

  const int16_t *samplev; // borrowed from song's synth store
  double samplep;
  double freq; // Because waves are synthesized at exactly 1 Hz, 'freq' is also the step distance.
  uint8_t trim;

  // Trim slider.
  uint8_t trimsrc,trimdst;
  int trimp,trimc;
  int trimend; // If nonzero, channel goes silent when this fade ends

  // Frequency slider.
  double freqsrc,freqdst;
  int freqp,freqc;
  
};

struct ctm_song_command {
  uint8_t opcode;
  uint8_t argv[3];
};

struct ctm_song {
  int songid;
  struct ctm_audio_channel_synth *synthchanv;
  int synthchanc;
  struct ctm_audio_channel_verbatim *drumchanv;
  int drumchanc;
  struct ctm_audio_effect *synthv; int synthc,syntha;
  struct ctm_song_command *cmdv; int cmdc,cmda;
  int delay;
  int cmdp;
  int tempo; // frames per beat
  uint8_t drumlevel;
  uint8_t synthlevel;
};

struct ctm_song *ctm_song_new();
void ctm_song_del(struct ctm_song *song);
int ctm_song_decode(struct ctm_song *song,const char *src,int srcc,const char *refname);

int16_t ctm_song_update(struct ctm_song *song);

int ctm_song_reset(struct ctm_song *song);

#define CTM_SONG_OPCODE_DELAY         0x01 // [0,1,2]: 24-bit frame count
#define CTM_SONG_OPCODE_DRUM          0x02 // [0]:channel, [1]:effectid, [2]:level
#define CTM_SONG_OPCODE_SYNTH         0x03 // [0]:channel, [1]:synth, [2]:note
#define CTM_SONG_OPCODE_LEVEL         0x04 // [0]:channel, [1]:level, [2]:duration (*1000 frames)
#define CTM_SONG_OPCODE_NOTE          0x05 // [0]:channel, [1]:note, [2]:duration (*1000 frames)
#define CTM_SONG_OPCODE_QUIET         0x06 // [0]:channel

#endif
