#ifndef __ISP__H_
#define __ISP__H_

#ifndef MAX_RES_FPS
#define MAX_RES_FPS (30)
#endif

#include <stdint.h>
#include <linux/types.h>

enum {
	V4L2_CID_POWER_LINE_FREQUENCY_AUTO      = 3,
};

enum err_idx {
	open_dev_failed = -50,
	lock_dev_failed,

};



#ifdef __cplusplus
extern "C"
{
#endif

int get_cur_char_size(int fd, uint8_t *font_width,
	uint8_t *font_height, uint8_t *font_size);
int get_osd_len_text(int fd, uint8_t *string_len, char *buf);
int isp_uvc_set_ctrl(int fd, uint32_t cmd_id, int val);
int isp_uvc_get_ctrl(int fd, uint32_t cmd_id, int *val,
	int *max_val, int *min_val, int *step, int *default_val);
int isp_set_flip(int fd, int val);
int isp_set_mirror(int fd, int val);
int isp_set_white_balance(int fd, char *val);
int h264_set_peak_bit_rate(int fd, uint32_t bpr);
int osd_set_char_clr(int fd, uint32_t color);
int osd_set_bk_clr(int fd, uint32_t color);
int osd_set_char_size(int fd, uint8_t char_size);
int osd_set_text(int fd, uint8_t len, uint8_t *buf);
int osd_enable(int fd, uint8_t enable);
int osd_bk_enable(int fd, uint8_t enable);
int osd_char_transparent_enable(int fd, uint8_t enable);
int osd_set_transparency(int fd, uint8_t transparency);
int h264_set_qp(int fd, uint32_t val);
int h264_get_qp(int fd, uint32_t *val);
int h264_set_gop(int fd, uint32_t val);
int h264_get_gop(int fd, uint32_t *val);
int h264_get_peak_bit_rate(int fd, uint32_t *val);
int osd_get_char_color(int fd, uint32_t *color);
int osd_get_bk_color(int fd, uint32_t *color);
int is_osd_enable(int fd, uint8_t *val);
int is_osd_bk_enable(int fd, uint8_t *val);
int osd_char_transparent_is_enable(int fd, uint8_t *val);
int osd_get_transparency(int fd, uint8_t *val);
int isp_get_flip(int fd, int *val, int *default_val);
int isp_get_mirror(int fd, int *val, int *default_val);
int h264_get_support_resolutions(int fd, int *len, char **val);
int h264_get_support_fps(int fd, int *len, char **val);
int h264_get_cur_resolution(int fd, char *val);
int h264_get_cur_fps(int fd, char *val);

#ifdef __cplusplus
}
#endif

#endif
