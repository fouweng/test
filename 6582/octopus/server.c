/*
 * server.c
 * Author: Tony Nie (tony_nie@realsil.com.cn)
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>


#include "uvcvideo.h"
#include "utils.h"
#include <hw_control.h>
#include "octopus.h"
#include "internal.h"

#define DOMAIN_TOKEN_LEN (64)
#define MAX_PATH_LEN (128)
#define SHARE_LIB_TMP_PATH "/usr/lib/octopus"
static struct list_head msg_queue_list;

static struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"daemon", no_argument, NULL, 'D'},
	{"log_level", required_argument, NULL, 'v'},
	{"domain", required_argument, NULL, 'f'},
	{0, 0, 0, 0},
};

#define VERSION "0.1"
static void print_usage()
{
	fprintf(stdout, "octopus-%s - ISP controller hub.\n", VERSION);
	fprintf(stdout, "build at: %s %s\n", __TIME__, __DATE__);
	fprintf(stdout, "usage:\n");
	fprintf(stdout, "\t-h | --help   show this help and exit\n");
	fprintf(stdout, "\t-f | --token  domain lib to be loaded\n");
	fprintf(stdout, "\t-D | --daemon run octopus as daemon\n");
	fprintf(stdout, "\t-v | --log_level <level> set log level\n");

}

static int g_run_as_daemon = 0;
static char g_domain_token[DOMAIN_TOKEN_LEN] = {0};
static void *g_shared_lib_handle;

typedef int(*opt_fn_init)(struct msg_queue *mq, const char *dev);
typedef int(*opt_fn_handler)(struct msg_queue *mq, struct cmd_msg *cmd);
typedef void(*opt_fn_release)(struct msg_queue *mq);

static opt_fn_init g_p_func_init;
static opt_fn_handler g_p_func_handler;
static opt_fn_release g_p_func_release;

static int parse_arg(int argc, char **argv)
{
	int ret = 0;
	char c = '0';

	while ((c =
		getopt_long(argc, argv, "-:v:f:Dh", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			ret = -1;
			break;
		case 'D':
			g_run_as_daemon = 1;
			break;
		case 'v':
			opt_log_level = strtoll(optarg, NULL, 0);
			break;
		case 'f':
			if (strlen(optarg) >= sizeof(g_domain_token)) {
				opt_error("domain token too long\n");
				exit(1);
			}
			strncpy(g_domain_token, optarg, sizeof(g_domain_token));
			g_domain_token[sizeof(g_domain_token) - 1] = '\0';
			break;
		case ':':
			fprintf(stdout, "%c require argument\n", optopt);
			ret = -1;
			break;
		case '?':
			fprintf(stdout, "%c invalid argument\n", optopt);
			ret = -1;
			break;
		default:
			break;
		}
	}

	return ret;
}


#define IPC_INFO 3

static void mq_put(struct msg_queue *mq)
{
	mq->ref--;
}

static void mq_get(struct msg_queue *mq)
{
	mq->ref++;
}

static struct msg_queue *mq_get_mq_by_name(struct list_head *header,
		const char *name)
{
	struct list_head *pos;

	list_for_each(pos, header) {
		struct msg_queue *mq = list_entry(pos, struct msg_queue, next);
		if (strcmp(mq->msg_file, name) == 0)
			return mq;
	}

	return NULL;
}

void get_msg_property(int msgid)
{
	struct msqid_ds msqid_ds;
	struct msginfo msginfo;
	int ret = 0;

	ret = msgctl(msgid, IPC_STAT, &msqid_ds);
	if (ret)
		goto out;

	ret = msgctl(msgid, IPC_INFO, (void *) &msginfo);
	if (ret)
		goto out;

	opt_fine("key:%d uid:%d gid:%d cuid:%d guid:%u mode:%d seq:%d\n",
			msqid_ds.msg_perm.__key,
			msqid_ds.msg_perm.uid,
			msqid_ds.msg_perm.gid,
			msqid_ds.msg_perm.cuid,
			msqid_ds.msg_perm.cgid,
			msqid_ds.msg_perm.mode,
			msqid_ds.msg_perm.__seq);

	opt_fine("msg_stime:%d msg_rtime:%d msg_ctime:%d "
			"__msg_cbytes:%d msg_qnum:%d msg_qbytes:%d "
			"msg_lspid:%d msg_lrpid:%d\n",
			(int) msqid_ds.msg_stime,
			(int) msqid_ds.msg_rtime,
			(int) msqid_ds.msg_ctime,
			(int) msqid_ds.__msg_cbytes,
			(int) msqid_ds.msg_qnum,
			(int) msqid_ds.msg_qbytes,
			msqid_ds.msg_lspid,
			msqid_ds.msg_lrpid);

	opt_fine("msgpool:%d msgmap:%d msgmax:%d msgmnb:%d msgmni:%d "
			"msgssz:%d msgtql:%d msgseg:%d\n",
			msginfo.msgpool,
			msginfo.msgmap,
			msginfo.msgmax,
			msginfo.msgmnb,
			msginfo.msgmni,
			msginfo.msgssz,
			msginfo.msgtql,
			msginfo.msgseg);

out:
	if (ret) {
		opt_error("errno %d: %s\n", errno, strerror(errno));
		return;
	}

	return;
}

int test_1(int argc, char **argv)
{
	struct msgbuf *buf = NULL;
	int ret = 0;
	key_t key;
	int msg = 0;

	ret = parse_arg(argc, argv);
	if (ret) {
		print_usage();
		exit(1);
	}

	ret = gen_msg_file(MSG_MASTER_FILE);
	if (ret)
		exit(1);

	key = ftok(MSG_MASTER_FILE, 0);
	if (key < 0) {
		ret = key;
		goto out;
	}

	msg = msgget(key, IPC_CREAT | S_IRWXU | S_IRWXG | S_IRWXO);
	if (msg < 0) {
		ret = msg;
		goto out;
	}

	get_msg_property(msg);

	buf = malloc(sizeof(*buf) + MSG_BUF_SIZE);
	if (!buf) {
		ret = -ENOMEM;
		goto out;
	}

	do {
		ssize_t size = msgrcv(msg, buf, MSG_BUF_SIZE, 0, 0);
		if (size > 0) {
			opt_fine("mtype:%ld mtext:%s\n",
					buf->mtype, buf->mtext);
		} else {
			opt_error("errno:%d %s\n",
					errno, strerror(errno));
		}
		sleep(1);
	} while (1);

out:
	if (ret)
		opt_error("errno:%d %s\n", errno, strerror(errno));
	if (buf)
		free(buf);
	return ret;
}

static int process_ctl_cmd_lock(struct msg_queue *mq, struct cmd_msg *cmd)
{
	int ret = 0;
	int msg = mq->msg;

	mq->locked = 1;
	mq->pid = cmd->pid;

	cmd->mtype  = cmd->pid;
	cmd->domain = cmd->domain;
	cmd->status = OPT_STATUS_OK;

	ret = msgsnd(msg, cmd, sizeof(*cmd), IPC_NOWAIT);

	return ret;
}

static int process_ctl_cmd_unlock(struct msg_queue *mq, struct cmd_msg *cmd)
{
	int ret = 0;
	int msg = mq->msg;

	mq->locked = 0;
	mq->pid = 0;

	cmd->mtype  = cmd->pid;
	cmd->domain = cmd->domain;
	cmd->status = OPT_STATUS_OK;

	ret = msgsnd(msg, cmd, sizeof(*cmd), IPC_NOWAIT);

	return ret;
}

static int process_ctl_cmd_unknown(struct msg_queue *mq,  struct cmd_msg *cmd)
{
	int ret = 0;
	int msg = mq->msg;

	cmd->mtype = cmd->pid;
	cmd->status = OPT_STATUS_UNKNOWN_CMD;

	ret = msgsnd(msg, cmd, sizeof(*cmd), IPC_NOWAIT);

	return ret;
}

static int process_ctl_cmd_del_mq(struct msg_queue *mq, struct cmd_msg *cmd)
{
	int ret = 0;
	int msg = mq->msg;

	cmd->mtype = cmd->pid;
	cmd->status = OPT_STATUS_OK;


	mq_put(mq);
	ret = msgsnd(msg, cmd, sizeof(*cmd), IPC_NOWAIT);

	return ret;
}

static int process_ctl_cmd_new_mq(struct msg_queue *mq,  struct cmd_msg *cmd)
{
	struct msg_queue *slave = NULL;
	struct ctl_msg_p *ctl = (struct ctl_msg_p *) cmd->priv;
	int ret = 0;
	int msg = mq->msg;

	cmd->mtype = cmd->pid;
	cmd->status = OPT_STATUS_OK;


	slave = mq_get_mq_by_name(&msg_queue_list, ctl->msg_file);
	if (slave) {
		mq_get(slave);
	} else {
		struct msg_queue *mq = calloc(1, sizeof(*mq));
		int msg = opt_get_msg_id(ctl->msg_file);

		if (mq && msg > 0) {
			INIT_LIST_HEAD(&mq->next);
			list_add_tail(&mq->next, &msg_queue_list);
			mq->msg = msg;
			mq->locked = 0;
			mq->ref = 1;
			mq->master = 0;
			strncpy(mq->msg_file,
					ctl->msg_file, sizeof(mq->msg_file));
			mq->msg_file[sizeof(mq->msg_file) - 1] = '\0';
			cmd->status = OPT_STATUS_OK;

			mq->init = g_p_func_init;
			mq->handler = g_p_func_handler;
			mq->release = g_p_func_release;

			if (mq->init) {
				ret = mq->init(mq, ctl->dev_name);
				if (ret) {
					opt_error("init msgq failed\n");
					return ret;
				}
			}

		} else {
			cmd->status = errno;

			if (mq)
				free(mq);
			opt_error("%s\n", strerror(errno));
		}
	}

	ret = msgsnd(msg, cmd, sizeof(*cmd), IPC_NOWAIT);

	return ret;
}

static int scan_master_queue(struct msg_queue *mq)
{
	struct cmd_msg *cmd = NULL;
	struct ctl_msg_p *ctl = NULL;
	int msg = mq->msg;
	int size = 0;
	int ret = 0;

	ret = opt_alloc_cmd_message(&cmd, MSG_BUF_SIZE);
	if (ret)
		goto out;

	size = sizeof(*cmd) + cmd->length - sizeof(cmd->mtype);
	ret = msgrcv(msg, cmd, size, MSG_TYPE_REQUEST, IPC_NOWAIT);
	if (ret <= 0)
		goto out;

	dump_cmd_msg(cmd);

	ctl = (struct ctl_msg_p *) cmd->priv;

	switch (ctl->cmd) {
	case CMD_CTL_NEW_MQ:
		ret = process_ctl_cmd_new_mq(mq, cmd);
		break;
	default:
		ret = process_ctl_cmd_unknown(mq, cmd);
		break;
	}

out:
	if (cmd)
		opt_free_cmd_msg(&cmd);
	return 0;
}

static int process_busy_status(struct msg_queue *mq, struct cmd_msg *cmd)
{
	int ret = 0;
	int msg = mq->msg;

	cmd->mtype = cmd->pid;
	cmd->domain = cmd->domain;
	cmd->status = OPT_STATUS_BUSY;

	ret = msgsnd(msg, cmd, sizeof(*cmd), IPC_NOWAIT);

	return ret;

}

static int process_ctl_domain(struct msg_queue *mq, struct cmd_msg *cmd)
{
	struct ctl_msg_p *ctl = (struct ctl_msg_p *) cmd->priv;
	int ret = 0;

	switch (ctl->cmd) {
	case CMD_CTL_DEL_MQ:
		ret = process_ctl_cmd_del_mq(mq, cmd);
		break;
	case CMD_CTL_LOCK_MQ:
		ret = process_ctl_cmd_lock(mq, cmd);
		break;
	case CMD_CTL_UNLOCK_MQ:
		ret = process_ctl_cmd_unlock(mq, cmd);
		break;
	default:
		ret = process_ctl_cmd_unknown(mq, cmd);
		break;
	}

	return ret;
}

static int cmd_need_exclusion(struct cmd_msg *cmd)
{
	struct ctl_msg_p *ctl = (struct ctl_msg_p *) cmd->priv;

	if (CMD_DOMAIN_CTL == cmd->domain && CMD_CTL_DEL_MQ == ctl->cmd)
		return 0;

	return 1;
}

static int scan_slave_queue(struct msg_queue *mq)
{
	struct cmd_msg *cmd = NULL;
	int msg = mq->msg;
	int size = 0;
	int ret = 0;

	ret = opt_alloc_cmd_message(&cmd, MSG_BUF_SIZE);
	if (ret)
		goto out;

	size = sizeof(*cmd) + cmd->length - sizeof(cmd->mtype);
	ret = msgrcv(msg, cmd, size, MSG_TYPE_REQUEST, IPC_NOWAIT);
	if (ret <= 0)
		goto out;
	cmd->length = ret - (sizeof(*cmd) - sizeof(cmd->mtype));
	cmd->length = (cmd->length > 0) ? cmd->length : 0;

	dump_cmd_msg(cmd);

	if (mq->locked && cmd->pid != mq->pid && cmd_need_exclusion(cmd)) {
		ret = process_busy_status(mq, cmd);
		goto out;
	}

	switch (cmd->domain) {
	case CMD_DOMAIN_CTL:
		ret = process_ctl_domain(mq, cmd);
		break;
	default:
		if (mq->handler)
			ret = mq->handler(mq, cmd);
		break;
	}
out:
	if (cmd)
		opt_free_cmd_msg(&cmd);

	return ret;
}

static void process_sprite_guys(struct msg_queue *mq)
{
	if (!mq->locked)
		return;

	if (kill(mq->pid, 0)) {
		opt_warning("pid:%d had exited, but missed unlocking the mq\n",
				mq->pid);
		mq->locked = 0;
		mq->pid = 0;
	}
}

static void scan_msg_queue_list(struct list_head *header)
{
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, header) {
		struct msg_queue *mq = list_entry(pos, struct msg_queue, next);
		if (mq->ref <= 0) {
			list_del(pos);
			if (mq->release)
				mq->release(mq);
			free(mq);
			opt_fine("delete mq: %s\n", mq->msg_file);
		} else {
			if (mq->master)
				scan_master_queue(mq);
			else
				scan_slave_queue(mq);
			process_sprite_guys(mq);
		}
	}
}

static void  free_msg_queue_list(struct list_head *header)
{
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, header) {
		struct msg_queue *mq = list_entry(pos, struct msg_queue, next);
		list_del(pos);
		free(mq);
	}
}

static int init_msq_queue_list(struct list_head *header, const char *domain)
{
	struct msg_queue *mq = calloc(1, sizeof(*mq));
	int msg = 0;
	char tmp_buf[MAX_PATH_LEN] = {0};

	INIT_LIST_HEAD(header);

	if (!mq) {
		opt_fine("%s:%s\n", __func__, strerror(errno));
		return -1;
	}
	sprintf(tmp_buf, "%s/%s_%s", MSG_FILE_PREFIX, domain, MSG_MASTER_FILE);
	opt_log(OPT_LOG_INFO, "init msg queue name is %s\n", tmp_buf);
	msg = opt_get_msg_id(tmp_buf);
	if (msg < 0) {
		free(mq);
		mq = NULL;
		goto fail;
	}

	list_add_tail(&mq->next, header);
	mq->msg = msg;
	mq->locked = 0;
	mq->ref = 1;
	mq->master = 1;
	strncpy(mq->msg_file, tmp_buf, sizeof(mq->msg_file));
	mq->msg_file[sizeof(mq->msg_file) - 1] = '\0';
	mq->init = NULL;
	mq->release = NULL;
	mq->handler = NULL;

	/* TODO: check return value */
	return 0;
fail:
	return -1;
}

static int get_shared_lib_func_addr(const char *domain_token)
{
	char *error = NULL;
	char shared_lib_name[MAX_PATH_LEN] = {0};
	char symbol_init[MAX_PATH_LEN] = {0};
	char symbol_handler[MAX_PATH_LEN] = {0};
	char symbol_release[MAX_PATH_LEN] = {0};

	/*sprintf(shared_lib_name, "/usr/lib/octopus/opt_%s.so", domain_token);*/
	sprintf(shared_lib_name, "%s/opt_%s.so", SHARE_LIB_TMP_PATH, domain_token);
	sprintf(symbol_init, "rts_%s_init", domain_token);
	sprintf(symbol_handler, "rts_%s_handler", domain_token);
	sprintf(symbol_release, "rts_%s_release", domain_token);
	opt_log(OPT_LOG_INFO, "share lib name is %s\n", shared_lib_name);
	opt_log(OPT_LOG_INFO, "share init name is %s\n", symbol_init);
	opt_log(OPT_LOG_INFO, "share handler name is %s\n", symbol_handler);
	opt_log(OPT_LOG_INFO, "share release name is %s\n", symbol_release);

	g_shared_lib_handle = dlopen(shared_lib_name, RTLD_NOW);
	if (!g_shared_lib_handle) {
		opt_error("%s\n", dlerror());
		exit(1);
	}

	g_p_func_init = (opt_fn_init)dlsym(g_shared_lib_handle, symbol_init);
	error = dlerror();
	if (error != NULL) {
		opt_error("%s\n", error);
		dlclose(g_shared_lib_handle);
		exit(1);
	}

	g_p_func_handler = (opt_fn_handler)dlsym(g_shared_lib_handle,
			symbol_handler);
	error = dlerror();
	if (error != NULL) {
		opt_error("%s\n", error);
		dlclose(g_shared_lib_handle);
		exit(1);
	}

	g_p_func_release = (opt_fn_release)dlsym(g_shared_lib_handle, symbol_release);
	error = dlerror();
	if (error != NULL) {
		opt_error("%s\n", error);
		dlclose(g_shared_lib_handle);
		exit(1);
	}
	opt_log(OPT_LOG_INFO, "share init addr is 0x%04x\n", g_p_func_init);
	opt_log(OPT_LOG_INFO, "share handler addr is 0x%04x\n", g_p_func_handler);
	opt_log(OPT_LOG_INFO, "share release addr is 0x%04x\n", g_p_func_release);


	return 0;
}


static int do_job(const char *domain)
{
	int ret = 0;
	ret = get_shared_lib_func_addr(domain);
	if (ret)
		goto out;

	ret = init_msq_queue_list(&msg_queue_list, domain);
	if (ret)
		goto out;

	do {
		scan_msg_queue_list(&msg_queue_list);
		usleep(100);
	} while (1);

out:
	if (ret)
		opt_error("errno:%d %s\n", errno, strerror(errno));

	dlclose(g_shared_lib_handle);
	free_msg_queue_list(&msg_queue_list);

	return ret;
}

int main(int argc, char **argv)
{
	struct sigaction sg;
	struct rlimit rl;
	int ret = 0;
	pid_t pid = 0;

	ret = parse_arg(argc, argv);
	if (ret) {
		print_usage();
		exit(1);
	}

	if (!g_run_as_daemon)
		return do_job(g_domain_token);

	umask(0);

	ret = getrlimit(RLIMIT_NOFILE, &rl);
	if (ret < 0)
		goto out;

	pid = fork();

	if (pid < 0)
		goto out;
	else if (pid > 0) /* parend */
		exit(0);

	setsid();

	sg.sa_handler = SIG_IGN;
	sigemptyset(&sg.sa_mask);
	sg.sa_flags = 0;
	ret = sigaction(SIGHUP, &sg, NULL);
	if (ret < 0)
		goto out;

	pid = fork();
	if (pid < 0)
		goto out;
	else if (pid > 0) /* parend */
		exit(0);

	ret = chdir("/");
	if (ret < 0)
		goto out;

	if (rl.rlim_max == RLIM_INFINITY)
		rl.rlim_max = 1024;

	do {
		int i = 0;
		int fd0, fd1, fd2;
		for (i = 0; i < rl.rlim_max; i++)
			close(i);

		fd0 = open("/dev/null", O_RDWR);
		fd1 = dup(0);
		fd2 = dup(1);

		openlog(argv[0], LOG_CONS | LOG_PID, LOG_DAEMON);

		if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
			opt_error("unexpedted file descriptors %d %d %d\n",
					fd0, fd1, fd2);
			opt_error("%s\n", strerror(errno));
			exit(1);
		}
	} while (0);


	return do_job(g_domain_token);

out:
	if (ret)
		opt_error("%s\n", strerror(errno));

	return ret;
}

