/*
 * uvc_client.c
 * Author: Tony Nie (tony_nie@realsil.com.cn)
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include "uvcvideo.h"
#include "utils.h"
#include "hw_control.h"
#include "octopus.h"
#include "rts_uvc.h"

#define UVC_GUID_REALTEK_VENDOR_CMD \
	{0x8c, 0xa7, 0x29, 0x12, 0xb4, 0x47, 0x94, 0x40, \
	 0xb0, 0xce, 0xdb, 0x07, 0x38, 0x6f, 0xb9, 0x38}

#define XU_CMD_STATUS		0x0A
#define XU_DATA_IN_OUT		0x0B

#define V4L2_CID_PRIVATE_CMD_STATUS		V4L2_CID_PRIVATE_BASE
#define V4L2_CID_PRIVATE_DATA_IN_OUT		(V4L2_CID_PRIVATE_BASE + 1)

#define DATA_CTL_LEN		8


#define MAKE_VENDOR_CMD(cmd_code, addr, len, resv) \
	{ \
	.cmd = (uint8_t)((cmd_code & 0XFF00) >> 8),\
	.subcmd = (uint8_t)((cmd_code & 0xFF)), \
	.address = {(uint8_t)(addr & 0XFF), (uint8_t)((addr & 0XFF00) >> 8)}, \
	.length = {(uint8_t)(len & 0XFF), (uint8_t)((len & 0XFF00) >> 8)}, \
	.reserved = {(uint8_t)(resv & 0XFF), (uint8_t)((resv & 0XFF00) >> 8)}, \
	}

static struct uvc_xu_control_mapping uvc_xu_ctrl_mappings[] = {
	{
		.id		= V4L2_CID_PRIVATE_CMD_STATUS,
		.name		= "Command status",
		.entity		= UVC_GUID_REALTEK_VENDOR_CMD,
		.selector	= XU_CMD_STATUS,
		.size		= 8,
		.offset		= 0,
		.v4l2_type	= V4L2_CTRL_TYPE_INTEGER,
		.data_type	= UVC_CTRL_DATA_TYPE_SIGNED,
	},
	{
		.id		= V4L2_CID_PRIVATE_DATA_IN_OUT,
		.name		= "Data in out",
		.entity		= UVC_GUID_REALTEK_VENDOR_CMD,
		.selector	= XU_DATA_IN_OUT,
		.size		= DATA_CTL_LEN,
		.offset		= 0,
		.v4l2_type	= V4L2_CTRL_TYPE_INTEGER,
		.data_type	= UVC_CTRL_DATA_TYPE_SIGNED,
	},
};

int uvc_add_xu_ctrls(int fd)
{
	int retval = 0;
	int i;
	struct v4l2_queryctrl queryctrl;
	char *ctrl_name[2] = {
		"Command status",
		"Data in out"
	};

	memset (&queryctrl, 0, sizeof (queryctrl));

	queryctrl.id = V4L2_CID_PRIVATE_CMD_STATUS;
	for (i = 0; i < 2; i++) {
		if (0 == uvc_ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)) {
			if (strcmp((char *)queryctrl.name, ctrl_name[i]) == 0)
				return 0;
		}

		queryctrl.id++;
	}

	for (i = 0; i < 2; i++) {
		retval = uvc_ioctl(fd,
			UVCIOC_CTRL_MAP, &(uvc_xu_ctrl_mappings[i]));
		if (retval) {
			fprintf(stderr, "Map ctrl %d fail!\n", i);
			return retval;
		}
	}

	return retval;
}


int uvc_query_xu_ctrl(int fd, struct uvc_xu_control_query *xctrl)
{
	return  uvc_ioctl(fd, UVCIOC_CTRL_QUERY, xctrl);
}

struct uvc_vendor_cmd {
	uint8_t cmd;
	uint8_t subcmd;
	uint8_t address[2];
	uint8_t length[2];
	uint8_t reserved[2];
};

static int xu_set_cmd(int fd, struct uvc_vendor_cmd *cmd)
{
	struct uvc_xu_control_query xctrl;

	memset(&xctrl, 0, sizeof(struct uvc_xu_control_query));

	xctrl.unit = 4;
	xctrl.selector = XU_CMD_STATUS;
	xctrl.size = 8;
	xctrl.data = (uint8_t *)cmd;
	xctrl.query = UVC_SET_CUR;

	return uvc_query_xu_ctrl(fd, &xctrl);
}

static int xu_get_status(int fd, uint16_t *status)
{
	struct uvc_xu_control_query xctrl;
	uint8_t buf[8] = {0};
	int retval;

	memset(&xctrl, 0, sizeof(struct uvc_xu_control_query));

	xctrl.unit = 4;
	xctrl.selector = XU_CMD_STATUS;
	xctrl.size = 8;
	xctrl.data = buf;
	xctrl.query = UVC_GET_CUR;

	retval = uvc_query_xu_ctrl(fd, &xctrl);
	if (retval) {
		return retval;
	}

	if (status) {
		*status = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
	}

	return 0;
}

static int xu_set_data(int fd, uint8_t *buf)
{
	struct uvc_xu_control_query xctrl;

	memset(&xctrl, 0, sizeof(struct uvc_xu_control_query));

	xctrl.unit = 4;
	xctrl.selector = XU_DATA_IN_OUT;
	xctrl.size = DATA_CTL_LEN;
	xctrl.data = buf;
	xctrl.query = UVC_SET_CUR;

	return uvc_query_xu_ctrl(fd, &xctrl);
}

static int xu_get_data(int fd, uint8_t *buf)
{
	struct uvc_xu_control_query xctrl;

	memset(&xctrl, 0, sizeof(struct uvc_xu_control_query));

	xctrl.unit = 4;
	xctrl.selector = XU_DATA_IN_OUT;
	xctrl.size = DATA_CTL_LEN;
	xctrl.data = buf;
	xctrl.query = UVC_GET_CUR;

	return uvc_query_xu_ctrl(fd, &xctrl);
}

static int xu_write_data(int fd, uint8_t *buf, int buf_len)
{
	int i;
	int retval;
	uint8_t *ptr = buf;

	for (i = 0; i < buf_len / DATA_CTL_LEN; i++) {
		retval = xu_set_data(fd, ptr);
		if (retval) {
			return retval;
		}
		ptr += DATA_CTL_LEN;
	}

	if (buf_len % DATA_CTL_LEN) {
		uint8_t tmp_buf[DATA_CTL_LEN] = {0};
		memcpy(tmp_buf, ptr, buf_len % DATA_CTL_LEN);
		retval = xu_set_data(fd, tmp_buf);
		if (retval) {
			return retval;
		}
	}

	return 0;
}

static int xu_read_data(int fd, uint8_t *buf, int buf_len)
{
	int i;
	int retval;
	uint8_t *ptr = buf;

	for (i = 0; i < buf_len / DATA_CTL_LEN; i++) {
		retval = xu_get_data(fd, ptr);
		if (retval) {
			return retval;
		}
		ptr += DATA_CTL_LEN;
	}

	if (buf_len % DATA_CTL_LEN) {
		uint8_t tmp_buf[DATA_CTL_LEN] = {0};
		retval = xu_get_data(fd, tmp_buf);
		if (retval) {
			return retval;
		}
		memcpy(ptr, tmp_buf, buf_len % DATA_CTL_LEN);
	}

	return 0;
}

static int try_uvc_exec_vendor_cmd(int fd, uint16_t cmd_code,
		uint16_t addr, uint8_t  *buf, uint16_t len, uint16_t resv)
{
	int ret = 0;
	struct uvc_vendor_cmd cmd = MAKE_VENDOR_CMD(cmd_code, addr, len, resv);

	if ((ret = xu_set_cmd(fd, &cmd)))
		return ret;

	if (cmd_code & 0x80) {
		if (cmd_code & 0x40)
			return xu_read_data(fd, buf, len);
		else
			return xu_write_data(fd, buf, len);
	}

	return 0;
}

int uvc_exec_vendor_cmd(int fd, uint16_t cmd_code,
		uint16_t addr, uint8_t  *buf, uint16_t len, uint16_t resv)
{
	int ret = 0;
	ret = try_uvc_exec_vendor_cmd(fd, cmd_code, addr, buf, len, resv);
	if (ret) {
		try_uvc_exec_vendor_cmd(fd, VC_RESET, 0, NULL, 0, 0);
		opt_error("exec vendor commond id: 0x%04x addr: 0x%04x len: %d failed ... now reset\n",
			cmd_code, addr, len);
	}

	return ret;
}

int uvc_get_cmd_status(int fd, uint16_t *status)
{
	return xu_get_status(fd, status);
}
