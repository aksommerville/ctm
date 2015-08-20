#include "ctm.h"
#include "video/ctm_video_internal.h"
#include "game/ctm_geography.h"

/* Generate newspaper texture.
 * Everything loaded, and GL is pointing at the texture.
 * If you do nothing here, a blank newspaper pops out.
 */

static int ctm_generate_newspaper_1(GLuint texid_bg,GLuint texid_bits,int stateix,int party) {

  glClearColor(0.0,0.0,0.0,0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  int event;
  while (1) {
    event=rand()%8;
    if ((party>=1)&&(party<=6)) switch (event) {
      // Some events are only valid for "all parties".
      case 1: case 2: case 6: continue; // Nobel prize, moon landing, and dragon.
    } else switch (event) {
      // Some events are only valid with a specific party.
      case 5: continue; // dedicating monument
    }
    break;
  }

  const char *statename="US";
  if ((stateix>=0)&&(stateix<9)) statename=ctm_state_data[stateix].NAME;
  const char *papername;
  switch (rand()%8) {
    case 0: papername="TIMES"; break;
    case 1: papername="INQUIRER"; break;
    case 2: papername="POST"; break;
    case 3: papername="GLOBE"; break;
    case 4: papername="FREE PRESS"; break;
    case 5: papername="TRIBUNE"; break;
    case 6: papername="TODAY"; break;
    case 7: papername="HERALD"; break;
  }

  { // Background...
    if (ctm_draw_texture(0,0,256,256,texid_bg,0)<0) return -1;
  }

  { // Photo...
    struct ctm_vertex_tex *vtxv=ctm_video_vtxv_append(&ctm_shader_tex,4);
    if (!vtxv) return -1;
    vtxv[0].x= 80; vtxv[0].y=124;
    vtxv[1].x=208; vtxv[1].y=124;
    vtxv[2].x= 80; vtxv[2].y= 60;
    vtxv[3].x=208; vtxv[3].y= 60;
    switch (event) {
      case 0: vtxv[0].tx=0.0f; vtxv[0].ty=0.25f; break;
      case 1: vtxv[0].tx=0.5f; vtxv[0].ty=0.25f; break;
      case 2: vtxv[0].tx=0.0f; vtxv[0].ty=0.50f; break;
      case 3: vtxv[0].tx=0.5f; vtxv[0].ty=0.50f; break;
      case 4: vtxv[0].tx=0.0f; vtxv[0].ty=0.75f; break;
      case 5: vtxv[0].tx=0.5f; vtxv[0].ty=0.75f; break;
      case 6: vtxv[0].tx=0.0f; vtxv[0].ty=1.00f; break;
      case 7: vtxv[0].tx=0.5f; vtxv[0].ty=1.00f; break;
    }
    vtxv[1].tx=vtxv[3].tx=vtxv[0].tx+0.5f;
    vtxv[2].tx=vtxv[0].tx;
    vtxv[1].ty=vtxv[0].ty;
    vtxv[2].ty=vtxv[3].ty=vtxv[0].ty-0.25f;
    glUseProgram(ctm_shader_tex.program);
    glBindTexture(GL_TEXTURE_2D,texid_bits);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct ctm_vertex_tex),&vtxv[0].x);
    glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(struct ctm_vertex_tex),&vtxv[0].tx);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    ctm_video.vtxc=0;
  }

  int namec;
  { // Headline and paper name...
    char name[22];
    char msg1[32]="",msg2[32]=""; // Actually limited to 30 bytes.
    switch (event) {
      case 0: {
          if ((party>=1)&&(party<=6)) snprintf(msg1,sizeof(msg1),"WEREWOLF RESCUES %s BABY",CTM_PARTY_NAME[party]);
          else snprintf(msg1,sizeof(msg1),"WEREWOLF RESCUES BABY");
          snprintf(msg2,sizeof(msg2),"FROM BURNING BUILDING");
        } break;
      case 1: {
          snprintf(msg1,sizeof(msg1),"WEREWOLF WINS NOBEL PRIZE");
        } break;
      case 2: {
          snprintf(msg1,sizeof(msg1),"WEREWOLF LANDS ON MOON");
        } break;
      case 3: if ((party>=1)&&(party<=6)) {
          snprintf(msg1,sizeof(msg1),"WEREWOLF CURES %sPOX",CTM_PARTY_NAME[party]);
        } else {
          snprintf(msg1,sizeof(msg1),"WEREWOLF DISCOVERS");
          snprintf(msg2,sizeof(msg2),"CURE FOR CANCER");
        } break;
      case 4: if ((party>=1)&&(party<=6)) {
          snprintf(msg1,sizeof(msg1),"\"FREE HOT DOGS FOR %sS!\"",CTM_PARTY_NAME[party]);
        } else {
          snprintf(msg1,sizeof(msg1),"\"FREE HOT DOGS!\"");
        } break;
      case 5: {
          snprintf(msg1,sizeof(msg1),"WEREWOLF DEDICATES");
          snprintf(msg2,sizeof(msg2),"%s MONUMENT",CTM_PARTY_NAME[party]);
        } break;
      case 6: {
          snprintf(msg1,sizeof(msg1),"WEREWOLF SLAYS");
          snprintf(msg2,sizeof(msg2),"UNPOPULAR DRAGON");
        } break;
      case 7: if ((party>=1)&&(party<=6)) {
          snprintf(msg1,sizeof(msg1),"WEREWOLF RESCUES");
          snprintf(msg2,sizeof(msg2),"%s PRINCESS",CTM_PARTY_NAME[party]);
        } else {
          snprintf(msg1,sizeof(msg1),"WEREWOLF RESCUES PRINCESS");
        } break;
    }
    namec=snprintf(name,sizeof(name),"%s %s",statename,papername);
    int msg1c=0; while (msg1[msg1c]) msg1c++;
    int msg2c=0; while (msg2[msg2c]) msg2c++;
    if (ctm_video_begin_text(16)<0) return -1;
    if (ctm_video_add_text(name,namec,129-namec*4,12,0x303030ff)<0) return -1;
    if (msg2c) {
      if (ctm_video_add_text(msg1,30,129-msg1c*4,34,0x303030ff)<0) return -1;
      if (ctm_video_add_text(msg2,30,129-msg2c*4,50,0x303030ff)<0) return -1;
    } else {
      if (ctm_video_add_text(msg1,30,129-msg1c*4,42,0x303030ff)<0) return -1;
    }
    if (ctm_video_end_text(ctm_video.texid_font)<0) return -1;
  }

  { // Fancy blue and red bars by the paper's name.
    int barw=(242-namec*8-10)>>1;
    if (ctm_draw_rect(4,4,barw,16,0x707090ff)<0) return -1;
    if (ctm_draw_rect(246-barw,4,barw,16,0x907070ff)<0) return -1;
  }
  
  return 0;
}

/* Generate newspaper texture, main entry point.
 */

GLuint ctm_generate_newspaper(int stateix,int party) {

  GLuint texid_bg=ctm_video_load_png("newspaper.png",0);
  if (!texid_bg) return 0;
  GLuint texid_bits=ctm_video_load_png("paperbits.png",0);
  if (!texid_bits) { glDeleteTextures(1,&texid_bg); return 0; }
  GLuint texid=0;
  glGenTextures(1,&texid);
  if (!texid) { glDeleteTextures(1,&texid_bg); glDeleteTextures(1,&texid_bits); return 0; }
  glBindTexture(GL_TEXTURE_2D,texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
  GLuint fb=0;
  glGenFramebuffers(1,&fb);
  if (!fb) { glDeleteTextures(1,&texid); glDeleteTextures(1,&texid_bg); glDeleteTextures(1,&texid_bits); return 0; }
  glBindFramebuffer(GL_FRAMEBUFFER,fb);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texid,0);
  ctm_video_set_uniform_screen_size(256,256);

  int err=ctm_generate_newspaper_1(texid_bg,texid_bits,stateix,party);

  glBindFramebuffer(GL_FRAMEBUFFER,0);
  ctm_video_reset_uniform_screen_size();
  glDeleteTextures(1,&texid_bits);
  glDeleteTextures(1,&texid_bg);
  glDeleteFramebuffers(1,&fb);
  if (err<0) { glDeleteTextures(1,&texid); return 0; }
  return texid;
}
