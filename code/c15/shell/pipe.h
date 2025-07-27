#ifndef __SHELL_PIPE_H
#define __SHELL_PIPE_H
#include "../lib/kernel/stdint.h"

uint32_t pipe_write(int32_t fd, const void* buf, uint32_t count);
uint32_t pipe_read(int32_t fd, void* buf, uint32_t count);
int32_t sys_pipe(int32_t pipefd[2]);
bool is_pipe(uint32_t local_fd);

#endif