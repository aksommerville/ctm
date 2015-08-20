/* ctm_shader.h
 * Structured GL shader.
 */

#ifndef CTM_SHADER_H
#define CTM_SHADER_H

struct ctm_shader {
  const char *vsrc; int vsrcc;
  const char *fsrc; int fsrcc;
  const char *name;
  int vtxsize;
  GLuint program;
  GLuint location_screensize;
  GLuint location_tilesize;
  GLuint location_upsidedown;
};

int ctm_shader_compile(struct ctm_shader *shader);
void ctm_shader_cleanup(struct ctm_shader *shader);

#endif
