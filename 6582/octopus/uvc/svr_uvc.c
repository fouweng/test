/*
 * svr_uvc.c
 * Author: Tony Nie (tony_nie@realsil.com.cn)
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <linux/videodev2.h>

#include "hw_control.h"
#include "uvcvideo.h"
#include "uvc_internal.h"

#define V4L2_PIX_FMT_H264     v4l2_fourcc('H', '2', '6', '4') /* H264 with start codes */
#define PATH_LEN (128)
struct uvc_data {
	int fd;
	char *dev_name;
};

static void release_uvc_data(struct uvc_data **_data)
{
	struct uvc_data *data = NULL;

	assert(_data != NULL);
	assert(*_data != NULL);

	data = *_data;

	if (data->fd > 0) {
		close(data->fd);
		data->fd = -1;
	}

	if (data->dev_name) {
		free(data->dev_name);
		data->dev_name = NULL;
	}

	free(data);
	*_data = NULL;
}

void rts_uvc_release(struct msg_queue *mq)
{
	if (mq->private_data)
		release_uvc_data((struct uvc_data **) &mq->private_data);
}

int rts_uvc_init(struct msg_queue *mq, const char *dev)
{
	struct uvc_data *data = NULL;
	int ret = 0;

	assert(dev != NULL && mq != NULL);

	data = calloc(1, sizeof(*data));
	if (!data) {
		ret = -ENOMEM;
		goto out;
	}

	data->fd = open(dev, O_RDWR);
	if (data->fd < 0) {
		ret = -errno;
		goto out;
	}
	data->dev_name = calloc(1, PATH_LEN);
	memset(data->dev_name, 0, PATH_LEN);
	strncpy(data->dev_name, dev, PATH_LEN);

	mq->private_data = data;
out:
	if (ret) {
		if (data)
			release_uvc_data(&data);
	}

	return ret;
}

int rts_uvc_handler(struct msg_queue *mq, struct cmd_msg *cmd)
{
	struct v4l2_msg_p *v4l2 = (struct v4l2_msg_p *) cmd->priv;
	struct uvc_data *data = (struct uvc_data *) mq->private_data;
	int ret = 0;

	if (UVCIOC_CTRL_QUERY == v4l2->request) {
		struct uvc_xu_control_query *query =
			(struct uvc_xu_control_query *) v4l2->args;
		query->data = cmd->priv + sizeof(*v4l2);
		ret = ioctl(data->fd, v4l2->request, query);
	} else if (VIDIOC_S_FMT == v4l2->request) {
		struct v4l2_format *stream_pxl_format =
			(struct v4l2_format *)v4l2->args;
		char resolution_buf[32] = {0};
		char pixformat_buf[16] = {0};
		if (V4L2_PIX_FMT_H264 == stream_pxl_format->fmt.pix.pixelformat)
			sprintf(pixformat_buf, "%s", "h264");
		else if (V4L2_PIX_FMT_NV12 == stream_pxl_format->fmt.pix.pixelformat)
			sprintf(pixformat_buf, "%s", "nv12");
		else if (V4L2_PIX_FMT_MJPEG == stream_pxl_format->fmt.pix.pixelformat)
			sprintf(pixformat_buf, "%s", "mjpg");

		sprintf(resolution_buf,
			"%dx%d", stream_pxl_format->fmt.pix.width,
			stream_pxl_format->fmt.pix.height);
		update_peacock_config(data->dev_name,
			NULL, resolution_buf, pixformat_buf, NULL, NULL);
	} else if (VIDIOC_S_PARM  == v4l2->request) {
		struct v4l2_streamparm *stream_fps =
			(struct v4l2_streamparm *)v4l2->args;
		char fps_buf[16] = {0};
		sprintf(fps_buf, "%d",
			stream_fps->parm.capture.timeperframe.denominator);
		update_peacock_config(data->dev_name,
			fps_buf, NULL, NULL, NULL, NULL);

	} else {
		ret = ioctl(data->fd, v4l2->request, v4l2->args);
	}

	if (0 == ret) {
		int size = sizeof(*cmd) + cmd->length - sizeof(cmd->mtype);

		cmd->mtype = cmd->pid;
		cmd->status = OPT_STATUS_OK;
		ret = msgsnd(mq->msg, cmd, size, IPC_NOWAIT);
	} else {
		int fail = -errno;

		opt_error("%s\n", strerror(errno));

		cmd->mtype = cmd->pid;
		cmd->status = fail;

		ret = msgsnd(mq->msg, cmd, sizeof(*cmd), IPC_NOWAIT);
	}

	return ret;
}
