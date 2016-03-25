/*
 * v4l2_client.c
 * Author: Tony Nie (tony_nie@realsil.com.cn)
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "uvcvideo.h"
#include "octopus.h"
#include "hw_control.h"
#include "uvc_internal.h"
#include "internal.h"

static int opt_send_v4l2_cmd(int msg, struct v4l2_msg_p *v4l2,
		char *data, int len, int *status, int timeout)
{
	struct cmd_msg *cmd = NULL;
	int ret = 0;
	int size = (v4l2->request >> _IOC_SIZESHIFT) & _IOC_SIZEMASK;

	ret = opt_alloc_cmd_message(&cmd, sizeof(*v4l2) + len);
	if (!cmd)
		goto out;

	cmd->mtype = MSG_TYPE_REQUEST;
	cmd->domain = CMD_DOMAIN_V4L2;
	cmd->pid = getpid();
	memcpy(cmd->priv, v4l2, sizeof(*v4l2));
	memcpy(cmd->priv + sizeof(*v4l2), data, len);

	ret = opt_send_cmd_msg(msg, cmd, timeout);
	if (ret)
		goto out;

	*status = cmd->status;
	if (OPT_STATUS_OK != *status)
		goto out;

	/* TODO: copy data when read from devcie */
	memcpy(v4l2->args, ((struct v4l2_msg_p *) (cmd->priv))->args, size);
	memcpy(data, cmd->priv + sizeof(*v4l2), len);

out:
	if (cmd)
		opt_free_cmd_msg(&cmd);

	return ret;
}

static int invalid_request(int request, void *arg)
{
	int ret = 0;

	switch (request) {
	case VIDIOC_G_EXT_CTRLS:
	case VIDIOC_S_EXT_CTRLS:
	case VIDIOC_TRY_EXT_CTRLS:
	case VIDIOC_G_ENC_INDEX:
		ret = 1;
		break;
	case VIDIOC_G_FMT:
	case VIDIOC_S_FMT:
	case VIDIOC_TRY_FMT:
	{
		struct v4l2_format *f = arg;
		if (V4L2_BUF_TYPE_VIDEO_OVERLAY == f->type)
			ret = 1;
		break;
	}

	/* case VIDIOC_CREATE_BUFS:
	{
		struct v4l2_create_buffers *b = arg;
		if (V4L2_BUF_TYPE_VIDEO_OVERLAY == b->format.type)
			ret = 1;
		break;
	} */

	case VIDIOC_QUERYBUF:
	case VIDIOC_QBUF:
	case VIDIOC_DQBUF:

	/* case VIDIOC_PREPARE_BUF:
	{
	      struct v4l2_buffer *b = arg;
	      if (V4L2_MEMORY_OVERLAY == b->memory)
		      ret = 1;
	      break;
	} */

	default:
		ret = 0;
		break;
	}

	return ret;
}

int uvc_ioctl(int msg, uint32_t request, void *arg)
{
	struct v4l2_msg_p *v4l2 = NULL;
	int size =  (request >> _IOC_SIZESHIFT) & (_IOC_SIZEMASK);
	int ret = 0;
	int status = 0;

	if (size > sizeof(v4l2->args)) {
		opt_error("the size of v4l2 param is too big\n");
		return -EINVAL;
	}

	if (invalid_request(request, arg)) {
		opt_error("donot support request:%d\n",
			(request >> _IOC_NRSHIFT) & (_IOC_NRMASK));
		return -EINVAL;
	}

	v4l2 = calloc(1, sizeof(*v4l2));
	if (!v4l2) {
		ret = -ENOMEM;
		goto out;
	}

	v4l2->request = request;
	memcpy(v4l2->args, arg, size);

	if (UVCIOC_CTRL_QUERY == request) {
		struct uvc_xu_control_query *query = arg;
		ret = opt_send_v4l2_cmd(msg, v4l2,
			(char *)query->data, query->size, &status, 10000);
	} else {
		ret = opt_send_v4l2_cmd(msg, v4l2, NULL, 0, &status, 10000);
	}

	if (!ret)
		ret = status;

	if (OPT_STATUS_OK != status)
		goto out;

	if (UVCIOC_CTRL_QUERY == request) {
		struct uvc_xu_control_query *query = arg;
		__u8 *data = query->data;
		memcpy(arg, v4l2->args, size);
		query->data = data;
	} else {
		memcpy(arg, v4l2->args, size);
	}

out:
	if (v4l2)
		free(v4l2);

	return ret;
}
