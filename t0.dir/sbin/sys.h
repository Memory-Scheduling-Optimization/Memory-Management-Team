#ifndef _SYS_H_
#define _SYS_H_

#include "stdint.h"

/****************/
/* System calls */
/****************/

typedef int ssize_t;
typedef unsigned int size_t;

/* all system calls return negative value on failure except when noted */

/* exit */
/* never returns, rc is the exit code */
extern void exit(int rc);

/* open */
/* opens a file, returns file descriptor, flags is ignored */
extern int open(const char* fn, int flags);

/* len */
/* returns number of bytes in the file, negative indicates error or a console device */
extern ssize_t len(int fd);

/* write */
/* writes up to 'nbytes' to file, returns number of bytes written */
extern ssize_t write(int fd, void* buf, size_t nbyte);

/* read */
/* reads up to nbytes from file, returns number of bytes read */
extern ssize_t read(int fd, void* buf, size_t nbyte);

/* create semaphore */
/* returns semaphore descriptor */
extern int sem(uint32_t initial);

/* up */
/* semaphore up */
/* return 0 on success, -ve value on failure */
extern int up(int id);

/* down */
/* semaphore down */
/* return 0 on success, -ve value on failure */
extern int down(int id);

/* close */
/* closes either a file or a semaphore or disowns a child process */
/* return 0 on success, -ve value on failure */
extern int close(int id);

/* shutdown */
/* should never return */
extern int shutdown(void);

/* wait */
/* wait for a child, status filled with exit value from child */
/* return 0 on success, -ve value on failure */
extern int wait(int id, uint32_t *status);

/* seek */
/* seek to given offset in file */
/* returns the new offset on success, -ve value on failure */
/* seeking in a console device is an error */
/* seeking outside the file is not an error but might cause
   subsequent read/write to fail */
extern off_t seek(int fd, off_t offset);

/* fork */
/* 0 => child, +ve => parent, -ve => error */
extern int fork();

/* execl */
/* returning indicates an error */
/* arg0 is the name of the program by convention */
/* a nullptr indicates end of arguments */
extern int execl(const char* path, const char* arg0, ...);

#endif
