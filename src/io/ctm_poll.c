#include "ctm.h"
#include "ctm_poll.h"

#if CTM_ARCH==CTM_ARCH_mswin // Stub.

int ctm_poll_init() { return 0; }
void ctm_poll_quit() {}
int ctm_poll_update() { return 0; }

int ctm_poll_open(
  int fd,void *userdata,
  int (*cb_read)(int fd,void *userdata),
  int (*cb_write)(int fd,void *userdata),
  int (*cb_error)(int fd,void *userdata),
  void (*userdata_del)(void *userdata)
) { return -1; }
void *ctm_poll_get_userdata(int fd) { return 0; }
int ctm_poll_get_fd(void *userdata) { return -1; }
int ctm_poll_set_readable(int fd,void *userdata,int readable) { return -1; }
int ctm_poll_set_readable_fd(int fd,int readable) { return -1; }
int ctm_poll_set_readable_userdata(void *userdata,int readable) { return -1; }
int ctm_poll_set_writeable(int fd,void *userdata,int writeable) { return -1; }
int ctm_poll_set_writeable_fd(int fd,int writeable) { return -1; }
int ctm_poll_set_writeable_userdata(void *userdata,int writeable) { return -1; }
int ctm_poll_close(int fd,void *userdata) { return -1; }
int ctm_poll_close_fd(int fd) { return -1; }
int ctm_poll_close_userdata(void *userdata) { return -1; }

#else // No stub, ordinary implementation.

#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* Globals.
 */

struct ctm_poll_meta {
  int (*cb_read)(int fd,void *userdata);
  int (*cb_write)(int fd,void *userdata);
  int (*cb_error)(int fd,void *userdata);
  void *userdata;
  void (*userdata_del)(void *userdata);
};

static struct {
  struct pollfd *pollfdv;
  struct ctm_poll_meta **metav;
  int c,a;
  int timeout; // ms
} ctm_poll={0};

/* Init.
 */

int ctm_poll_init() {
  memset(&ctm_poll,0,sizeof(ctm_poll));

  ctm_poll.timeout=10; // ms
  
  return 0;
}

/* Quit.
 */

void ctm_poll_quit() {

  while (ctm_poll.c-->0) {
    int fd=ctm_poll.pollfdv[ctm_poll.c].fd;
    struct ctm_poll_meta *meta=ctm_poll.metav[ctm_poll.c];
    if (!meta) continue;
    if (meta->userdata_del) meta->userdata_del(meta->userdata);
    free(meta);
  }

  if (ctm_poll.pollfdv) free(ctm_poll.pollfdv);
  if (ctm_poll.metav) free(ctm_poll.metav);
  memset(&ctm_poll,0,sizeof(ctm_poll));
}

/* Update.
 */

int ctm_poll_update() {

  if (ctm_poll.c<1) {
    usleep(ctm_poll.timeout*1000);
    return 0;
  }

  int activec=poll(ctm_poll.pollfdv,ctm_poll.c,ctm_poll.timeout);
  if (activec<0) {
    if (errno==EINTR) return 0;
    return -1;
  }
  int i; for (i=0;i<ctm_poll.c;i++) {
    if (!ctm_poll.pollfdv[i].revents) continue;
    int fd=ctm_poll.pollfdv[i].fd;
    int revents=ctm_poll.pollfdv[i].revents;
    struct ctm_poll_meta *meta=ctm_poll.metav[i];
    
    if (revents&(POLLERR|POLLHUP)) {
      if (meta->cb_error) {
        if (meta->cb_error(fd,meta->userdata)<0) return -1;
      } else {
        if (ctm_poll_close(fd,meta->userdata)<0) return -1;
      }
    } else if (revents&POLLIN) {
      if (meta->cb_read) {
        if (meta->cb_read(fd,meta->userdata)<0) return -1;
      } else {
        if (ctm_poll_close(fd,meta->userdata)<0) return -1;
      }
    } else if (revents&POLLOUT) {
      if (meta->cb_write) {
        if (meta->cb_write(fd,meta->userdata)<0) return -1;
      } else {
        if (ctm_poll_close(fd,meta->userdata)<0) return -1;
      }
    }
    
    if (--activec<=0) break;
  }

  return 0;
}

/* List internals.
 */

static int ctm_poll_search_fd(int fd) {
  int lo=0,hi=ctm_poll.c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (fd<ctm_poll.pollfdv[ck].fd) hi=ck;
    else if (fd>ctm_poll.pollfdv[ck].fd) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int ctm_poll_search_userdata(void *userdata) {
  int i; for (i=0;i<ctm_poll.c;i++) if (userdata==ctm_poll.metav[i]->userdata) return i;
  return -1;
}

static int ctm_poll_insert(int p,int fd) {
  if ((p<0)||(p>ctm_poll.c)) return -1;
  if (fd<0) return -1;
  if (p&&(fd<=ctm_poll.pollfdv[p-1].fd)) return -1;
  if ((p<ctm_poll.c)&&(fd>=ctm_poll.pollfdv[p].fd)) return -1;
  if (ctm_poll.c>=ctm_poll.a) {
    int na=ctm_poll.a+8;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(ctm_poll.pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    ctm_poll.pollfdv=nv;
    if (!(nv=realloc(ctm_poll.metav,sizeof(void*)*na))) return -1;
    ctm_poll.metav=nv;
    ctm_poll.a=na;
  }
  struct ctm_poll_meta *meta=calloc(1,sizeof(struct ctm_poll_meta));
  if (!meta) return -1;
  memmove(ctm_poll.pollfdv+p+1,ctm_poll.pollfdv+p,sizeof(struct pollfd)*(ctm_poll.c-p));
  memmove(ctm_poll.metav+p+1,ctm_poll.metav+p,sizeof(void*)*(ctm_poll.c-p));
  ctm_poll.c++;
  struct pollfd *pollfd=ctm_poll.pollfdv+p;
  memset(pollfd,0,sizeof(struct pollfd));
  pollfd->fd=fd;
  ctm_poll.metav[p]=meta;
  return 0;
}

static int ctm_poll_remove(int p) {
  if ((p<0)||(p>=ctm_poll.c)) return -1;
  int fd=ctm_poll.pollfdv[p].fd;
  struct ctm_poll_meta *meta=ctm_poll.metav[p];
  ctm_poll.c--;
  memmove(ctm_poll.pollfdv+p,ctm_poll.pollfdv+p+1,sizeof(struct pollfd)*(ctm_poll.c-p));
  memmove(ctm_poll.metav+p,ctm_poll.metav+p+1,sizeof(void*)*(ctm_poll.c-p));
  if (meta->userdata_del) meta->userdata_del(meta->userdata);
  free(meta);
  return 0;
}

/* Add file.
 */

int ctm_poll_open(
  int fd,void *userdata,
  int (*cb_read)(int fd,void *userdata),
  int (*cb_write)(int fd,void *userdata),
  int (*cb_error)(int fd,void *userdata),
  void (*userdata_del)(void *userdata)
) {
  if (fd<0) return -1;
  int p=ctm_poll_search_fd(fd);
  if (p>=0) return -1;
  p=-p-1;
  if (ctm_poll_insert(p,fd)<0) return -1;
  struct pollfd *pollfd=ctm_poll.pollfdv+p;
  struct ctm_poll_meta *meta=ctm_poll.metav[p];
  meta->userdata=userdata;
  meta->userdata_del=userdata_del;
  if (meta->cb_read=cb_read) pollfd->events|=POLLIN;
  meta->cb_write=cb_write;
  meta->cb_error=cb_error;
  return 0;
}

/* Userdata for fd or vice versa.
 */

void *ctm_poll_get_userdata(int fd) {
  int p=ctm_poll_search_fd(fd);
  if (p<0) return 0;
  return ctm_poll.metav[p]->userdata;
}

int ctm_poll_get_fd(void *userdata) {
  int p=ctm_poll_search_userdata(userdata);
  if (p<0) return -1;
  return ctm_poll.pollfdv[p].fd;
}

/* Set event bits.
 */

static int ctm_poll_set_bit(int p,int mask,int value,void *cb) {
  if (value) {
    if (ctm_poll.pollfdv[p].events&mask) return 0;
    if (!cb) return -1;
    ctm_poll.pollfdv[p].events|=mask;
  } else {
    if (!(ctm_poll.pollfdv[p].events&mask)) return 0;
    ctm_poll.pollfdv[p].events&=~mask;
  }
  return 1;
}

int ctm_poll_set_readable(int fd,void *userdata,int readable) {
  int p=ctm_poll_search_fd(fd);
  if (p<0) return -1;
  if (ctm_poll.metav[p]->userdata!=userdata) return 0;
  return ctm_poll_set_bit(p,POLLIN,readable,ctm_poll.metav[p]->cb_read);
}

int ctm_poll_set_readable_fd(int fd,int readable) {
  int p=ctm_poll_search_fd(fd);
  if (p<0) return -1;
  return ctm_poll_set_bit(p,POLLIN,readable,ctm_poll.metav[p]->cb_read);
}

int ctm_poll_set_readable_userdata(void *userdata,int readable) {
  int p=ctm_poll_search_userdata(userdata);
  if (p<0) return -1;
  return ctm_poll_set_bit(p,POLLIN,readable,ctm_poll.metav[p]->cb_read);
}

int ctm_poll_set_writeable(int fd,void *userdata,int writeable) {
  int p=ctm_poll_search_fd(fd);
  if (p<0) return -1;
  if (ctm_poll.metav[p]->userdata!=userdata) return 0;
  return ctm_poll_set_bit(p,POLLOUT,writeable,ctm_poll.metav[p]->cb_write);
}

int ctm_poll_set_writeable_fd(int fd,int writeable) {
  int p=ctm_poll_search_fd(fd);
  if (p<0) return -1;
  return ctm_poll_set_bit(p,POLLOUT,writeable,ctm_poll.metav[p]->cb_write);
}

int ctm_poll_set_writeable_userdata(void *userdata,int writeable) {
  int p=ctm_poll_search_userdata(userdata);
  if (p<0) return -1;
  return ctm_poll_set_bit(p,POLLOUT,writeable,ctm_poll.metav[p]->cb_write);
}

/* Close file.
 */

int ctm_poll_close(int fd,void *userdata) {
  int p=ctm_poll_search_fd(fd);
  if (p<0) return -1;
  if (ctm_poll.metav[p]->userdata!=userdata) return -1;
  return ctm_poll_remove(p);
}

int ctm_poll_close_fd(int fd) {
  int p=ctm_poll_search_fd(fd);
  if (p<0) return -1;
  return ctm_poll_remove(p);
}

int ctm_poll_close_userdata(void *userdata) {
  int p=ctm_poll_search_userdata(userdata);
  if (p<0) return -1;
  return ctm_poll_remove(p);
}

#endif
