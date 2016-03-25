/*
 * octopus.c
 * Author: Tony Nie (tony_nie@realsil.com.cn)
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <hw_control.h>
#include "utils.h"
#include "octopus.h"

int opt_get_msg_id(const char *path)
{
	key_t key;
	int msg = 0;
	int ret = 0;

	ret = gen_msg_file(path);
	if (ret < 0) {
		opt_error("gen msg file failed ret = %d\n", ret);
		goto fail;
	}

	key = ftok(path, 0);
	if (key < 0) {
		ret = key;
		opt_error("ftok failed ret = %d\n", ret);
		goto fail;
	}

	msg = msgget(key, IPC_CREAT | S_IRWXU | S_IRWXG | S_IRWXO);
	if (msg < 0) {
		ret = msg;
		opt_error("msgget failed, ret = %d\n", ret);
		goto fail;
	}

	return msg;

fail:
	opt_error("%d\n", errno);
	opt_error("%s\n", strerror(errno));
	return ret;
}

int opt_alloc_cmd_message(struct cmd_msg **__cmd, int len)
{
	struct cmd_msg *cmd = NULL;

	assert(__cmd != NULL);

	cmd = *__cmd;
	if (cmd == NULL) {
		cmd = calloc(1, sizeof(*cmd) + len);
		if (cmd == NULL) {
			opt_error("%s\n", strerror(errno));
			return -ENOMEM;
		}
		*__cmd = cmd;
	}

	cmd->length = len;

	return 0;
}

void opt_free_cmd_msg(struct cmd_msg **cmd)
{
	if (*cmd != NULL) {
		free(*cmd);
		*cmd = NULL;
	}
}

/* opt_send_cmd_msg - put command message  to message queue
 *
 * @msg: message queue identifier
 * @cmd: the content of message
 * @timeout: in millisecond
 *
 *
 */
int opt_send_cmd_msg(int msg, struct cmd_msg *cmd, int _timeout)
{
	int ret = 0;
	int elapsed = 0;
	int timeout = _timeout / 2;
	int data_len = cmd->length;

	do {
		int size = sizeof(*cmd) + data_len - sizeof(cmd->mtype);

		ret = msgsnd(msg, cmd, size, IPC_NOWAIT);
		if (0 == ret)
			break;

		ret = -errno;
		if (errno == EAGAIN) {
			elapsed += 5;
			usleep(5 * 1000);
		} else {
			opt_error("%s\n", strerror(errno));
			goto out;
		}

	} while (elapsed < timeout);

	do {
		int size = sizeof(*cmd) + data_len - sizeof(cmd->mtype);
		int recv = 0;

		memset(cmd, 0, sizeof(*cmd));
		recv = msgrcv(msg, cmd, size, getpid(),
				IPC_NOWAIT | MSG_NOERROR);
		if (recv < 0) {
			ret = -errno;
			if (errno == EAGAIN || errno == ENOMSG) {
				elapsed += 5;
				usleep(5 * 1000);
			} else {
				opt_error("%s:%s\n", __func__,
						strerror(errno));
				break;
			}
		} else {
			cmd->length =
				recv - (sizeof(*cmd) - sizeof(cmd->mtype));
			ret = 0;
			break;
		}
	} while (elapsed < timeout);

out:
	return ret;
}

static int get_msg_file(char *buf, int size, const char *name)
{
	char *pos = strrchr(name, '/');

	assert(name != NULL);

	if (pos) {
		pos++;
		snprintf(buf, size, "%s/%s.msg", MSG_FILE_PREFIX, pos);
	} else {
		snprintf(buf, size, "%s/%s.msg", MSG_FILE_PREFIX, name);
	}

	return 0;
}

static int opt_send_ctl_cmd(int msg, struct ctl_msg_p *ctl,
		char *data, int len, int *status, int timeout)
{
	struct cmd_msg *cmd = NULL;
	int ret = 0;

	ret = opt_alloc_cmd_message(&cmd, sizeof(*ctl) + len);
	if (!cmd)
		goto out;

	cmd->mtype = MSG_TYPE_REQUEST;
	cmd->domain = CMD_DOMAIN_CTL;
	cmd->pid = getpid();
	memcpy(cmd->priv, ctl, sizeof(*ctl));
	memcpy(cmd->priv + sizeof(*ctl), data, len);

	ret = opt_send_cmd_msg(msg, cmd, timeout);
	if (ret)
		goto out;

	*status = cmd->status;

out:
	if (cmd)
		opt_free_cmd_msg(&cmd);

	return ret;
}

static int get_domain_by_name(const char *dev_name, char *domain_name)
{
	int ret = 0;
	if (!domain_name || !dev_name)
		return -1;

	if (strstr(dev_name, "video"))
		strcpy(domain_name, "uvc");
	else if (strstr(dev_name, "/dev/ttyS0"))
		strncpy(domain_name, "io", 3);
	else
		ret = -2;
	return ret;
}

/* library interface */
int opt_open_dev(const char *name)
{
	struct ctl_msg_p ctl = {0};
	char msg_file[MAX_MSG_PATH_LEN] = {0};
	char domain_name[MAX_MSG_PATH_LEN] = {0};
	char master_q_name[MAX_MSG_PATH_LEN] = {0};
	int master = 0;
	int slave = 0;
	int ret = 0;
	int status = OPT_STATUS_OK;

	get_msg_file(msg_file, sizeof(msg_file), name);

	slave = opt_get_msg_id(msg_file);
	if (slave < 0) {
		opt_error("get slave message queue identifier fail\n");
		goto fail;
	}

	ret = get_domain_by_name(name, domain_name);
	if (ret) {
		opt_error("get domain fail\n");
		goto fail;
	}

	snprintf(master_q_name, MAX_MSG_PATH_LEN, "%s/%s_%s",
		MSG_FILE_PREFIX, domain_name, MSG_MASTER_FILE);
	master = opt_get_msg_id(master_q_name);
	if (master < 0) {
		opt_error("get master message queue identifier fail\n");
		goto fail;
	}

	ctl.cmd = CMD_CTL_NEW_MQ;
	snprintf(ctl.msg_file, sizeof(ctl.msg_file), "%s", msg_file);
	snprintf(ctl.dev_name, sizeof(ctl.dev_name), "%s", name);

	ret = opt_send_ctl_cmd(master, &ctl, NULL, 0, &status, 1000);
	if (ret) {
		opt_error("send CTL CMD CMD_CTL_NEW_MQ fail\n");
		goto fail;
	}

	return slave;

fail:
	del_msg_file(msg_file);
	return -1;
}

void opt_close_dev(int msg)
{
	struct ctl_msg_p ctl = {0};
	int ret = 0;
	int status = OPT_STATUS_OK;


	memset(&ctl, 0, sizeof(ctl));
	ctl.cmd = CMD_CTL_DEL_MQ;

	ret = opt_send_ctl_cmd(msg, &ctl, NULL, 0, &status, 1000);
	if (ret)
		opt_error("send CTL CMD CMD_CTL_DEL_MQ fail\n");
}

int opt_lock_dev(int msg)
{
	struct ctl_msg_p ctl = {0};
	int ret = 0;
	int status = OPT_STATUS_OK;


	ctl.cmd = CMD_CTL_LOCK_MQ;

	ret = opt_send_ctl_cmd(msg, &ctl, NULL, 0, &status, 1000);
	if (!ret && status != OPT_STATUS_OK) {
		opt_error("lock device fail\n");
		return -status;
	}

	return ret;
}

int opt_trylock_dev(int msg, int timeout)
{
	int ret = 0;

	do {
		ret = opt_lock_dev(msg);
		if (!ret)
			break;
		usleep(1000);
		timeout -= 1;
	} while (timeout > 0);

	return ret;
}

int opt_unlock_dev(int msg)
{
	struct ctl_msg_p ctl = {0};
	int ret = 0;
	int status = OPT_STATUS_OK;


	ctl.cmd = CMD_CTL_UNLOCK_MQ;

	ret = opt_send_ctl_cmd(msg, &ctl, NULL, 0, &status, 1000);
	if (!ret && status != OPT_STATUS_OK) {
		opt_error("unlock device fail\n");
		return -status;
	}

	return ret;
}

