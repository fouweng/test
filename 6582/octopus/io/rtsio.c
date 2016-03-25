/*************************************************************************
    > File Name: io_client.c
    > Author: steven_feng
    > Mail: steven_feng@realsil.com.cn
    > Created Time: 2015/04/05
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/types.h>
#include<string.h>
#include<errno.h>

#include"internal.h"
#include"hw_control.h"
#include"rtsio.h"

static uint8_t g_strcommout[7] = {0xFF, CAMERAID, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t g_chdir = 0x00;
static uint8_t g_level = 0x3F;

enum {
	CREAT = 0x03,
	RUN   = 0x07,
	DEL   = 0x05,
};

static inline void checksum(uint8_t *strcommout)
{
	int  i, sum = 0;

	for (i = 1; i < 6; i++)
		sum += strcommout[i];

	if (sum > 0xFF)
		strcommout[6] = sum % 0x100;
	else
		strcommout[6] = sum;
}

static void _set_direct(uint8_t chdir)
{
	g_chdir = chdir;
	bzero(&g_strcommout[2], 4);
	g_strcommout[3] = g_chdir;
	g_strcommout[4] = g_level;
	g_strcommout[5] = g_level;
	checksum(g_strcommout);
}

static void _set_speed(uint8_t level)
{
	g_level = level;
	bzero(&g_strcommout[2], 4);
	g_strcommout[3] = g_chdir;
	g_strcommout[4] = g_level;
	g_strcommout[5] = g_level;
	checksum(g_strcommout);
}

static void _set_preset(uint8_t func)
{
	bzero(&g_strcommout[2], 4);
	g_strcommout[3]  = func;
	g_strcommout[5]  = 0x01;
	checksum(g_strcommout);
}

static int set_io_cmd(int msg, uint8_t *strcommout)
{
	int ret;
	struct cmd_msg *cmd = NULL;

	ret = opt_alloc_cmd_message(&cmd, 7);
	if (!cmd)
		goto out;

	cmd->mtype = MSG_TYPE_REQUEST;
	cmd->pid = getpid();
	cmd->domain = CMD_DOMAIN_IO;

	memcpy(cmd->priv, strcommout, cmd->length);
	ret = opt_send_cmd_msg(msg, cmd, 10000);
	if (ret)
		goto out;
	if (OPT_STATUS_OK != cmd->status)
		fprintf(stderr, "cmd send failed\n");
out:
	if (cmd)
		opt_free_cmd_msg(&cmd);
	return ret;
}

int rts_io_ptz_running(uint8_t chdir)
{
	int ret;
	int fd = 0;

	if (chdir != PTZ_LEFT && chdir != PTZ_RIGHT && chdir != PTZ_UP
	   && chdir != PTZ_DOWN && chdir != PTZ_STOP)
		return -1;

	fd = opt_open_dev("/dev/ttyS0");
	if (fd < 0)
		goto out;

	ret = opt_lock_dev(fd);
	if (ret) {
		fprintf(stderr, "lock device fail\n");
		goto out;
	}

	_set_direct(chdir);
	ret = set_io_cmd(fd, g_strcommout);
	if (ret) {
		fprintf(stderr, "running device fail\n");
		goto out;
	}
out:
	opt_unlock_dev(fd);
	return ret;
}

int rts_io_ptz_running_angle(uint8_t chdir, uint16_t angle)
{
	int ret;
	int fd = 0;
	int time = 0;

	if (angle > 355 && angle < 360)
		return -1;
	if (angle > 360)
		angle = angle%360;

	if (chdir != PTZ_LEFT && chdir != PTZ_RIGHT && chdir != PTZ_UP
	   && chdir != PTZ_DOWN && chdir != PTZ_STOP)
		return -1;

	time = (angle*100*1000)/408;

	fd = opt_open_dev("/dev/ttyS0");
	if (fd < 0)
		goto out;

	ret = opt_lock_dev(fd);
	if (ret) {
		fprintf(stderr, "lock device fail\n");
		goto out;
	}

	_set_direct(chdir);
	ret = set_io_cmd(fd, g_strcommout);
	if (ret) {
		fprintf(stderr, "Running angle device fail\n");
		goto out;
	}
	opt_unlock_dev(fd);

	usleep(time * 1000);

	opt_lock_dev(fd);

	_set_direct(PTZ_STOP);
	ret = set_io_cmd(fd, g_strcommout);
	if (ret) {
		fprintf(stderr, "Running angle stop device fail\n");
		goto out;
	}

out:
	opt_unlock_dev(fd);
	return ret;
}

int rts_io_ptz_set_speed(uint8_t level)
{
	int ret;
	int fd = 0;

	if (level <= 0)
		level = 0;
	else if (level <= PTZ_SLOWEST)
		level = PTZ_SLOWEST;
	else if (level <= PTZ_SLOW)
		level = PTZ_SLOW;
	else if (level <= PTZ_NOMAL)
		level = PTZ_NOMAL;
	else if (level <= PTZ_FAST)
		level = PTZ_FAST;
	else if (level <= PTZ_FASTEST)
		level = PTZ_FASTEST;
	else
		level = 0x3F;

	fd = opt_open_dev("/dev/ttyS0");
	if (fd < 0)
		goto out;

	ret = opt_lock_dev(fd);
	if (ret) {
		fprintf(stderr, "lock device fail\n");
		goto out;
	}

	_set_speed(level);
	ret = set_io_cmd(fd, g_strcommout);
	if (ret) {
		fprintf(stderr, "Set speed fail\n");
		goto out;
	}
out:
	opt_unlock_dev(fd);
	return ret;
}

int rts_io_ptz_get_speed()
{
	return (int)g_level;
}


int rts_io_ptz_creat_preset()
{
	int fd;
	int ret;

	fd = opt_open_dev("/dev/ttyS0");
	if (fd < 0)
		goto out;

	ret = opt_lock_dev(fd);
	if (ret) {
		fprintf(stderr, "lock device fail\n");
		goto out;
	}

	_set_preset(CREAT);
	ret = set_io_cmd(fd, g_strcommout);
	if (ret) {
		fprintf(stderr, "rts_io_ptz_creat_preset fail\n");
		goto out;
	}
out:
	opt_unlock_dev(fd);
	return ret;
}

int rts_io_ptz_run_preset()
{
	int fd;
	int ret;

	fd = opt_open_dev("/dev/ttyS0");
	if (fd < 0)
		goto out;

	ret = opt_lock_dev(fd);
	if (ret) {
		fprintf(stderr, "lock device fail\n");
		goto out;
	}

	_set_preset(RUN);
	ret = set_io_cmd(fd, g_strcommout);
	if (ret) {
		fprintf(stderr, "rts_io_ptz_run_preset fail\n");
		goto out;
	}
out:
	opt_unlock_dev(fd);
	return ret;
}

int rts_io_ptz_del_preset()
{
	int fd;
	int ret;

	fd = opt_open_dev("/dev/ttyS0");
	if (fd < 0)
		goto out;

	ret = opt_lock_dev(fd);
	if (ret) {
		fprintf(stderr, "lock device fail\n");
		goto out;
	}

	_set_preset(DEL);
	ret = set_io_cmd(fd, g_strcommout);
	if (ret) {
		fprintf(stderr, "rts_io_ptz_del_preset fail\n");
		goto out;
	}
out:
	opt_unlock_dev(fd);
	return ret;
}
