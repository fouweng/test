#ifndef __RTS_CAM_H
#define __RTS_CAM_H

#include <stdint.h>


#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_MD_MODULE_NUM (3)
#define MAX_MASK_MODULE_NUM (3)

enum {
	/* attribute ID */
	ATTR_ID_VIDEO		= 1,
	ATTR_ID_AUDIO		= 2,
	ATTR_ID_STREAM		= 3,
	ATTR_ID_MOTION_DETECT	= 4,
	ATTR_ID_MASK		= 5,
	ATTR_ID_H264		= 6,
	ATTR_ID_OSD		= 7,

};


enum video_frmsizetypes {
	VIDEO_FRMSIZE_TYPE_DISCRETE	= 1,
	VIDEO_FRMSIZE_TYPE_CONTINUOUS	= 2,
	VIDEO_FRMSIZE_TYPE_STEPWISE	= 3,
};

struct rts_fract {
	uint32_t numerator;
	uint32_t denominator;
};

struct video_frmival_stepwise {
	struct rts_fract min;	/* Minimum frame interval [s] */
	struct rts_fract max;	/* Maximum frame interval [s] */
	struct rts_fract step;	/* Frame interval step size [s] */
};

struct video_frmival {
	uint32_t type; /* enum */

	union {
		struct rts_fract		discrete;
		struct video_frmival_stepwise	stepwise;
	};
};

struct video_resolution_discrete {
	uint32_t width;		/* Frame width [pixel] */
	uint32_t height;	/* Frame height [pixel] */
};

struct video_resolution_stepwise {
	uint32_t min_width;	/* Minimum frame width [pixel] */
	uint32_t max_width;	/* Maximum frame width [pixel] */
	uint32_t step_width;	/* Frame width step size [pixel] */
	uint32_t min_height;	/* Minimum frame height [pixel] */
	uint32_t max_height;	/* Maximum frame height [pixel] */
	uint32_t step_height;	/* Frame height step size [pixel] */
};

struct video_resolution {
	uint32_t type;  /* enum */
	union {
		struct video_resolution_discrete discrete;
		struct video_resolution_stepwise stepwise;
	};

	int number; /* count of fps */
	/* define fps as integrate ? */
	struct video_frmival *frmivals;
};

struct video_format {
	uint32_t format;
	int number; /* count of resolution */
	struct video_resolution *resolutions;
};

struct attr_stream {
	uint32_t attr_id;

	int number; /* count of format */
	struct video_format *formats;

	struct {
		uint32_t format;
		struct video_resolution resolution;
		struct video_frmival frmival;
	} current;

	uint32_t reserved[4];
};

enum video_ctrl_type {
	VIDEO_CTRL_TYPE_INTEGER		= 1,
	VIDEO_CTRL_TYPE_BOOLEAN		= 2,
	VIDEO_CTRL_TYPE_MENU		= 3,
	VIDEO_CTRL_TYPE_BUTTON		= 4,
	VIDEO_CTRL_TYPE_INTEGER64	= 5,
	VIDEO_CTRL_TYPE_CTRL_CLASS	= 6,
	VIDEO_CTRL_TYPE_STRING		= 7,
	VIDEO_CTRL_TYPE_BITMASK		= 8,
	VIDEO_CTRL_TYPE_INTEGER_MENU	= 9,
};

struct video_rect {
	uint32_t left;
	uint32_t top;
	uint32_t right;
	uint32_t bottom;
};


struct video_ctrl {
	uint32_t	type;		/* enum video_ctrl_type */
	uint32_t	supported;	/* is this ctrl supported */
	int8_t		name[32];	/* Whatever */
	int32_t		minimum;	/* Note signedness */
	int32_t		maximum;
	int32_t		step;
	int32_t		default_val;
	int32_t		current;
	uint32_t	flags;
	uint32_t	reserved[2];
};

typedef struct video_ctrl brightness_ctl;
typedef struct video_ctrl contrast_ctl;
typedef struct video_ctrl hue_ctl;
typedef struct video_ctrl saturation_ctl;
typedef struct video_ctrl sharpness_ctl;
typedef struct video_ctrl gamma_ctl;
typedef struct video_ctrl auto_white_balance_ctl;
typedef struct video_ctrl white_balance_temperature_ctl;
typedef struct video_ctrl backlight_compensate_ctl;
typedef struct video_ctrl gain_ctl;
typedef struct video_ctrl power_line_frequency_ctl;
typedef struct video_ctrl exposure_mode_ctl;
typedef struct video_ctrl exposure_priority_ctl;
typedef struct video_ctrl exposure_time_ctl;
typedef struct video_ctrl auto_focus_ctl;
typedef struct video_ctrl focus_ctl;
typedef struct video_ctrl zoom_ctl;
typedef struct video_ctrl pan_tilt_ctl;
typedef struct video_ctrl roll_ctl;
typedef struct video_ctrl flip_ctl;
typedef struct video_ctrl mirror_ctl;
typedef struct video_ctrl rotate_ctl;
typedef struct video_ctrl isp_special_effect_ctl;
typedef struct video_ctrl ev_compensate_ctl;
typedef struct video_ctrl color_temperature_estimation_ctl;
typedef struct video_ctrl ae_lock_ctl;
typedef struct video_ctrl awb_lock_ctl;
typedef struct video_ctrl af_lock_ctl;
typedef struct video_ctrl led_touch_mode_ctl;
typedef struct video_ctrl led_flash_mode_ctl;
typedef struct video_ctrl iso_ctl;
typedef struct video_ctrl scene_mode_ctl;
typedef struct video_ctrl roi_mode_ctl;
typedef struct video_ctrl ae_awb_af_status_ctl;
typedef struct video_ctrl idea_eye_sensitivity_ctl;
typedef struct video_ctrl idea_eye_status_ctl;
typedef struct video_ctrl idea_eye_mode_ctl;

struct attr_video {

	uint32_t attr_id;

	brightness_ctl brightness;
	contrast_ctl contrast;
	hue_ctl hue;
	saturation_ctl saturation;
	sharpness_ctl sharpness;
	gamma_ctl gamma;
	auto_white_balance_ctl auto_white_balance;
	white_balance_temperature_ctl white_balance_temperature;
	backlight_compensate_ctl backlight_compensate;
	gain_ctl gain;
	power_line_frequency_ctl power_line_frequency;
	exposure_mode_ctl exposure_mode;
	exposure_priority_ctl exposure_priority;
	exposure_time_ctl exposure_time;
	auto_focus_ctl auto_focus;
	focus_ctl focus;
	zoom_ctl zoom;
	pan_tilt_ctl pan_tilt;
	roll_ctl roll;
	flip_ctl flip;
	mirror_ctl mirror;
	rotate_ctl rotate;
	isp_special_effect_ctl isp_special_effect;
	ev_compensate_ctl ev_compensate;
	color_temperature_estimation_ctl color_temperature_estimation;
	ae_lock_ctl ae_lock;
	awb_lock_ctl awb_lock;
	af_lock_ctl af_lock;
	led_touch_mode_ctl led_touch_mode;
	led_flash_mode_ctl led_flash_mode;
	iso_ctl iso;
	scene_mode_ctl scene_mode;
	roi_mode_ctl roi_mode;
	ae_awb_af_status_ctl ae_awb_af_status;
	idea_eye_sensitivity_ctl idea_eye_sensitivity;
	idea_eye_status_ctl idea_eye_status;
	idea_eye_mode_ctl idea_eye_mode;

	/* roi */
	struct video_rect	roi;

	uint32_t		reserved[4];
};

enum {
	/* motion detect block type */
	MD_BLOCK_TYPE_RECT	= 1,
	MD_BLOCK_TYPE_GRID,

	MD_MAX_BITMAP_SIZE	= 1024,

	MASK_BLOCK_TYPE_RECT	= 1,
	MASK_BLOCK_TYPE_GRID,
};

struct grid_unit {
	uint32_t width;
	uint32_t hegiht;
};

struct video_grid {
	uint32_t left;
	uint32_t top;

	struct grid_unit cur;
	uint32_t rows;
	uint32_t columns;
	int	lenth;
	uint8_t *bitmap;

	struct grid_unit min;
	struct grid_unit step;
};


struct md_block {
	uint32_t type;
	int enable;
	int32_t sensitivity;
	int32_t percentage;

	union {
		struct video_rect rect;
		struct video_grid grid;
	};
};

struct attr_md {
	uint32_t attr_id;
	int number;
	struct md_block *blocks;

	uint32_t reserved[4];
};


struct mask_block {
	uint32_t type;
	int enable;
	uint32_t color;

	union {
		struct video_rect rect;
		/*struct mask_grid grid;*/
		struct video_grid grid;
	};
};

struct attr_mask {
	uint32_t attr_id;
	int number;
	struct mask_block *blocks;

	uint32_t reserved[4];
};

enum {
	OSD_SINGLE_CHAR = 0,
	OSD_DOUBLE_CHAR,
	OSD_BITMAP,
	OSD_OTHERS,
	OSD_ROTATE_90 = 0,
	OSD_ROTATE_180,
	OSD_ROTATE_270,
	ATTR_NOT_SUPPORTED = 100,
};

struct osd_block {
	uint8_t enable;
	struct video_rect rect;

	/*uint8_t addressing_mode;*/	/* 0: direct addressing, 1: indirect addressing*/
	uint8_t fg_mode;	/* single char, double char, or bitmap*/
	/*uint32_t start_addr;*/	/* addr should set by this lib*/
	uint8_t flick_enable;
	uint32_t flick_speed;
	uint8_t fg_transparent_enable;
	uint32_t fg_transparency; /* 0-100*/
	uint32_t fg_color;
	uint8_t bg_enable;
	uint32_t bg_color;
	uint8_t each_line_wrap_enable;
	uint32_t each_line_wrap_num;
	int osd_total_num;	/* total num of char to be displayed, if bitmap selected size is 1*/
	uint8_t *osd_buffer;	/* char or bitmap info*/
	uint32_t osd_font_size;
	uint32_t osd_width;	/* direct addressing, char dot array or bitmap width*/
	uint32_t osd_height;	/* direct addressing, char dot array or bitmap height*/
	uint8_t osd_rotate_mode;	/* 90/180/270*/
	uint32_t osd_up_shift;
	uint32_t osd_left_shift;
};

struct attr_osd {
	uint32_t attr_id;
	int number; /* default 6 */
	struct osd_block *blocks;
	uint8_t count_of_supported_font_type;
	/*uint32_t double_width_char_addr;*/	/* addr should set by this lib */
	/*uint32_t time_info_addr; */	/* addr should set by this lib */
	uint8_t time_am_pm_enable;	/* only for soc */
	uint8_t date_display_enable;	/* only for soc */
	uint8_t time_display_enable;	/* only for soc */
	/*uint32_t date_info_addr; */	/* addr should set by this lib */
	/*uint32_t config_info_addr;*/	/* this is fw const var*/

	uint32_t reserved[4];
};


enum bitrate_mode {
	BITRATE_MODE_CBR	= (1 << 1),
	BITRATE_MODE_VBR	= (1 << 2),
	BITRATE_MODE_C_VBR	= (1 << 3),
};

struct attr_video_encoder {
	uint32_t attr_id;
	uint32_t codec_id;
	uint32_t support_bitrate_mode;
	uint32_t bitrate_mode;

	uint32_t bitrate;
	uint32_t max_bitrate;
	uint32_t min_bitrate;

	uint32_t qp;
	uint32_t max_qp;
	uint32_t min_qp;

	uint32_t gop;
	uint32_t max_gop;
	uint32_t min_gop;

	uint32_t reserved[4];
};

enum event_mode {
	EVENT_MOTION_DETECT = (1 << 1),
	EVENT_VOICE_DETECT = (1 << 2),
};

int rts_cam_open(const char *name);
void rts_cam_close(int fd);

int rts_cam_lock(int fd, int time_out);
int rts_cam_unlock(int fd);

int rts_cam_get_stream_attr(int fd, struct attr_stream **attr);
int rts_cam_set_stream_attr(int fd, struct attr_stream *attr);

int rts_cam_get_video_attr(int fd, struct attr_video **attr);
int rts_cam_set_video_attr(int fd, struct attr_video *attr);

int rts_cam_get_md_attr(int fd, struct attr_md **attr);
int rts_cam_set_md_attr(int fd, struct attr_md *attr);

int rts_cam_get_mask_attr(int fd, struct attr_mask **attr);
int rts_cam_set_mask_attr(int fd, struct attr_mask *attr);

int rts_cam_get_osd_attr(int fd, struct attr_osd **attr);
int rts_cam_set_osd_attr(int fd, struct attr_osd *attr);

int rts_cam_get_video_encoder_attr(int fd, struct attr_video_encoder **attr);
int rts_cam_set_video_encoder_attr(int fd, struct attr_video_encoder *attr);

int rts_cam_get_events(int fd, int event_id, uint32_t *property);

void rts_cam_free_attr(void *attr);

#ifdef __cplusplus
}
#endif

#endif
