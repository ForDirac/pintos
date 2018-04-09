#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

/* For proj #2 */
int syscall_open(const char *file);

int syscall_read(int fd, void *buffer, unsigned size);

int syscall_write(int fd, void *buffer, unsigned size);

void syscall_close(int fd);

#endif /* userprog/syscall.h */
