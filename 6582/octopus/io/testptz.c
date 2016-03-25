/*************************************************************************
    > File Name: testptz.c
    > Author: steven_feng
    > Mail: steven_feng@realsil.com.cn
    > Created Time: 2015/03/27
 ************************************************************************/
#include  <stdio.h>
#include  <stdlib.h>
#include  <unistd.h>
#include  <sys/types.h>
#include  <sys/stat.h>
#include  <fcntl.h>
#include  <errno.h>

#include "rtsio.h"

#define TRUE  0
#define FALSE  -1

int  main()
{
	int ret = 0, flag = 0;
	int level, angle;
	int id;

	rts_io_ptz_running(PTZ_RIGHT);
	sleep(2);
	rts_io_ptz_running(PTZ_LEFT);
	sleep(2);
	rts_io_ptz_running(PTZ_STOP);

	while (!flag) {
		printf("enter 1 to rurn left:\n");
		printf("enter 2 to rurn right:\n");
		printf("enter 3 to stop:\n");
		printf("enter 4 to rurn left some angle:\n");
		printf("enter 5 to rurn right some angle:\n");
		printf("enter 6 to set speed:\n");
		printf("enter 7 to get speed:\n");
		printf("enter 8 to creat preset:\n");
		printf("enter 9 to fun preset:\n");
		printf("enter 10 to del preset:\n");
		printf("enter 11 to quit\n");
		printf("Enter the command id:\n");
		printf("the command befor is %d\n", id);
		ret = scanf("%d", &id);
		if (ret != 1)
			printf("scanf erro %d\n", ret);

		getchar();

		switch (id) {
		case 1:
			ret = rts_io_ptz_running(PTZ_LEFT);
			break;
		case 2:
			ret = rts_io_ptz_running(PTZ_RIGHT);
			break;
		case 3:
			ret = rts_io_ptz_running(PTZ_STOP);
			break;
		case 4:
			printf("please enter the angle:");
			scanf("%d", &angle);
			getchar();
			ret = rts_io_ptz_running_angle(PTZ_LEFT, (uint16_t)angle);
			break;
		case 5:
			printf("please enter the angle:");
			scanf("%d", &angle);
			getchar();
			printf("angle = %d", (uint16_t)angle);
			ret = rts_io_ptz_running_angle(PTZ_RIGHT, (uint16_t)angle);
			break;
		case 6:
			printf("please enter the speed(11,20,32,42,63,0~63):");
			scanf("%d", &level);
			getchar();
			ret = rts_io_ptz_set_speed((uint8_t)level);
			break;
		case 7:
			ret = rts_io_ptz_get_speed();
			printf("the speed is %d/n", ret);
			break;
		case 8:
			ret = rts_io_ptz_creat_preset();
			break;
		case 9:
			ret = rts_io_ptz_run_preset();
			break;
		case 10:
			ret = rts_io_ptz_del_preset();
			break;
		case 11:
			flag = 1;
			break;
		default:
			flag = 0;
		}
	}
	return 0;
}
