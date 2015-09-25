#include "ctm_video_internal.h"
#include "ctm_shader.h"

#if CTM_TEST_DISABLE_VIDEO
  void ctm_shader_cleanup(struct ctm_shader *shader) {}
  int ctm_shader_compile(struct ctm_shader *shader) { return 0; }
#else

/* Delete shader object.
 */

void ctm_shader_cleanup(struct ctm_shader *shader) {
  if (!shader) return;
  if (shader->program) { glDeleteProgram(shader->program); shader->program=0; }
}

/* Compile single shader.
 */

static GLuint ctm_shader_compile_1(struct ctm_shader *shader,GLuint type,const char *src,int srcc) {

  char version[256];
  int versionc;
  #if CTM_ARCH==CTM_ARCH_linux // Don't ask me, I just work here.
    versionc=snprintf(version,sizeof(version),"#version %d\nprecision mediump float;\n",GLSLVERSION);
  #else
    versionc=snprintf(version,sizeof(version),"#version %d\n",GLSLVERSION);
  #endif
  if ((versionc<1)||(versionc>=sizeof(version))) return 0;
  const char *srcv[2]={version,src};
  GLuint lenv[2]={versionc,srcc};

  GLuint id=glCreateShader(type);
  if (!id) return 0;
  glShaderSource(id,2,srcv,lenv);
  glCompileShader(id);

  GLint status=0;
  glGetShaderiv(id,GL_COMPILE_STATUS,&status);
  if (status) return id;

  const char *tname=(type==GL_VERTEX_SHADER)?"vertex":"fragment";
  fprintf(stderr,"ctm: Failed to compile %s shader '%s'.\n",tname,shader->name);
  GLint loga=0;
  glGetShaderiv(id,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetShaderInfoLog(id,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) fprintf(stderr,"%.*s\n",logc,log);
      free(log);
    }
  }
  glDeleteShader(id);
  return 0;
}

/* Bind attributes automatically from text.
 */
 
static int ctm_bind_attributes(GLuint program,const char *src,int srcc) {
  char zname[256];
  int attrid=0;
  int srcp=0; while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (srcp>srcc-9) break;
    if (memcmp(src+srcp,"attribute",9)) {
      while ((srcp<srcc)&&(src[srcp]!=0x0a)&&(src[srcp]!=0x0d)) srcp++;
    } else {
      srcp+=9;
      while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++; // type
      while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      const char *name=src+srcp;
      int namec=0;
      while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!=';')) { srcp++; namec++; }
      if (!namec) return -1;
      if (namec>=sizeof(zname)) return -1;
      memcpy(zname,name,namec);
      zname[namec]=0;
      glBindAttribLocation(program,attrid,zname);
      attrid++;
      while ((srcp<srcc)&&(src[srcp]!=0x0a)&&(src[srcp]!=0x0d)) srcp++;
    }
  }
  return 0;
}

/* Compile and link shader.
 */

int ctm_shader_compile(struct ctm_shader *shader) {
  if (!shader) return -1;
  if (shader->program) return 0;

  GLuint vshader=ctm_shader_compile_1(shader,GL_VERTEX_SHADER,shader->vsrc,shader->vsrcc);
  if (!vshader) return -1;
  GLuint fshader=ctm_shader_compile_1(shader,GL_FRAGMENT_SHADER,shader->fsrc,shader->fsrcc);
  if (!fshader) { glDeleteShader(vshader); return -1; }

  if (!(shader->program=glCreateProgram())) { glDeleteShader(vshader); glDeleteShader(fshader); return -1; }
  glAttachShader(shader->program,vshader);
  glAttachShader(shader->program,fshader);
  glDeleteShader(vshader);
  glDeleteShader(fshader);

  if (ctm_bind_attributes(shader->program,shader->vsrc,shader->vsrcc)<0) { glDeleteProgram(shader->program); shader->program=0; return -1; }

  glLinkProgram(shader->program);
  GLint status=0;
  glGetProgramiv(shader->program,GL_LINK_STATUS,&status);
  if (status) {

    /* Link successful. Get the location of a few common uniforms. */
    shader->location_screensize=glGetUniformLocation(shader->program,"screensize");
    shader->location_tilesize=glGetUniformLocation(shader->program,"tilesize");
    shader->location_upsidedown=glGetUniformLocation(shader->program,"upsidedown");

    return 0;
  }

  /* Link failed. Log it and clear the object. */
  fprintf(stderr,"ctm: Failed to link program '%s'.\n",shader->name);
  GLint loga=0;
  glGetProgramiv(shader->program,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetProgramInfoLog(shader->program,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) fprintf(stderr,"%.*s\n",logc,log);
      free(log);
    }
  }
  glDeleteProgram(shader->program);
  shader->program=0;
  return -1;
}

#endif
