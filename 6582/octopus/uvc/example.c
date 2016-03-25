/*
 * client.c
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

#include "octopus.h"

static void enum_frame_interval(int fd,
		struct v4l2_fmtdesc *fmtdesc, struct v4l2_frmsizeenum *fsize)
{
	int ret;
	struct v4l2_frmivalenum fival;

	memset(&fival, 0, sizeof(fival));
	fival.index = 0;
	fival.pixel_format = fmtdesc->pixelformat;

	fival.width = fsize->discrete.width;
	fival.height = fsize->discrete.height;

	while (!(ret = v4l2_ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival))) {
		if (fival.type) {
			fprintf(stdout, "discrete frm inteval ");
			fprintf(stdout, "numerator:%d denominator:%d\n",
					fival.discrete.numerator,
					fival.discrete.denominator);
		}
		fival.index++;
	}
}


static void enum_frame_size(int fd, struct v4l2_fmtdesc *fmtdesc)
{
	int ret;
	struct v4l2_frmsizeenum fsize;

	memset(&fsize, 0, sizeof(fsize));
	fsize.index = 0;
	fsize.pixel_format = fmtdesc->pixelformat;

	while (!(ret = v4l2_ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsize))) {
		if (fsize.type) {
			fprintf(stdout, "discrete frm size:");
			fprintf(stdout, "%dx%d\n",
					fsize.discrete.width,
					fsize.discrete.height);
		}
		enum_frame_interval(fd, fmtdesc, &fsize);
		fsize.index++;
	}
}

static void enum_frame_format(int fd)
{
	struct v4l2_fmtdesc fmtdesc;
	int ret = 0;

	memset(&fmtdesc, 0, sizeof(fmtdesc));
	fmtdesc.index = 0;
	fmtdesc.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/*output all video format supported by video device*/
	do {
		ret = v4l2_ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);
		if (0 == ret) {
			fprintf(stdout, "support format: %s\n",
					fmtdesc.description);
		} else {
			fprintf(stderr, "%s\n", strerror(-ret));
			break;
		}

		enum_frame_size(fd, &fmtdesc);
		fmtdesc.index++;
	} while (0 <= ret);
}

static void demo_howo_use_lock(int fd)
{
	int ret = 0;

	ret = opt_lock_dev(fd);
	if (ret) {
		fprintf(stderr, "%s:%s \n", __func__, strerror(-ret));
		goto out;
	}

	/* do something need exclusive, etc, write flash
	 *
	 * ...
	 * ...
	 *
	 * You must unlock the device at the end.
	 */
out:
	opt_unlock_dev(fd);
}

static void demo_howto_execute_uvc_vendor_cmd(int fd)
{
	int ret = 0;
	uint8_t buf[8] = {0};

	ret = opt_lock_dev(fd);
	if (ret) {
		fprintf(stderr, "lock device fail\n");
		goto out;
	}

	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	ret = uvc_exec_vendor_cmd(fd, VC_DEV_STATUS, 0, buf, sizeof(buf), 0);

	if (!ret) {
		int i = 0;
		for (i = 0; i < sizeof(buf); i++)
			fprintf(stdout, "0x%2x ", buf[i]);
		fprintf(stdout, "\n");
	}

out:
	opt_unlock_dev(fd);
}

int main(int argc, char **argv)
{
	int fd  = 0;

	fd = opt_open_dev("/dev/video0");
	if (fd < 0)
		goto out;

	demo_howo_use_lock(fd);

	enum_frame_format(fd);

	demo_howto_execute_uvc_vendor_cmd(fd);

out:
	if (fd > 0)
		opt_close_dev(fd);

	return 0;
}
