/*
 * svr_uvc_utils.c
 * Author: yafei meng (yafei_meng@realsil.com.cn)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <peacock.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <json-c/json.h>

#define PEACOCK_CONFIG_FN "/usr/conf/peacock.json"

static int get_msg_id(const char *path)
{

	key_t key;
	int msg = 0;
	int ret = 0;

	key = ftok(path, 0);
	if (key < 0) {
		ret = key;
		goto fail;
	}

	msg = msgget(key, IPC_CREAT | S_IRWXU | S_IRWXG | S_IRWXO);
	if (msg < 0) {
		ret = msg;
		goto fail;
	}

	return msg;

fail:
	fprintf(stderr, "%s\n", strerror(errno));
	return ret;

}

static int send_msg_to_sync(int event_type, int timeout)
{

	struct event_msg *event = NULL;
	int msg = 0;
	int ret = 0;
	int size = 0;

	msg = get_msg_id(MSG_FILE_PREFIX"/"MSG_MASTER_FILE);
	if (msg < 0) {
		fprintf(stderr, "get message id fail\n");
		ret = -errno;
		goto out;
	}

	event = calloc(1, sizeof(*event));
	if (!event) {
		fprintf(stderr, "%s:%s\n", __func__, strerror(errno));
		ret = -ENOMEM;
		goto out;
	}

	event->length = 0;
	event->mtype = MSG_TYPE_REQUEST;
	event->pid = getpid();
	event->event = event_type;

	size = sizeof(*event) - sizeof(event->mtype);
	ret = msgsnd(msg, event, size, IPC_NOWAIT);

	if (ret && EAGAIN == errno) {
		fprintf(stderr, "server is busy, try again\n");
		ret = -errno;
		goto out;
	}

	size = sizeof(*event) - sizeof(event->mtype);
	memset(event, 0, sizeof(*event));

	do {
		int recv = 0;
		recv = msgrcv(msg, event, size, getpid(),
				IPC_NOWAIT | MSG_NOERROR);
		if (recv < 0) {
			ret = -errno;
			if (EAGAIN == errno || ENOMSG == errno) {
				usleep(100 * 1000);
				timeout -= 100;
				continue;
			} else {
				fprintf(stderr, "%s:%d %s\n",
					__func__, errno, strerror(errno));
				break;
			}
		} else {
			event->length =
				recv - (sizeof(*event) - sizeof(event->mtype));
			ret = 0;
			break;
		}
	} while (timeout > 0);
out:
	if (event)
		free(event);
	return ret;

}

int update_peacock_config(char *dev_path,
			const char *new_fps,
			const char *new_resolution,
			const char *new_pixformat,
			const char *new_gop,
			const char *new_bitrate)
{
	int i = 0;
	int ret = 0;
	json_object *root_obj =  json_object_from_file(PEACOCK_CONFIG_FN);
	if (!root_obj) {
		fprintf(stderr, "cannot find file\n");
		return -1;
	}
	if (!dev_path)
		dev_path = "/dev/video1";

	json_object *arrary_profiles = NULL;
	json_object_object_get_ex(root_obj, "profiles", &arrary_profiles);

	int num = json_object_array_length(arrary_profiles);
	for (i = 0; i < num; i++) {

		json_object *pval = NULL;
		pval = json_object_array_get_idx(arrary_profiles, i);

		json_object *node = NULL;
		json_object_object_get_ex(pval, "video", &node);
		if (!strcmp(dev_path, json_object_get_string(node))) {
			if (new_fps && new_fps[0] != '\0')
				json_object_object_add(pval, "framerate",
					json_object_new_string(new_fps));
			if (new_resolution && new_resolution[0] != '\0')
				json_object_object_add(pval, "video_size",
					json_object_new_string(new_resolution));
			if (new_pixformat && new_pixformat[0] != '\0')
				json_object_object_add(pval, "pixel_format",
					json_object_new_string(new_pixformat));
			if (new_gop && new_gop[0] != '\0')
				json_object_object_add(pval, "keyframe",
					json_object_new_string(new_gop));
			if (new_bitrate && new_bitrate[0] != '\0')
				json_object_object_add(pval, "bitrate",
					json_object_new_string(new_bitrate));
		}
	}

	json_object_to_file(PEACOCK_CONFIG_FN, root_obj);

	json_object_put(root_obj);


	ret = send_msg_to_sync(EVENT_TYPE_CONFIG_CHANGED, 1000);
	if (ret)
		return ret;
	ret = send_msg_to_sync(EVENT_TYPE_PROBE, 2000);

	return ret;
}
