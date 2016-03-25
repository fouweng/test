/*
 * utils.c
 * Author: Tony Nie (tony_nie@realsil.com.cn)
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>


#include "utils.h"
#include "hw_control.h"

uint32_t opt_log_level = 0
	| OPT_LOG_FINE
	| OPT_LOG_INFO
	| OPT_LOG_WARNING
	| OPT_LOG_ERROR
	| OPT_LOG_FATAL
	| 0;

void del_msg_file(const char *path)
{
	unlink(path);
}

int gen_msg_file(const char *path)
{
	int fd = 0;

	fd = open(path, O_CREAT | O_RDONLY, S_IRWXU | S_IRWXU | S_IRGRP);
	if (fd > 0) {
		fsync(fd);
		close(fd);
		return 0;
	} else {
		opt_error("errno:%d %s %s\n",
				errno, strerror(errno), __func__);
		return fd;
	}

	return 0;
}
