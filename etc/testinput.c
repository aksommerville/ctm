#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <linux/input.h>

/* --help
 */

static void print_help() {
  printf(
    "Usage: testinput [OPTIONS] DEVICE_FILE\n"
    "DEVICE_FILE is typically one of /dev/input/eventX.\n"
    "OPTIONS:\n"
    "  --help        Print this message and exit.\n"
    "  --no-axes     Ignore EV_ABS and EV_REL events.\n"
    "  --minimal     Ignore all but EV_KEY, and maybe EV_ABS and EV_REL.\n"
    "  --decimal     Print event codes in decimal rather than hexadecimal.\n"
  );
}

/* Signals.
 */

static volatile int sigc=0;

static void rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: case SIGQUIT: case SIGTERM: if (++sigc>=3) exit(1); break;
  }
}

/* Main.
 */

int main(int argc,char **argv) {

  signal(SIGINT,rcvsig);
  signal(SIGTERM,rcvsig);
  signal(SIGQUIT,rcvsig);

  /* Read arguments. */
  const char *path=0;
  int no_axes=0;
  int minimal=0;
  int decimal=0;
  int argi; for (argi=1;argi<argc;) {
    const char *arg=argv[argi++];
    if (!strcmp(arg,"--help")) { print_help(); return 0; }
    else if (!strcmp(arg,"--no-axes")) no_axes=1;
    else if (!strcmp(arg,"--minimal")) minimal=1;
    else if (!strcmp(arg,"--decimal")) decimal=1;
    else if (arg[0]=='-') {
      fprintf(stderr,"%s: Unexpected option '%s'.\n",argv[0],arg);
      return 1;
    } else if (path) {
      fprintf(stderr,"%s: One device at a time, please.\n",argv[0]);
      return 1;
    } else path=arg;
  }
  if (!path) { print_help(); return 1; }

  /* Open device and read some info. */
  int fd=open(path,O_RDONLY);
  if (fd<0) {
    fprintf(stderr,"%s:open: %m\n",path);
    return 1;
  }
  int version=0;
  if (ioctl(fd,EVIOCGVERSION,&version)<0) {
    fprintf(stderr,"%s:EVIOCGVERSION: %m\n",path);
    return 1;
  }
  ioctl(fd,EVIOCGRAB,1);
  char name[256];
  ioctl(fd,EVIOCGNAME(256),name);
  name[255]=0;
  int namec=0; while (name[namec]) namec++;
  struct input_id id={0};
  ioctl(fd,EVIOCGID,&id);
  printf("%s: Opened evdev device.\n",path);
  printf("  Driver version: 0x%08x\n",version);
  printf("  Device name: '%.*s'\n",namec,name);
  printf("  Bus, vendor, product, version: %d, 0x%04x, 0x%04x, 0x%04x\n",id.bustype,id.vendor,id.product,id.version);
  printf("Will now dump events. Ctrl-C to quit...\n");

  /* Dump events until interrupted. */
  while (!sigc) {
    fd_set fds;
    struct timeval to={1,0};
    FD_ZERO(&fds);
    FD_SET(fd,&fds);
    if (select(FD_SETSIZE,&fds,0,0,&to)>0) {
      struct input_event evt={0};
      int err=read(fd,&evt,sizeof(evt));
      if (err<0) {
        printf("%s:read: %m\n",path);
        break;
      }
      if (!err) {
        printf("%s:read: End of file.\n",path);
        break;
      }
      if (minimal&&!evt.value) continue;
      switch (evt.type) {
        case EV_ABS: if (no_axes) continue; break;
        case EV_REL: if (no_axes) continue; break;
        case EV_KEY: break;
        default: if (minimal) continue; break;
      }
      if (decimal) {
        printf("%d.%06d %d.%d = %d\n",evt.time.tv_sec,evt.time.tv_usec,evt.type,evt.code,evt.value);
      } else {
        printf("%d.%06d %d.0x%04x = %d\n",evt.time.tv_sec,evt.time.tv_usec,evt.type,evt.code,evt.value);
      }
    }
  }

  ioctl(fd,EVIOCGRAB,0);
  close(fd);
  return 0;
}
