/*************************************************************************
    > File Name: rts_io.c
    > Author: steven_feng
    > Mail: steven_feng@realsil.com.cn
    > Created Time: 2015/03/27
 ************************************************************************/

#include<stdio.h>
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
#include <termios.h>

#include "hw_control.h"

#define TRUE  0
#define FALSE  -1

int speed_arr[] = { B38400, B19200, B9600, B4800, B2400, B1200, B300,
	B38400, B19200, B9600, B4800, B2400, B1200, B300,};
int name_arr[] = { 38400, 19200, 9600, 4800, 2400, 1200, 300,
	38400, 19200, 9600, 4800, 2400, 1200, 300,};

/**
*@breif open uart
 */
static int opendev(char *dev)
{
	int fd = open(dev, O_RDWR);         /* | O_NOCTTY | O_NDELAY*/

	if (-1 == fd)
		return -1;
	else
		return fd;
}

/** *@brief set uart port baud rate
* @param   fd   uart file handle
* @param   speed  baud rate
* @return   void
*/
static void setspeed(int fd, int speed)
{
	int i;
	int status;
	struct termios opt;
	tcgetattr(fd, &opt);
	for (i = 0; i < sizeof(speed_arr)/sizeof(int); i++) {
		if (speed == name_arr[i]) {
			tcflush(fd, TCIOFLUSH);
			cfsetispeed(&opt, speed_arr[i]);
			cfsetospeed(&opt, speed_arr[i]);
			status = tcsetattr(fd, TCSANOW, &opt);
			if (status != 0)
				return;
		}
		tcflush(fd, TCIOFLUSH);
	}
}

/**
*@brief
* @param    fd
* @param    databits
* @param    stopbits
* @param    parity
 */
static int setparity(int fd, int databits, int stopbits, int parity)
{
	struct termios options;
	if (tcgetattr(fd, &options) != 0)
		return FALSE;
	options.c_cflag &= ~CSIZE;
	switch (databits) {
	case 7:
		options.c_cflag |= CS7;
		break;
	case 8:
		options.c_cflag |= CS8;
		break;
	default:
		fprintf(stderr, " Unsupported data size\n ");
		return FALSE;
	}

	switch (parity) {
	case 'n':
	case 'N':
		options.c_cflag &= ~PARENB;/*  Clear parity enable  */
		options.c_iflag &= ~INPCK;/*  Enable parity checking  */
		break;
	case 'o':
	case 'O':
		options.c_cflag |= (PARODD | PARENB);
		options.c_iflag |= INPCK;
		break;
	case 'e':
	case 'E':
		options.c_cflag |= PARENB;/*  Enable parity  */
		options.c_cflag &= ~PARODD;
		options.c_iflag |= INPCK;/*  Disnable parity checking  */
		break;
	case 'S':
	case 's':
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;
		break;
	default:
		fprintf(stderr, " Unsupported parity\n ");
		return FALSE;
	}

	switch (stopbits) {
	case 1:
		options.c_cflag &= ~CSTOPB;
		break;
	case 2:
		options.c_cflag |= CSTOPB;
		break;
	default:
		fprintf(stderr, " Unsupported stop bits\n ");
		return FALSE;
	}
	options.c_cflag |= CRTSCTS;
	/*  Set input parity option  */
	if (parity != 'n')
		options.c_iflag |= INPCK;
	options.c_cc[VTIME] = 150;  /*15 seconds*/
	options.c_cc[VMIN] = 0;

	tcflush(fd, TCIFLUSH);  /*  Update the options and do it NOW  */
	if (tcsetattr(fd, TCSANOW, &options) != 0) {
		perror(" SetupSerial 3 ");
		return FALSE;
	}
	return TRUE;
}

void rts_io_release(struct msg_queue *mq)
{
	if (mq->private_data)
		free(mq->private_data);
}

int rts_io_init(struct msg_queue *mq, const char *dev)
{
	return 0;
}

int rts_io_handler(struct msg_queue *mq, struct cmd_msg *cmd)
{
	char *dev2 = "/dev/ttyS0";
	int ret = 0;
	int size = sizeof(*cmd) - sizeof(cmd->mtype);

	int fd = opendev(dev2);
	if (fd > 0)
		setspeed(fd, 2400); /* set baud rate*/
	else {
		opt_error(" Can't Open Serial Port '%s'\n ", dev2);
		return -1;
	}

	if (setparity(fd, 8, 1, 'N') == -1) {
		opt_error(" Set Parity Error\n ");
		return -1;
	}

	ret = write(fd, cmd->priv, cmd->length);
	if (ret) {
		cmd->mtype = cmd->pid;
		cmd->status = OPT_STATUS_OK;

		ret = msgsnd(mq->msg, cmd, size, IPC_NOWAIT);
	} else {
		int fail = -errno;

		opt_error("%s\n", strerror(errno));

		cmd->mtype = cmd->pid;
		cmd->status = fail;

		ret = msgsnd(mq->msg, cmd, size, IPC_NOWAIT);
	}
	close(fd);
	return ret;
}
