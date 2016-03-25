/*
 * isp.c
 * Author: yafei meng (yafei_meng@realsil.com.cn)
 */

#include <linux/videodev2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "isp.h"
#include "rts_uvc.h"
#include "char_library.h"
#include "reg_addr.h"

#define OSD_TEXT_BUF_LEN (36)
#define CHARACTER_COUNT  (39)
#define CHAR_HEADER_LEN  (3)
#define MAX_PATH_LEN (128)
#define TMP_INFO_SIZE (64)
#define STREAMS_NUM (2)

#define SECOND_IN_DAY (86400)

static uint16_t g_char_width[2] = {0x10, 0x18};
static int16_t g_char_height[2] = {0x17, 0x20};
static uint8_t  g_char_max_num[2] = {30, 20};
static char     g_text_last[OSD_TEXT_BUF_LEN] = {"REALTEK001"};
static char     g_char_size = 1;


int get_cur_char_size(int fd, uint8_t *font_width, uint8_t *font_height, uint8_t *font_size)
{
	int ret = 0;
	uint8_t char_w = 0;
	uint8_t char_h = 0;

	assert(font_width);
	assert(font_height);
	assert(font_size);

	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr,  "add xu control fail: %s\n", strerror(-ret));
		return ret;
	}
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
				ISP_OSD_CHAR_WIDTH, &char_w, 1, 0);
	if (ret)
		return ret;
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
				ISP_OSD_CHAR_HEIGHT, &char_h, 1, 0);
	if (ret)
		return ret;

	*font_size = 2;
	if (char_w == g_char_width[0] && char_h == g_char_height[0])
		*font_size = 0;
	if (char_w == g_char_width[1] && char_h == g_char_height[1])
		*font_size = 1;

	*font_width = char_w;
	*font_height = char_h;

	return ret;
}


int get_osd_len_text(int fd, uint8_t *string_len, char *buf)
{
	int ret = 0;
	assert(buf);
	assert(string_len);
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr , "add xu control fail: %s\n", strerror(-ret));
		return ret ;
	}

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
				ISP_OSD_NUM, string_len, 1, 0);
	if (ret)
		return ret;

	ret = uvc_exec_vendor_cmd(fd, VC_OSD_CFG_STRING_READ,
				0, (uint8_t *)buf, *string_len, 0);

	return ret;
}

static int set_osd_text(int fd)
{
	int ret = 0;
	/* uint8_t byData[8] = {0}; */
	uint8_t cfg_string_len = (uint8_t)(strlen(g_text_last));
	cfg_string_len = cfg_string_len & 0x1F;

	uint8_t **p_character = NULL;
	p_character = (uint8_t **)malloc(CHARACTER_COUNT*sizeof(uint8_t *));
	uint8_t i = 0;
	switch (g_char_size) {
	case 0:
		for (i = 0 ; i < CHARACTER_COUNT; i++)
			p_character[i] = character_size_one[i];
		break;
	case 1:
		for (i = 0 ; i < CHARACTER_COUNT; i++)
			p_character[i] = character_small_init[i];
		break;
	default:
		for (i = 0 ; i < CHARACTER_COUNT; i++)
			p_character[i] = character_small_init[i];
		break;
	}
	uint8_t char_width  = p_character[0][0];
	uint8_t char_height = p_character[0][1];
	uint8_t char_len    = p_character[0][2];

	int char_dot_array_len = &(p_character[1][0]) -
		&(p_character[0][0]) - CHAR_HEADER_LEN;

	if ((char_dot_array_len%char_len) != 0) {
		fprintf(stderr,
			"In Func:%s \nLine:%d Character Library Error!!!\n",
			__func__, __LINE__);
		return -100 ;
	}

	/*13 --> [0-9 space / : ]*/
	uint16_t nCharInfoLen2SRAM = char_dot_array_len/char_len*4*(13+cfg_string_len);
	uint8_t *p_char_info_2_sram = (uint8_t *)malloc(nCharInfoLen2SRAM);
	memset(p_char_info_2_sram, 0, nCharInfoLen2SRAM);

	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr , "add xu control fail: %s\n", strerror(-ret));
		return ret ;
	}
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_OSD_CHAR_WIDTH, &char_width, 1, 0);
	if (ret)
		return ret;
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_OSD_CHAR_HEIGHT, &char_height, 1, 0);
	if (ret)
		return ret;

	uint8_t temp_data = CHAR_HEADER_LEN;
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_OSD_CHAR_HEIGHT_UP, &temp_data, 1, 0);
	if (ret)
		return ret;
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_OSD_CHAR_HEIGHT_DOWN, &temp_data, 1, 0);
	if (ret)
		return ret;
	temp_data = 1;
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_OSD_CHAR_WIDTH_LEFT, &temp_data, 1, 0);
	if (ret)
		return ret;
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_OSD_NUM, &cfg_string_len, 1, 0);
	if (ret)
		return ret;

	int j = 0;
	uint8_t str_idx = 0;
	for (str_idx = 0; str_idx < 13; str_idx++) {
		if (4 == char_len) {
			for (i = CHAR_HEADER_LEN; i < char_dot_array_len+CHAR_HEADER_LEN; i++) {
				if ((i-CHAR_HEADER_LEN)%char_len == 0) {
					p_char_info_2_sram[j++] = p_character[str_idx][i];
					p_char_info_2_sram[j++] = p_character[str_idx][i+1];
					p_char_info_2_sram[j++] = p_character[str_idx][i+2];
					p_char_info_2_sram[j++] = p_character[str_idx][i+3];
				}
			}

		} else if (3 == char_len) {
			for (i = CHAR_HEADER_LEN; i < char_dot_array_len+CHAR_HEADER_LEN; i++) {
				if ((i-CHAR_HEADER_LEN)%char_len == 0) {
					p_char_info_2_sram[j++] = p_character[str_idx][i];
					p_char_info_2_sram[j++] = p_character[str_idx][i+1];
					p_char_info_2_sram[j++] = p_character[str_idx][i+2];
					p_char_info_2_sram[j++] = 0;
				}
			}
		} else if (2 == char_len) {
			for (i = CHAR_HEADER_LEN; i < char_dot_array_len+CHAR_HEADER_LEN; i++) {
				if ((i-CHAR_HEADER_LEN)%char_len == 0) {
					p_char_info_2_sram[j++] = p_character[str_idx][i];
					p_char_info_2_sram[j++] = p_character[str_idx][i+1];
					p_char_info_2_sram[j++] = 0;
					p_char_info_2_sram[j++] = 0;
				}
			}
		} else if (1 == char_len) {
			for (i = CHAR_HEADER_LEN; i < char_dot_array_len+CHAR_HEADER_LEN; i++) {
				if ((i-CHAR_HEADER_LEN)%char_len == 0) {
					p_char_info_2_sram[j++] = p_character[str_idx][i];
					p_char_info_2_sram[j++] = 0;
					p_char_info_2_sram[j++] = 0;
					p_char_info_2_sram[j++] = 0;
				}
			}
		}
	}

	for (str_idx = 0; str_idx < cfg_string_len; str_idx++) {
		char character = g_text_last[str_idx];
		int index = 0;
		if (32 == character)
			index = 10;
		else if (47 == character)
			index = 11;
		else if (58 == character)
			index = 12;
		else if (character >= 48 && character <= 57)
			index = character - 48;
		else if (character >= 65 && character <= 90)
			index = character - 65+13;


		if (4 == char_len) {
			for (i = CHAR_HEADER_LEN; i < char_dot_array_len+CHAR_HEADER_LEN; i++) {
				if ((i-CHAR_HEADER_LEN)%char_len == 0) {
					p_char_info_2_sram[j++] = p_character[index][i];
					p_char_info_2_sram[j++] = p_character[index][i+1];
					p_char_info_2_sram[j++] = p_character[index][i+2];
					p_char_info_2_sram[j++] = p_character[index][i+3];
				}
			}
		} else if (3 == char_len) {
			for (i = CHAR_HEADER_LEN; i < char_dot_array_len+CHAR_HEADER_LEN; i++) {
				if ((i-CHAR_HEADER_LEN)%char_len == 0) {
					p_char_info_2_sram[j++] = p_character[index][i];
					p_char_info_2_sram[j++] = p_character[index][i+1];
					p_char_info_2_sram[j++] = p_character[index][i+2];
					p_char_info_2_sram[j++] = 0;
				}
			}
		} else if (2 == char_len) {
			for (i = CHAR_HEADER_LEN; i < char_dot_array_len+CHAR_HEADER_LEN; i++) {
				if ((i-CHAR_HEADER_LEN)%char_len == 0) {
					p_char_info_2_sram[j++] = p_character[index][i];
					p_char_info_2_sram[j++] = p_character[index][i+1];
					p_char_info_2_sram[j++] = 0;
					p_char_info_2_sram[j++] = 0;
				}
			}
		} else if (1 == char_len) {
			for (i = CHAR_HEADER_LEN; i < char_dot_array_len+CHAR_HEADER_LEN; i++) {
				if ((i-CHAR_HEADER_LEN)%char_len == 0) {
					p_char_info_2_sram[j++] = p_character[index][i];
					p_char_info_2_sram[j++] = 0;
					p_char_info_2_sram[j++] = 0;
					p_char_info_2_sram[j++] = 0;
				}
			}
		}
	}

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
				ISP_OSD_NUM, &cfg_string_len, 1, 0);
	if (ret)
		return ret;

	ret = uvc_exec_vendor_cmd(fd, VC_OSD_CFG_STRING_WRITE, 0,
			(uint8_t *)g_text_last, cfg_string_len, 0);
	if (ret)
		return ret;

	/*ensure ISP Clock is valid;*/
	ret = uvc_exec_vendor_cmd(fd, VC_OSD_SRAM_SET, 0,
			p_char_info_2_sram, nCharInfoLen2SRAM, 0);
	/*
	if (ret)
		return ret;
	*/
	if (p_char_info_2_sram) {
		free(p_char_info_2_sram);
		p_char_info_2_sram = NULL;
	}

	for (i = 0; i < CHARACTER_COUNT; i++)
		p_character[i] = NULL;

	if (p_character) {
		free(p_character);
		p_character = NULL;
	}

	return ret;
}

int isp_uvc_set_ctrl(int fd, uint32_t cmd_id, int val)
{
	int ret = 0;

	fprintf(stdout, "in Func :%s with ctrl id  0x%08x\n", __func__, cmd_id);
	struct v4l2_queryctrl queryctrl;
	struct v4l2_control ctrl;
	queryctrl.id = cmd_id;
	ret = v4l2_ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl);
	if (ret < 0) {
		fprintf(stderr, "[Error] : Query  Failed!");
		goto out;
	}

	ctrl.id = queryctrl.id;
	if (val > queryctrl.maximum)
		ctrl.value = queryctrl.maximum;
	else if (val < queryctrl.minimum)
		ctrl.value = queryctrl.minimum;
	else
		ctrl.value = val;

	ret = v4l2_ioctl(fd, VIDIOC_S_CTRL, &ctrl);
	if (ret < 0) {
		fprintf(stderr,
		"[Error] : Set Ctrl Failed,please check the value");
		goto out;
	}

out:
	fprintf(stdout, "exit Func :%s ret = %d\n", __func__, ret);
	return ret;
}

int isp_uvc_get_ctrl(int fd, uint32_t cmd_id,
			int *val, int *max_val, int *min_val, int *step, int *default_val)
{
	int ret = 0;
	fprintf(stdout, "in Func :%s with ctrl id  0x%08x\n", __func__, cmd_id);
	struct v4l2_queryctrl queryctrl;
	struct v4l2_control ctrl;
	queryctrl.id = cmd_id;

	ret = v4l2_ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl);
	if (ret < 0) {
		fprintf(stderr, "[Error]:Query  Failed!");
		goto out;
	}

	ctrl.id = queryctrl.id;
	ret = v4l2_ioctl(fd, VIDIOC_G_CTRL, &ctrl);
	if (ret < 0) {
		fprintf(stderr,
		"[Error]:Set Ctrl Failed,please check the value");
		goto out;
	}

	if (NULL != val)
		*val = ctrl.value;
	if (NULL != max_val)
		*max_val = queryctrl.maximum;
	if (NULL != min_val)
		*min_val = queryctrl.minimum;
	if (NULL != step)
		*step = queryctrl.step;
	if (NULL != default_val)
		*default_val = queryctrl.default_value;

	fprintf(stdout, "exit Func :%s \n", __func__);
out:
	return ret;
}


int isp_set_flip(int fd, int val)
{
	int ret = 0;
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr,  "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	uint8_t buf[2] = {0};
	buf[0] = 0x04;

	ret = uvc_exec_vendor_cmd(fd, VC_I2C_NORMAL_WRITE, 0x60, buf, 1, 0);
	if (ret)
		goto out;

	ret = uvc_exec_vendor_cmd(fd, VC_I2C_NORMAL_READ, 0x60, buf, 1, 0);
	if (ret)
		goto out;

	if (val)
		buf[1] = buf[0]|0x40;/*Flip on*/
	else
		buf[1] = buf[0]&0xBF;/*Flip: off*/
	buf[0] = 0x04;
	ret = uvc_exec_vendor_cmd(fd, VC_I2C_NORMAL_WRITE, 0x60, buf, 2, 0);

out:
	return ret;
}

int isp_set_mirror(int fd, int val)
{
	int ret = 0;

	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr,  "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	uint8_t buf[2] = {0};

	buf[0] = 0x04;
	ret = uvc_exec_vendor_cmd(fd, VC_I2C_NORMAL_WRITE, 0x60, buf, 1, 0);
	if (ret)
		goto out;
	ret = uvc_exec_vendor_cmd(fd, VC_I2C_NORMAL_READ, 0x60, buf, 1, 0);
	if (ret)
		goto out;

	if (val)
		buf[1] = buf[0]|0x80;/*mirror on*/
	else
		buf[1] = buf[0]&0x7f;/*mirror off*/

	buf[0] = 0x04;
	ret = uvc_exec_vendor_cmd(fd, VC_I2C_NORMAL_WRITE, 0x60, buf, 2, 0);

out:
	return ret;
}


int isp_set_white_balance(int fd, char *val)
{
	int ret = 0;
	int mode = atoi(val);
	if (0 == mode) {
		ret = isp_uvc_set_ctrl(fd, V4L2_CID_AUTO_WHITE_BALANCE, 1);
		if (ret)
			goto out;
	} else if (1 == mode) {
		ret = isp_uvc_set_ctrl(fd, V4L2_CID_AUTO_WHITE_BALANCE, 0);
		if (ret)
			goto out;
		ret = isp_uvc_set_ctrl(fd,
			V4L2_CID_WHITE_BALANCE_TEMPERATURE, 6500);
		if (ret)
			goto out;
	} else if (2 == mode) {
		ret = isp_uvc_set_ctrl(fd, V4L2_CID_AUTO_WHITE_BALANCE, 0);
		if (ret)
			goto out;
		ret = isp_uvc_set_ctrl(fd,
			V4L2_CID_WHITE_BALANCE_TEMPERATURE, 4600);
		if (ret)
			goto out;
	} else if (3 == mode) {
		ret = isp_uvc_set_ctrl(fd, V4L2_CID_AUTO_WHITE_BALANCE, 0);
		if (ret)
			goto out;
		ret = isp_uvc_set_ctrl(fd,
			V4L2_CID_WHITE_BALANCE_TEMPERATURE, 3500);
		if (ret)
			goto out;
	} else if (4 == mode) {
		ret = isp_uvc_set_ctrl(fd, V4L2_CID_AUTO_WHITE_BALANCE, 0);
		if (ret)
			goto out;
		ret = isp_uvc_set_ctrl(fd,
			V4L2_CID_WHITE_BALANCE_TEMPERATURE, 2800);
		if (ret)
			goto out;
	}
out:
	return ret;
}


int h264_set_peak_bit_rate(int fd, uint32_t bpr)
{
	int ret = 0;

	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	uint8_t bitrate[4] = {0};
	/*long temp_data = bpr<<10;*/
	long temp_data = bpr;
	bitrate[0] = (uint8_t)(temp_data & 0xFF);
	bitrate[1] = (uint8_t)(temp_data >> 8) & 0xFF;
	bitrate[2] = (uint8_t)(temp_data >> 16) & 0xFF;
	bitrate[3] = (uint8_t)(temp_data >> 24) & 0xFF;
	ret = uvc_exec_vendor_cmd(fd, VC_H264_BITRATE_SET, 0, bitrate, 4, 0);
out:
	return ret;
}

static int osd_set_color(int fd, uint32_t addr, uint32_t color)
{
	int ret = 0;
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	uint8_t buf[8] = {0};

	uint8_t R = (color >> 16) & 0xFF;
	uint8_t G = (color >> 8) & 0xFF;
	uint8_t B = color & 0xFF;
	buf[0] = ((66 * R  + 129 * G +  25 * B + 128) >> 8) +  16;
	buf[1] = ((-38 * R -  74 * G + 112 * B + 128) >> 8) + 128;
	buf[2] = ((112 * R -  94 * G -  18 * B + 128) >> 8) + 128;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
				addr, buf, 3, 0);
out:
	return ret;
}
int osd_set_char_clr(int fd, uint32_t color)
{
	int ret = 0;
	ret = osd_set_color(fd, ISP_OSD_CHAR_Y, color);
	return ret;
}


int osd_set_bk_clr(int fd, uint32_t color)
{
	int ret = 0;
	ret = osd_set_color(fd, ISP_OSD_BACK_Y, color);
	return ret;
}

int osd_set_char_size(int fd, uint8_t char_size)
{
	int ret = 0;
	uint8_t string_len = 0;

	if (char_size > 1)
		char_size = 1;
	g_char_size = char_size;
	/*get string from sram*/
	memset(g_text_last, 0, OSD_TEXT_BUF_LEN * sizeof(uint8_t));
	ret = get_osd_len_text(fd, &string_len, g_text_last);
	if (ret)
		goto out;
	/*set text right now*/
	ret = set_osd_text(fd);
	if (ret)
		goto out;
out:
	return ret;
}

int osd_set_text(int fd, uint8_t len, uint8_t *buf)
{
	int ret = 0;
	uint8_t char_size = 0;
	uint8_t char_width = 0;
	uint8_t char_height = 0;

	int num = g_char_max_num[g_char_size];
	if (num > len)
		num = len;
	uint8_t idx = 0;
	memset(g_text_last, 0, OSD_TEXT_BUF_LEN);

	for (idx = 0; idx < num; idx++)
		g_text_last[idx] = toupper(buf[idx]);

	ret = get_cur_char_size(fd, &char_width, &char_height, &char_size);
	if (ret)
		goto out;
	g_char_size = char_size;

	ret = set_osd_text(fd);
out:
	return ret;
}

static int osd_set_date_time(int fd)
{
	int ret = 0;
	uint8_t buf[16] = {0};
	struct tm *local;
	time_t t_today;
	time_t t_tomorrow;
	/*from 1970 1 1 0 to now,how many seconds are there?*/
	t_today = time((time_t *)NULL);
	/*change to local time*/
	local = localtime(&t_today);
	/*current date*/
	buf[0] = (local->tm_year + 1900)/1000;
	buf[1] = ((local->tm_year + 1900)%1000)/100;
	buf[2] = ((local->tm_year + 1900)%100)/10;
	buf[3] = (local->tm_year + 1900)%10;
	buf[4] = (local->tm_mon + 1)/10;
	buf[5] = (local->tm_mon + 1)%10;
	buf[6] = (local->tm_mday)/10;
	buf[7] = (local->tm_mday)%10;
	/*tomorrow*/
	t_tomorrow = t_today + SECOND_IN_DAY;
	local = localtime(&t_tomorrow);
	buf[8] = (local->tm_year + 1900)/1000;
	buf[9] = ((local->tm_year + 1900)%1000)/100;
	buf[10] = ((local->tm_year + 1900)%100)/10;
	buf[11] = (local->tm_year + 1900)%10;
	buf[12] = (local->tm_mon + 1)/10;
	buf[13] = (local->tm_mon + 1)%10;
	buf[14] = (local->tm_mday)/10;
	buf[15] = (local->tm_mday)%10;

	uint8_t date_flag = 0;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
				ISP_OSD_DATE_SEL, &date_flag, 1, 0);
	if (ret)
		goto out;
	/*write_date*/
	if (date_flag&0x01) {
		ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_OSD_DATE_B_PARAMETER, buf, 8, 0);
		if (ret)
			goto out;
		ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_OSD_DATE_A_PARAMETER, &buf[8], 8, 0);
		if (ret)
			goto out;
	} else {
		ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_OSD_DATE_A_PARAMETER, buf, 8, 0);
		if (ret)
			goto out;
		ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_OSD_DATE_B_PARAMETER, &buf[8], 8, 0);
		if (ret)
			goto out;
	}
	/*write time*/
	t_today = time((time_t *)NULL);
	local = localtime(&t_today);
	buf[0] = 0;
	buf[1] = local->tm_hour / 10;
	buf[2] = local->tm_hour % 10;
	buf[3] = local->tm_min / 10;
	buf[4] = local->tm_min % 10;
	buf[5] = local->tm_sec / 10;
	buf[6] = local->tm_sec % 10;
	buf[7] = 0;
	ret = uvc_exec_vendor_cmd(fd, VC_OSD_TIME_SET,
			0, buf, 8, 0);
out:
	return ret;
}

int osd_enable(int fd, uint8_t enable)
{
	int ret = 0;

	uint8_t buf[2] = {0};
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
				ISP_CONTROL3, buf, 1, 0);
	if (ret)
		goto out;

	buf[0] &= 0xBF;
	if (enable) {
		buf[0] |= 0x40;
		/*update os time to asic register*/
		ret = osd_set_date_time(fd);
		if (ret)
			goto out;
	}

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
				ISP_CONTROL3, buf, 1, 0);
	if (ret)
		goto out;
out:
	return ret;
}

int osd_bk_enable(int fd, uint8_t enable)
{
	int ret = 0;

	uint8_t buf[2] = {0};
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	if (enable)
		buf[0] = 0x01;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
				ISP_OSD_BACK_CTRL, buf, 1, 0);
	if (ret)
		goto out;
out:
	return ret;
}

int osd_char_transparent_enable(int fd, uint8_t enable)
{
	int ret = 0;
	uint8_t buf[2] = {0};
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	if (enable)
		buf[0] = 0x01;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
				ISP_OSD_CHAR_CTRL, buf, 1, 0);

out:
	return ret;
}

int osd_set_transparency(int fd, uint8_t transparency)
{
	int ret = 0;
	uint8_t buf[2] = {0};
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	if (transparency > 15)
		buf[0] = 15;
	else if (transparency < 0)
		buf[0] = 0;
	else
		buf[0] = transparency;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
				ISP_OSD_CHAR_ALPHA, buf, 1, 0);

out:
	return ret;
}

int h264_set_qp(int fd, uint32_t val)
{
	int ret = 0;
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	uint8_t buf[2] = {0};
	buf[0] = (uint8_t)val;
	ret = uvc_exec_vendor_cmd(fd, VC_H264_MAX_QP_SET, 0, buf, 1, 0);
	if (ret)
		goto out;
out:
	return ret;
}

int h264_get_qp(int fd, uint32_t *val)
{
	int ret = 0;
	assert(val);
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	uint8_t buf[2] = {0};
	ret = uvc_exec_vendor_cmd(fd, VC_H264_MAX_QP_GET, 0, buf, 1, 0);
	if (ret)
		goto out;

	*val = buf[0];
out:
	return ret;
}

int h264_set_gop(int fd, uint32_t val)
{
	int ret = 0;
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	uint8_t buf[2] = {0};
	buf[0] = (uint8_t)val;
	ret = uvc_exec_vendor_cmd(fd, VC_H264_GOP_SET, 0, buf, 1, 0);
	if (ret)
		goto out;
out:
	return ret;
}

int h264_get_gop(int fd, uint32_t *val)
{
	int ret = 0;
	assert(val);
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	uint8_t buf[2] = {0};
	ret = uvc_exec_vendor_cmd(fd, VC_H264_GOP_GET, 0, buf, 1, 0);
	if (ret)
		goto out;

	*val = buf[0];
out:
	return ret;
}


int h264_get_peak_bit_rate(int fd, uint32_t *val)
{
	int ret = 0;
	assert(val);
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	uint8_t bitrate[4] = {0};
	ret = uvc_exec_vendor_cmd(fd, VC_H264_BITRATE_GET, 0, bitrate, 4, 0);
	if (ret)
		goto out;

	long temp_val = 0;
	temp_val  = bitrate[0];
	temp_val |= (bitrate[1]<<8);
	temp_val |= (bitrate[2]<<16);
	temp_val |= (bitrate[3]<<24);
	/*temp_val = temp_val>>10;*/
	*val = (uint32_t)temp_val;
out:
	return ret;
}

static int osd_get_color(int fd, uint32_t *color, int address)
{
	int ret = 0;
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	uint8_t buf[8] = {0};
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
				address, buf, 3, 0);
	uint32_t clr_y = buf[0];
	uint32_t clr_u = buf[1];
	uint32_t clr_v = buf[2];

	uint32_t C = clr_y - 16;
	uint32_t D = clr_u - 128;
	uint32_t E = clr_v - 128;
	uint32_t R = (298 * C           + 409 * E + 128) >> 8;
	uint32_t G = (298 * C - 100 * D - 208 * E + 128) >> 8;
	uint32_t B = (298 * C + 516 * D           + 128) >> 8;
	R = R > 255 ? 255 : R;
	R = R < 0 ? 0 : R;
	G = G > 255 ? 255 : G;
	G = G < 0 ? 0 : G;
	B = B > 255 ? 255 : B;
	B = B < 0 ? 0 : B;

	uint32_t clr = (R<<16) + (G<<8) + B;
	if (color)
		*color = clr;
	else
		ret = -1;
out:
	return ret;
}

int osd_get_char_color(int fd, uint32_t *color)
{
	int ret = 0;
	ret = osd_get_color(fd, color, ISP_OSD_CHAR_Y);
	return ret;
}

int osd_get_bk_color(int fd, uint32_t *color)
{
	int ret = 0;
	ret = osd_get_color(fd, color, ISP_OSD_BACK_Y);
	return ret;
}

int is_osd_enable(int fd, uint8_t *val)
{
	int ret = 0;

	uint8_t buf[2] = {0};
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ, ISP_CONTROL3, buf, 1, 0);
	if (ret)
		goto out;

	if (val)
		*val = (buf[0] & 0x40) >> 6;
	else
		ret = -1;

out:
	return ret;
}

int is_osd_bk_enable(int fd, uint8_t *val)
{
	int ret = 0;
	uint8_t buf[2] = {0};
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
				ISP_OSD_BACK_CTRL, buf, 1, 0);
	if (ret)
		goto out;
	if (val)
		*val = buf[0];
	else
		ret = -1;
out:
	return ret;
}

int osd_char_transparent_is_enable(int fd, uint8_t *val)
{
	int ret = 0;

	uint8_t buf[2] = {0};
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
				ISP_OSD_CHAR_CTRL, buf, 1, 0);
	if (ret)
		goto out;

	if (val)
		*val = buf[0];
	else
		ret = -1;
out:
	return ret;
}

int osd_get_transparency(int fd, uint8_t *val)
{
	int ret = 0;
	uint8_t buf[2] = {0};
	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
				ISP_OSD_CHAR_ALPHA, buf, 1, 0);

	if (ret)
		goto out;
	if (val)
		*val = buf[0];
	else
		ret = -1;
out:
	return ret;
}


int isp_get_flip(int fd, int *val, int *default_val)
{
	int ret = 0;

	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	uint8_t buf[2] = {0};
	buf[0] = 0x04;

	ret = uvc_exec_vendor_cmd(fd, VC_I2C_NORMAL_WRITE, 0x60, buf, 1, 0);
	if (ret)
		goto out;

	ret = uvc_exec_vendor_cmd(fd, VC_I2C_NORMAL_READ, 0x60, buf, 1, 0);
	if (ret)
		goto out;
	if (val)
		*val = (buf[0]&0xBF)>>6;
	if (default_val)
		*default_val = *val;
out:
	return ret;
}

int isp_get_mirror(int fd, int *val, int *default_val)
{
	int ret = 0;

	ret = uvc_add_xu_ctrls(fd);
	if (ret < 0) {
		fprintf(stderr, "add xu control fail: %s\n", strerror(-ret));
		goto out;
	}

	uint8_t buf[2] = {0};
	buf[0] = 0x04;

	ret = uvc_exec_vendor_cmd(fd, VC_I2C_NORMAL_WRITE, 0x60, buf, 1, 0);
	if (ret)
		goto out;

	ret = uvc_exec_vendor_cmd(fd, VC_I2C_NORMAL_READ, 0x60, buf, 1, 0);
	if (ret)
		goto out;

	if (val)
		*val = (buf[0]&0x7F)>>7;
	if (default_val)
		*default_val = *val;
out:
	return ret;
}


static void enum_frame_interval(int fd,
		struct v4l2_fmtdesc *fmtdesc,
		struct v4l2_frmsizeenum *fsize,
		char **val, int *len)
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
			assert(val[fival.index]);
			sprintf(val[fival.index], "%d",
				fival.discrete.denominator);
		}
		fival.index++;
	}
	*len = fival.index;
}

static void enum_frame_size(int fd, struct v4l2_fmtdesc *fmtdesc,
			char **val, int *len, uint8_t is_get_fps)
{
	int ret;
	struct v4l2_frmsizeenum fsize;

	memset(&fsize, 0, sizeof(fsize));
	fsize.index = 0;
	fsize.pixel_format = fmtdesc->pixelformat;

	while (!(ret = v4l2_ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsize))) {
		if (!is_get_fps) {
			if (fsize.type) {
				assert(val[fsize.index]);
				sprintf(val[fsize.index], "%dx%d",
					fsize.discrete.width,
					fsize.discrete.height);
			}
		} else {
			enum_frame_interval(fd, fmtdesc, &fsize, val, len);
			break;
		}
		fsize.index++;
	}
	if (!is_get_fps)
		*len = fsize.index;
}

static int get_resolutions_fps(int fd,
		int *len, char **val, uint8_t b_fps)
{
	int ret = 0;

	struct v4l2_fmtdesc fmtdesc;

	memset(&fmtdesc, 0, sizeof(fmtdesc));
	fmtdesc.index = 0;
	fmtdesc.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	int flag = 0;
	/*output all video format supported by video device*/
	do {
		flag = v4l2_ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);
		if (0 == flag) {
			if (!strcmp("H264", fmtdesc.description))
				enum_frame_size(fd, &fmtdesc, val, len, b_fps);
		} else {
			fprintf(stderr, "%s\n", strerror(-ret));
			break;
		}
		fmtdesc.index++;
	} while (0 <= flag);

	return ret;
}

int h264_get_support_resolutions(int fd, int *len, char **val)
{
	return get_resolutions_fps(fd, len, val, 0);
}

int h264_get_support_fps(int fd, int *len, char **val)
{
	return get_resolutions_fps(fd, len, val, 1);
}

int h264_get_cur_resolution(int fd, char *val)
{
	int ret = 0;
	struct v4l2_format fmt;
	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	ret = v4l2_ioctl(fd, VIDIOC_G_FMT, &fmt);
	if (ret < 0) {
		fprintf(stderr, "Get FMT failed\n");
		goto out;
	}
/*	char buf[64] = {0};*/
	sprintf(val, "%dx%d", fmt.fmt.pix.width, fmt.fmt.pix.height);
/*
	for (i = 0; i < MAX_RES_FPS; i++) {
		if (!strcmp(buf, g_resolution[i])) {
			*val = i;
			break;
		}
	}
*/
out:
	return ret;
}

int h264_get_cur_fps(int fd, char *val)
{
	int ret = 0;
	/* int i = 0; */
	struct v4l2_streamparm stream_param;
	memset(&stream_param, 0, sizeof(struct v4l2_streamparm));
	stream_param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = v4l2_ioctl(fd, VIDIOC_G_PARM, &stream_param);
	if (ret < 0) {
		fprintf(stderr, "Get Parm failed\n");
		goto out;
	}

/*	char buf[16] = {0};*/
	sprintf(val, "%d", stream_param.parm.capture.timeperframe.denominator);
/*
	for (i = 0; i < MAX_RES_FPS; i++) {
		if (!strcmp(buf, g_fps[i])) {
			*val = i;
			break;
		}
	}
*/
out:
	return ret;
}
