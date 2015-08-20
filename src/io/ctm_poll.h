/* ctm_poll.h
 * Organized interface to poll(), so we only have to do it once per frame.
 * Everything using a pipe should cooperate with this.
 * We maintain associated lists of file descriptor and object.
 *
 * *** MinGW does not provide the poll() function. ***
 * It turns out that we only use this unit in conjunction with evdev, which doesn't exist under Windows.
 * So, instead of faking it (yikes!), we'll just stub out the whole unit when compiling for Windows.
 * I think that's safe: Under Windows, a file descriptor is meant to be only a regular file.
 *
 * The global poller never takes ownership of a file descriptor.
 * That is, it never calls close().
 *
 * Many operations have three forms, letting you search by fd, userdata, or both.
 * The three forms do precisely the same thing, just different ways of finding the object.
 *
 * This being a game about an election, maybe I should point out:
 *   "poll" in this file is an I/O thing, not a gameplay thing!
 */

#ifndef CTM_POLL_H
#define CTM_POLL_H

int ctm_poll_init();
void ctm_poll_quit();
int ctm_poll_update();

/* Register an open file and live object with the global poller.
 * If (cb_read) not NULL, the file is initially marked 'readable'.
 *
 * (fd) is your open file, must be >=0 and must not already be registered.
 * (userdata) is your live object. Can be anything.
 * (userdata_del) will be called by the core exactly once, when this file is removed.
 *
 * (cb_read,cb_write) are called during ctm_poll_update() when the file is ready for I/O.
 * (cb_error) is called during ctm_poll_update() if the file becomes unusable.
 * It *is* legal to modify the file registry during these callbacks.
 * If any callback fails, ctm_poll_update() will fail immediately.
 */
int ctm_poll_open(
  int fd,void *userdata,
  int (*cb_read)(int fd,void *userdata),
  int (*cb_write)(int fd,void *userdata),
  int (*cb_error)(int fd,void *userdata),
  void (*userdata_del)(void *userdata)
);

/* The association between (fd) and (userdata) is never allowed to change while registered.
 */
void *ctm_poll_get_userdata(int fd);
int ctm_poll_get_fd(void *userdata);

/* Set the 'readable' and 'writeable' bits for a file.
 * These all return >0 if the event mask was changed.
 * It is an error to set a flag for which the callback is unset.
 */
int ctm_poll_set_readable(int fd,void *userdata,int readable);
int ctm_poll_set_readable_fd(int fd,int readable);
int ctm_poll_set_readable_userdata(void *userdata,int readable);
int ctm_poll_set_writeable(int fd,void *userdata,int writeable);
int ctm_poll_set_writeable_fd(int fd,int writeable);
int ctm_poll_set_writeable_userdata(void *userdata,int writeable);

// NB: We do not close the file descriptor; that's up to you.
int ctm_poll_close(int fd,void *userdata);
int ctm_poll_close_fd(int fd);
int ctm_poll_close_userdata(void *userdata);

#endif
