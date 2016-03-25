#include <rts_cam.h>
#include <octopus.h>
#include <linux/videodev2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "rts_uvc.h"
#include "isp.h"
#include "reg_addr.h"

#define MAKEWORD(a, b) ((uint16_t) (((uint8_t) (a)) | ((uint16_t) ((uint8_t) (b))) << 8))
#define MAX_PIXEL_IN_RECT (8192)

#define V4L2_PIX_FMT_H264     v4l2_fourcc('H', '2', '6', '4') /* H264 with start codes */


int rts_cam_open(const char *name)
{
	int ret = 0;
	ret = opt_open_dev(name);
	return ret;
}

void rts_cam_close(int fd)
{
	opt_close_dev(fd);
}

int rts_cam_lock(int fd, int time_out)
{
	int ret = 0;
	if (0 == time_out)
		ret = opt_lock_dev(fd);
	else
		ret = opt_trylock_dev(fd, time_out);
	return ret;
}

int rts_cam_unlock(int fd)
{
	int ret = 0;
	ret = opt_unlock_dev(fd);
	return ret;
}

static void get_fps_by_format_resolution(int fd,
		uint32_t pixelformat,
		struct v4l2_frmsizeenum *frmsize,
		struct video_resolution **resolution)
{
	int ret;
	struct v4l2_frmivalenum fival;

	memset(&fival, 0, sizeof(fival));
	fival.index = 0;
	fival.pixel_format = pixelformat;

	if (V4L2_FRMSIZE_TYPE_DISCRETE == frmsize->type) {
		fival.width = frmsize->discrete.width;
		fival.height = frmsize->discrete.height;
	} else {
		;
	}

	(*resolution)->frmivals = (struct video_frmival *)calloc(1,
					sizeof(struct video_frmival));

	while (!(ret = v4l2_ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival))) {
		if (fival.index)
			(*resolution)->frmivals =
			(struct video_frmival *)realloc((*resolution)->frmivals,
			(fival.index + 1) * sizeof(struct video_frmival));
		(*resolution)->number = fival.index + 1;
		(*resolution)->frmivals[fival.index].type = fival.type;
		if (V4L2_FRMIVAL_TYPE_DISCRETE == fival.type) {
			(*resolution)->frmivals[fival.index].discrete.numerator =
							fival.discrete.numerator;
			(*resolution)->frmivals[fival.index].discrete.denominator =
							fival.discrete.denominator;
		} else if (V4L2_FRMIVAL_TYPE_STEPWISE == fival.type) {
			(*resolution)->frmivals[fival.index].stepwise.min.numerator =
							fival.stepwise.min.numerator;
			(*resolution)->frmivals[fival.index].stepwise.min.denominator =
							fival.stepwise.min.denominator;
			(*resolution)->frmivals[fival.index].stepwise.max.numerator =
							fival.stepwise.max.numerator;
			(*resolution)->frmivals[fival.index].stepwise.max.denominator =
							fival.stepwise.max.denominator;
			(*resolution)->frmivals[fival.index].stepwise.step.numerator =
							fival.stepwise.step.numerator;
			(*resolution)->frmivals[fival.index].stepwise.step.denominator =
							fival.stepwise.step.denominator;
		}
		fival.index++;
	}
}

static void get_supported_resolution_by_format(int fd,
		struct video_format **format)
{
	int ret;
	struct v4l2_frmsizeenum fsize;

	memset(&fsize, 0, sizeof(fsize));
	fsize.index = 0;
	fsize.pixel_format = (*format)->format;
	(*format)->resolutions = (struct video_resolution *)calloc(1,
				sizeof(struct video_resolution));

	while (!(ret = v4l2_ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsize))) {
		if (fsize.index)
			(*format)->resolutions =
			(struct video_resolution *)realloc((*format)->resolutions,
			(fsize.index + 1) * sizeof(struct video_resolution));
		(*format)->number = fsize.index + 1;
		(*format)->resolutions[fsize.index].type = fsize.type;

		if (V4L2_FRMSIZE_TYPE_DISCRETE == fsize.type) {
			(*format)->resolutions[fsize.index].discrete.width =
							fsize.discrete.width;
			(*format)->resolutions[fsize.index].discrete.height =
							fsize.discrete.height;
		} else if (V4L2_FRMSIZE_TYPE_STEPWISE == fsize.type) {
			(*format)->resolutions[fsize.index].stepwise.min_width =
							fsize.stepwise.min_width;
			(*format)->resolutions[fsize.index].stepwise.max_width =
							fsize.stepwise.max_width;
			(*format)->resolutions[fsize.index].stepwise.step_width =
							fsize.stepwise.step_width;
			(*format)->resolutions[fsize.index].stepwise.min_height =
							fsize.stepwise.min_height;
			(*format)->resolutions[fsize.index].stepwise.max_height =
							fsize.stepwise.max_height;
			(*format)->resolutions[fsize.index].stepwise.step_height =
							fsize.stepwise.step_height;
		}
		struct video_resolution *video_resolution_tmp =
				&((*format)->resolutions[fsize.index]);
		get_fps_by_format_resolution(fd, (*format)->format,
				&fsize, &video_resolution_tmp);
		fsize.index++;
	}
}

static int get_supported_format_resolution_fps(int fd,
	struct attr_stream **stream_attr)
{
	int ret = 0;
	struct v4l2_fmtdesc fmtdesc;

	memset(&fmtdesc, 0, sizeof(fmtdesc));
	fmtdesc.index = 0;
	fmtdesc.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	(*stream_attr)->formats = (struct video_format *)calloc(1,
				sizeof(struct video_format));
	int flag = 0;
	/*output all video format supported by video device*/
	do {
		flag = v4l2_ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);
		if (0 == flag) {
			if (fmtdesc.index)
				(*stream_attr)->formats =
				(struct video_format *)realloc((*stream_attr)->formats,
				(fmtdesc.index + 1) * sizeof(struct video_format));
			(*stream_attr)->formats[fmtdesc.index].format =
							fmtdesc.pixelformat;
			(*stream_attr)->number = fmtdesc.index + 1;
			/*if (!strcmp("H264", fmtdesc.description))*/
			struct video_format *format_tmp =
				&((*stream_attr)->formats[fmtdesc.index]);
			get_supported_resolution_by_format(fd, &format_tmp);
		} else {
			fprintf(stderr, "%s\n", strerror(-ret));
			break;
		}
		fmtdesc.index++;
	} while (0 <= flag);

	return ret;
}

static int get_cur_format_resolution_fps(int fd,
			struct attr_stream *stream_attr)
{
	/* need change later */
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

	stream_attr->current.format = fmt.fmt.pix.pixelformat;
	stream_attr->current.resolution.discrete.width = fmt.fmt.pix.width;
	stream_attr->current.resolution.discrete.height = fmt.fmt.pix.height;

	struct v4l2_streamparm stream_param;
	memset(&stream_param, 0, sizeof(struct v4l2_streamparm));
	stream_param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = v4l2_ioctl(fd, VIDIOC_G_PARM, &stream_param);
	if (ret < 0) {
		fprintf(stderr, "Get Parm failed\n");
		goto out;
	}
	stream_attr->current.frmival.discrete.numerator =
		stream_param.parm.capture.timeperframe.numerator;
	stream_attr->current.frmival.discrete.denominator =
		stream_param.parm.capture.timeperframe.denominator;
out:
	return ret;
}


int rts_cam_get_stream_attr(int fd, struct attr_stream **attr)
{
	int ret = 0;

	struct attr_stream *stream_attr = calloc(1, sizeof(*stream_attr));
	memset(stream_attr, 0, sizeof(*stream_attr));

	stream_attr->attr_id = ATTR_ID_STREAM;
	ret = get_supported_format_resolution_fps(fd, &stream_attr);
	if (ret)
		goto out;
	ret = get_cur_format_resolution_fps(fd, stream_attr);
	if (ret)
		goto out;

	*attr = stream_attr;
out:
	return ret;
}

int rts_cam_set_stream_attr(int fd, struct attr_stream *attr)
{
	int ret = 0;
	char fps_buf[16] = {0};
	char resolution_buf[32] = {0};
	char pixformat_buf[16] = {0};
	struct v4l2_format stream_pxl_format;
	memset(&stream_pxl_format, 0, sizeof(stream_pxl_format));
	struct v4l2_streamparm stream_fps;
	memset(&stream_fps, 0, sizeof(stream_fps));

	stream_pxl_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	/* Get format first */
	ret = v4l2_ioctl(fd, VIDIOC_G_FMT, &stream_pxl_format);
	if (ret) {
		fprintf(stderr, "VIDIOC_G_FMT failed\n");
		goto out;
	}
	/* update format */
	if (V4L2_PIX_FMT_H264 == attr->current.format)
		stream_pxl_format.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
	else if (V4L2_PIX_FMT_NV12 == attr->current.format)
		stream_pxl_format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
	else if (V4L2_PIX_FMT_MJPEG == attr->current.format)
		stream_pxl_format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;

	/* if (V4L2_FRMSIZE_TYPE_DISCRETE == attr->current.resolution.type)*/
	stream_pxl_format.fmt.pix.width =
		attr->current.resolution.discrete.width;
	stream_pxl_format.fmt.pix.height =
		attr->current.resolution.discrete.height;
	/* set format */
	ret = v4l2_ioctl(fd, VIDIOC_S_FMT, &stream_pxl_format);
	if (ret) {
		fprintf(stderr, "VIDIOC_S_FMT failed\n");
		goto out;
	}

	stream_fps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = v4l2_ioctl(fd, VIDIOC_G_PARM, &stream_fps);
	if (ret) {
		fprintf(stderr, "VIDIOC_G_PARM failed\n");
		goto out;
	}

	/*if (V4L2_FRMIVAL_TYPE_DISCRETE == attr->current.frmival.type)*/
	stream_fps.parm.capture.timeperframe.denominator =
		attr->current.frmival.discrete.denominator;

	ret = v4l2_ioctl(fd, VIDIOC_S_PARM, &stream_fps);
	if (ret) {
		fprintf(stderr, "VIDIOC_S_PARM failed\n");
		goto out;
	}
out:
	return ret;
}

int rts_cam_get_video_attr(int fd, struct attr_video **attr)
{
	int ret = 0;
	int cur_val = 0 ;
	int max_val = 0, min_val = 0, step = 0, default_val = 0;

	struct attr_video *video_ctrl_attr = calloc(1,
				sizeof(*video_ctrl_attr));
	memset(video_ctrl_attr, 0, sizeof(*video_ctrl_attr));

	video_ctrl_attr->attr_id = ATTR_ID_VIDEO;
	/* brightness ctrl*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_BRIGHTNESS,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->brightness.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->brightness.minimum = min_val;
	video_ctrl_attr->brightness.maximum = max_val;
	video_ctrl_attr->brightness.step = step;
	video_ctrl_attr->brightness.default_val = default_val;
	video_ctrl_attr->brightness.current = cur_val;
	/* contrast ctrl*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_CONTRAST,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->contrast.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->contrast.minimum = min_val;
	video_ctrl_attr->contrast.maximum = max_val;
	video_ctrl_attr->contrast.step = step;
	video_ctrl_attr->contrast.default_val = default_val;
	video_ctrl_attr->contrast.current = cur_val;
	/* hue ctrl*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_HUE,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->hue.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->hue.minimum = min_val;
	video_ctrl_attr->hue.maximum = max_val;
	video_ctrl_attr->hue.step = step;
	video_ctrl_attr->hue.default_val = default_val;
	video_ctrl_attr->hue.current = cur_val;
	/* saturation ctrl*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_SATURATION,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->saturation.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->saturation.minimum = min_val;
	video_ctrl_attr->saturation.maximum = max_val;
	video_ctrl_attr->saturation.step = step;
	video_ctrl_attr->saturation.default_val = default_val;
	video_ctrl_attr->saturation.current = cur_val;
	/* sharpness ctrl*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_SHARPNESS,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->sharpness.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->sharpness.minimum = min_val;
	video_ctrl_attr->sharpness.maximum = max_val;
	video_ctrl_attr->sharpness.step = step;
	video_ctrl_attr->sharpness.default_val = default_val;
	video_ctrl_attr->sharpness.current = cur_val;
	/* gamma ctrl*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_GAMMA,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->gamma.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->gamma.minimum = min_val;
	video_ctrl_attr->gamma.maximum = max_val;
	video_ctrl_attr->gamma.step = step;
	video_ctrl_attr->gamma.default_val = default_val;
	video_ctrl_attr->gamma.current = cur_val;
	/* auto white balance ctrl*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_AUTO_WHITE_BALANCE,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->auto_white_balance.type = VIDEO_CTRL_TYPE_BOOLEAN;
	video_ctrl_attr->auto_white_balance.minimum = min_val;
	video_ctrl_attr->auto_white_balance.maximum = max_val;
	video_ctrl_attr->auto_white_balance.step = step;
	video_ctrl_attr->auto_white_balance.default_val = default_val;
	video_ctrl_attr->auto_white_balance.current = cur_val;
	if (!video_ctrl_attr->auto_white_balance.current) {
		/* white balance temperature*/
		/* must check awb first, or will output error*/
		ret = isp_uvc_get_ctrl(fd, V4L2_CID_WHITE_BALANCE_TEMPERATURE,
			&cur_val, &max_val, &min_val, &step, &default_val);
		if (ret)
			goto out;
		video_ctrl_attr->white_balance_temperature.type =
						VIDEO_CTRL_TYPE_INTEGER;
		video_ctrl_attr->white_balance_temperature.minimum = min_val;
		video_ctrl_attr->white_balance_temperature.maximum = max_val;
		video_ctrl_attr->white_balance_temperature.step = step;
		video_ctrl_attr->white_balance_temperature.default_val = default_val;
		video_ctrl_attr->white_balance_temperature.current = cur_val;
	}
	/* power_line_frequency*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_POWER_LINE_FREQUENCY,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->power_line_frequency.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->power_line_frequency.minimum = min_val;
	video_ctrl_attr->power_line_frequency.maximum = max_val;
	video_ctrl_attr->power_line_frequency.step = step;
	video_ctrl_attr->power_line_frequency.default_val = default_val;
	video_ctrl_attr->power_line_frequency.current = cur_val;
	/* flip */
	/*v4l2 does not support this two ctrls*/
	/*
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_VFLIP,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	*/
	min_val = 0;
	max_val = 1;
	step = 1;
	ret = isp_get_flip(fd, &cur_val, NULL);
	if (ret)
		goto out;
	video_ctrl_attr->flip.type = VIDEO_CTRL_TYPE_BOOLEAN;
	video_ctrl_attr->flip.minimum = min_val;
	video_ctrl_attr->flip.maximum = max_val;
	video_ctrl_attr->flip.step = step;
	video_ctrl_attr->flip.default_val = default_val;
	video_ctrl_attr->flip.current = cur_val;
	/* mirror */
	min_val = 0;
	max_val = 1;
	step = 1;
	/*
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_HFLIP,
		&cur_val, &max_val, &min_val, &step, &default_val);
	*/
	ret = isp_get_mirror(fd, &cur_val, NULL);
	if (ret)
		goto out;
	video_ctrl_attr->mirror.type = VIDEO_CTRL_TYPE_BOOLEAN;
	video_ctrl_attr->mirror.minimum = min_val;
	video_ctrl_attr->mirror.maximum = max_val;
	video_ctrl_attr->mirror.step = step;
	video_ctrl_attr->mirror.default_val = default_val;
	video_ctrl_attr->mirror.current = cur_val;

#if 0
	/* backlight compensate*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_BACKLIGHT_COMPENSATION,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->backlight_compensate.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->backlight_compensate.minimum = min_val;
	video_ctrl_attr->backlight_compensate.maximum = max_val;
	video_ctrl_attr->backlight_compensate.step = step;
	video_ctrl_attr->backlight_compensate.default_val = default_val;
	video_ctrl_attr->backlight_compensate.current = cur_val;
	/* gain*/
	/*
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_GAIN,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->gain.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->gain.minimum = min_val;
	video_ctrl_attr->gain.maximum = max_val;
	video_ctrl_attr->gain.step = step;
	video_ctrl_attr->gain.default_val = default_val;
	video_ctrl_attr->gain.current = cur_val;
	*/
	/* exposure_mode*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_EXPOSURE_AUTO,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->exposure_mode.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->exposure_mode.minimum = min_val;
	video_ctrl_attr->exposure_mode.maximum = max_val;
	video_ctrl_attr->exposure_mode.step = step;
	video_ctrl_attr->exposure_mode.default_val = default_val;
	video_ctrl_attr->exposure_mode.current = cur_val;
	/* exposure_priority*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_EXPOSURE_AUTO_PRIORITY,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->exposure_priority.type = VIDEO_CTRL_TYPE_BOOLEAN;
	video_ctrl_attr->exposure_priority.minimum = min_val;
	video_ctrl_attr->exposure_priority.maximum = max_val;
	video_ctrl_attr->exposure_priority.step = step;
	video_ctrl_attr->exposure_priority.default_val = default_val;
	video_ctrl_attr->exposure_priority.current = cur_val;
	/* exposure_time*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_EXPOSURE,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->exposure_time.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->exposure_time.minimum = min_val;
	video_ctrl_attr->exposure_time.maximum = max_val;
	video_ctrl_attr->exposure_time.step = step;
	video_ctrl_attr->exposure_time.default_val = default_val;
	video_ctrl_attr->exposure_time.current = cur_val;
	/* auto_foucus*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_FOCUS_AUTO,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->auto_focus.type = VIDEO_CTRL_TYPE_BOOLEAN;
	video_ctrl_attr->auto_focus.minimum = min_val;
	video_ctrl_attr->auto_focus.maximum = max_val;
	video_ctrl_attr->auto_focus.step = step;
	video_ctrl_attr->auto_focus.default_val = default_val;
	video_ctrl_attr->auto_focus.current = cur_val;
	/* foucus*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_FOCUS_ABSOLUTE,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->focus.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->focus.minimum = min_val;
	video_ctrl_attr->focus.maximum = max_val;
	video_ctrl_attr->focus.step = step;
	video_ctrl_attr->focus.default_val = default_val;
	video_ctrl_attr->focus.current = cur_val;
	/* zoom*/
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_ZOOM_CONTINUOUS,
		&cur_val, &max_val, &min_val, &step, &default_val);
	if (ret)
		goto out;
	video_ctrl_attr->zoom.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->zoom.minimum = min_val;
	video_ctrl_attr->zoom.maximum = max_val;
	video_ctrl_attr->zoom.step = step;
	video_ctrl_attr->zoom.default_val = default_val;
	video_ctrl_attr->zoom.current = cur_val;

#endif
	/* rotate*/
	/*
	ret = isp_uvc_get_ctrl(fd, V4L2_CID_ROTATE,
		&cur_val, &max_val, &min_val, &step, &default_val);
	video_ctrl_attr->rotate.type = VIDEO_CTRL_TYPE_INTEGER;
	video_ctrl_attr->rotate.minimum = min_val;
	video_ctrl_attr->rotate.maximum = max_val;
	video_ctrl_attr->rotate.step = step;
	video_ctrl_attr->rotate.default_val = default_val;
	video_ctrl_attr->rotate.current = cur_val;
	*/
	/*didnot figure out how does the following items work?*/
	/*
	pan_tilt_ctl pan_tilt;
	roll_ctl roll;
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
	*/

	*attr = video_ctrl_attr;
out:
	return ret;
}

int rts_cam_set_video_attr(int fd, struct attr_video *attr)
{
	int ret = 0;
	/* brightness ctrl*/
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_BRIGHTNESS,
			attr->brightness.current);
	if (ret)
		goto out;
	/* contrast ctrl*/
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_CONTRAST,
			attr->contrast.current);
	if (ret)
		goto out;
	/* hue ctrl*/
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_HUE,
			attr->hue.current);
	if (ret)
		goto out;
	/* saturation ctrl*/
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_SATURATION,
			attr->saturation.current);
	if (ret)
		goto out;
	/* sharpness ctrl*/
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_SHARPNESS,
			attr->sharpness.current);
	if (ret)
		goto out;
	/* gamma ctrl*/
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_GAMMA,
			attr->gamma.current);
	if (ret)
		goto out;
	/* auto white balance ctrl*/
	ret = isp_uvc_set_ctrl(fd,
			V4L2_CID_AUTO_WHITE_BALANCE,
			attr->auto_white_balance.current);
	if (ret)
		goto out;
	if (!attr->auto_white_balance.current) {
		/* white balance temperature*/
		ret = isp_uvc_set_ctrl(fd,
			V4L2_CID_WHITE_BALANCE_TEMPERATURE,
			attr->white_balance_temperature.current);
		if (ret)
			goto out;
	}
	/* power_line_frequency*/
	ret = isp_uvc_set_ctrl(fd,
			V4L2_CID_POWER_LINE_FREQUENCY,
			attr->power_line_frequency.current);
	if (ret)
		goto out;
	/* flip */
	/*ret = isp_uvc_set_ctrl(fd, V4L2_CID_VFLIP, attr->flip.current);*/
	ret = isp_set_flip(fd, attr->flip.current);
	if (ret)
		goto out;
	/* mirror */
	/*ret = isp_uvc_set_ctrl(fd, V4L2_CID_HFLIP, attr->mirror.current);*/
	ret = isp_set_mirror(fd, attr->mirror.current);
	if (ret)
		goto out;

#if 0
	/* backlight compensate*/
	ret = isp_uvc_set_ctrl(fd,
			V4L2_CID_BACKLIGHT_COMPENSATION,
			attr->backlight_compensate.current);
	if (ret)
		goto out;
	/* gain*/
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_GAIN, attr->gain.current);
	if (ret)
		goto out;
	/* exposure_mode*/
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_EXPOSURE_AUTO,
			attr->exposure_mode.current);
	if (ret)
		goto out;
	/* exposure_priority*/
	ret = isp_uvc_set_ctrl(fd,
			V4L2_CID_EXPOSURE_AUTO_PRIORITY,
			attr->exposure_priority.current);
	if (ret)
		goto out;
	/* exposure_time*/
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_EXPOSURE,
			attr->exposure_time.current);
	if (ret)
		goto out;
	/* auto_foucus*/
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_FOCUS_AUTO,
			attr->auto_focus.current);
	if (ret)
		goto out;
	/* foucus*/
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_FOCUS_ABSOLUTE,
			attr->focus.current);
	if (ret)
		goto out;
	/* zoom*/
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_ZOOM_CONTINUOUS,
			attr->zoom.current);
	if (ret)
		goto out;
	/* rotate*/
	/* toolchain didnot support this cmd
	ret = isp_uvc_set_ctrl(fd, V4L2_CID_ROTATE, attr->rotate.current);
	*/
	/*didnot figure out how does the following items work?*/
	/*
	pan_tilt_ctl pan_tilt;
	roll_ctl roll;
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
	*/
#endif
out:
	return ret;
}

int rts_cam_get_md_attr(int fd, struct attr_md **attr)
{
	int ret = 0;
	uint8_t rect1_thd = 0;
	uint8_t rect2_thd = 0;
	uint8_t rect3_thd = 0;
	uint8_t md_enabled = 0;
	uint8_t md_tmp_buf[8] = { 0 };
	struct attr_md *attr_motion_detect = NULL;

	attr_motion_detect = calloc(1, sizeof(*attr_motion_detect));
	memset(attr_motion_detect, 0, sizeof(*attr_motion_detect));

	attr_motion_detect->attr_id = ATTR_ID_MOTION_DETECT;
	attr_motion_detect->number = MAX_MD_MODULE_NUM;
	attr_motion_detect->blocks = calloc(MAX_MD_MODULE_NUM,
					sizeof(struct md_block));
	memset(attr_motion_detect->blocks, 0,
		MAX_MD_MODULE_NUM * sizeof(struct md_block));

	attr_motion_detect->blocks[0].type = MD_BLOCK_TYPE_RECT;
	attr_motion_detect->blocks[1].type = MD_BLOCK_TYPE_RECT;
	attr_motion_detect->blocks[2].type = MD_BLOCK_TYPE_RECT;
	/* get motion detect enable option */
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MTD_CTRL, &md_enabled, 1, 0);
	if (ret)
		goto out;

	attr_motion_detect->blocks[0].enable = md_enabled & 0x01;
	attr_motion_detect->blocks[1].enable = (md_enabled & 0x02) >> 1;
	attr_motion_detect->blocks[2].enable = (md_enabled & 0x04) >> 2;

	/* get motion detect module sensitivity */
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MTD_thdp0, &rect1_thd, 1, 0);
	if (ret)
		goto out;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MTD_thdp1, &rect2_thd, 1, 0);
	if (ret)
		goto out;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MTD_thdp2, &rect3_thd, 1, 0);
	if (ret)
		goto out;

	attr_motion_detect->blocks[0].sensitivity =
			(1.0 - rect1_thd/127.)*100 + 0.5;
	attr_motion_detect->blocks[1].sensitivity =
			(1.0 - rect2_thd/127.)*100 + 0.5;
	attr_motion_detect->blocks[2].sensitivity =
			(1.0 - rect3_thd/127.)*100 + 0.5;

	/* get motion detect module rect position */
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MTD_START0_X, md_tmp_buf, 8, 0);
	if (ret)
		goto out;
	attr_motion_detect->blocks[0].rect.left =
		MAKEWORD(md_tmp_buf[1], md_tmp_buf[0] & 0x0F);
	attr_motion_detect->blocks[0].rect.top =
		MAKEWORD(md_tmp_buf[3], md_tmp_buf[2] & 0x0F);
	attr_motion_detect->blocks[0].rect.right =
		MAKEWORD(md_tmp_buf[5], md_tmp_buf[4] & 0x0F);
	attr_motion_detect->blocks[0].rect.bottom =
		MAKEWORD(md_tmp_buf[7], md_tmp_buf[6] & 0x0F);
	memset(md_tmp_buf, 0, sizeof(md_tmp_buf));

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MTD_START1_X, md_tmp_buf, 8, 0);
	if (ret)
		goto out;
	attr_motion_detect->blocks[1].rect.left =
		MAKEWORD(md_tmp_buf[1], md_tmp_buf[0] & 0x0F);
	attr_motion_detect->blocks[1].rect.top =
		MAKEWORD(md_tmp_buf[3], md_tmp_buf[2] & 0x0F);
	attr_motion_detect->blocks[1].rect.right =
		MAKEWORD(md_tmp_buf[5], md_tmp_buf[4] & 0x0F);
	attr_motion_detect->blocks[1].rect.bottom =
		MAKEWORD(md_tmp_buf[7], md_tmp_buf[6] & 0x0F);
	memset(md_tmp_buf, 0, sizeof(md_tmp_buf));

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MTD_START2_X, md_tmp_buf, 8, 0);
	if (ret)
		goto out;
	attr_motion_detect->blocks[2].rect.left =
		MAKEWORD(md_tmp_buf[1], md_tmp_buf[0] & 0x0F);
	attr_motion_detect->blocks[2].rect.top =
		MAKEWORD(md_tmp_buf[3], md_tmp_buf[2] & 0x0F);
	attr_motion_detect->blocks[2].rect.right =
		MAKEWORD(md_tmp_buf[5], md_tmp_buf[4] & 0x0F);
	attr_motion_detect->blocks[2].rect.bottom =
		MAKEWORD(md_tmp_buf[7], md_tmp_buf[6] & 0x0F);

	*attr = attr_motion_detect;
out:
	return ret;
}

int rts_cam_set_md_attr(int fd, struct attr_md *attr)
{
	int ret = 0;
	int sensitivity1 = 0;
	int sensitivity2 = 0;
	int sensitivity3 = 0;
	uint8_t rect1_thd = 0;
	uint8_t rect2_thd = 0;
	uint8_t rect3_thd = 0;
	uint8_t md_enabled = 0;
	uint8_t md_tmp_buf[8] = { 0 };
	int rect_area = 0;
	uint8_t tmp_data = 0;

	assert(attr);
	assert(attr->number == MAX_MD_MODULE_NUM);
	assert(attr->blocks);
	assert(attr->blocks[0].type == MD_BLOCK_TYPE_RECT);
	assert(attr->blocks[1].type == MD_BLOCK_TYPE_RECT);
	assert(attr->blocks[2].type == MD_BLOCK_TYPE_RECT);

	/* set motion detect enable option */
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MTD_CTRL, &md_enabled, 1, 0);
	if (ret)
		goto out;

	if (attr->blocks[0].enable)
		md_enabled |= 0x01;
	else
		md_enabled &= 0xFE;

	if (attr->blocks[1].enable)
		md_enabled |= 0x02;
	else
		md_enabled &= 0xFD;

	if (attr->blocks[2].enable)
		md_enabled |= 0x04;
	else
		md_enabled &= 0xFB;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
		ISP_MTD_CTRL, &md_enabled, 1, 0);
	if (ret)
		goto out;

	/* set motion detect module sensitivity */
	sensitivity1 = attr->blocks[0].sensitivity;
	sensitivity2 = attr->blocks[1].sensitivity;
	sensitivity3 = attr->blocks[2].sensitivity;
	sensitivity1 = sensitivity1 < 0 ? 0 : sensitivity1;
	sensitivity2 = sensitivity2 < 0 ? 0 : sensitivity2;
	sensitivity3 = sensitivity3 < 0 ? 0 : sensitivity3;
	sensitivity1 = sensitivity1 > 100 ? 100 : sensitivity1;
	sensitivity2 = sensitivity2 > 100 ? 100 : sensitivity2;
	sensitivity3 = sensitivity3 > 100 ? 100 : sensitivity3;
	rect1_thd = (uint8_t)((100 - sensitivity1) * 127 / 100. + 0.5);
	rect2_thd = (uint8_t)((100 - sensitivity2) * 127 / 100. + 0.5);
	rect3_thd = (uint8_t)((100 - sensitivity3) * 127 / 100. + 0.5);

	attr->blocks[0].sensitivity = sensitivity1;
	attr->blocks[1].sensitivity = sensitivity2;
	attr->blocks[2].sensitivity = sensitivity3;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
		ISP_MTD_thdp0, &rect1_thd, 1, 0);
	if (ret)
		goto out;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
		ISP_MTD_thdp1, &rect2_thd, 1, 0);
	if (ret)
		goto out;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
		ISP_MTD_thdp2, &rect3_thd, 1, 0);
	if (ret)
		goto out;

	/* set motion detect module rect position */
	/* set rect 1 rect position */
	md_tmp_buf[0] = (attr->blocks[0].rect.left >> 8) & 0x0F;
	md_tmp_buf[1] = (attr->blocks[0].rect.left) & 0xFF;
	md_tmp_buf[2] = (attr->blocks[0].rect.top >> 8) & 0x0F;
	md_tmp_buf[3] = (attr->blocks[0].rect.top) & 0xFF;
	md_tmp_buf[4] = (attr->blocks[0].rect.right >> 8) & 0x0F;
	md_tmp_buf[5] = (attr->blocks[0].rect.right) & 0xFF;
	md_tmp_buf[6] = (attr->blocks[0].rect.bottom >> 8) & 0x0F;
	md_tmp_buf[7] = (attr->blocks[0].rect.bottom) & 0xFF;
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
		ISP_MTD_START0_X, md_tmp_buf, 8, 0);
	if (ret)
		goto out;
	/* set rect 1 sample mode*/
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MTD_SAMPLE, &tmp_data, 1, 0);
	if (ret)
		goto out;

	rect_area = (attr->blocks[0].rect.right - attr->blocks[0].rect.left) *
		(attr->blocks[0].rect.bottom - attr->blocks[0].rect.top);
	check_sample_mode(&tmp_data, 0, rect_area);
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
		ISP_MTD_SAMPLE, &tmp_data, 1, 0);
	if (ret)
		goto out;
	tmp_data = 0;

	/* set rect 2 rect position */
	memset(md_tmp_buf, 0, sizeof(md_tmp_buf));
	md_tmp_buf[0] = (attr->blocks[1].rect.left >> 8) & 0x0F;
	md_tmp_buf[1] = (attr->blocks[1].rect.left) & 0xFF;
	md_tmp_buf[2] = (attr->blocks[1].rect.top >> 8) & 0x0F;
	md_tmp_buf[3] = (attr->blocks[1].rect.top) & 0xFF;
	md_tmp_buf[4] = (attr->blocks[1].rect.right >> 8) & 0x0F;
	md_tmp_buf[5] = (attr->blocks[1].rect.right) & 0xFF;
	md_tmp_buf[6] = (attr->blocks[1].rect.bottom >> 8) & 0x0F;
	md_tmp_buf[7] = (attr->blocks[1].rect.bottom) & 0xFF;
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
		ISP_MTD_START1_X, md_tmp_buf, 8, 0);
	if (ret)
		goto out;
	/* set rect 2 sample mode*/
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MTD_SAMPLE, &tmp_data, 1, 0);
	if (ret)
		goto out;

	rect_area = (attr->blocks[1].rect.right - attr->blocks[1].rect.left) *
		(attr->blocks[1].rect.bottom - attr->blocks[1].rect.top);
	check_sample_mode(&tmp_data, 1, rect_area);
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
		ISP_MTD_SAMPLE, &tmp_data, 1, 0);
	if (ret)
		goto out;
	tmp_data = 0;


	/* set rect 3 rect position */
	memset(md_tmp_buf, 0, sizeof(md_tmp_buf));
	md_tmp_buf[0] = (attr->blocks[2].rect.left >> 8) & 0x0F;
	md_tmp_buf[1] = (attr->blocks[2].rect.left) & 0xFF;
	md_tmp_buf[2] = (attr->blocks[2].rect.top >> 8) & 0x0F;
	md_tmp_buf[3] = (attr->blocks[2].rect.top) & 0xFF;
	md_tmp_buf[4] = (attr->blocks[2].rect.right >> 8) & 0x0F;
	md_tmp_buf[5] = (attr->blocks[2].rect.right) & 0xFF;
	md_tmp_buf[6] = (attr->blocks[2].rect.bottom >> 8) & 0x0F;
	md_tmp_buf[7] = (attr->blocks[2].rect.bottom) & 0xFF;
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
		ISP_MTD_START2_X, md_tmp_buf, 8, 0);
	if (ret)
		goto out;
	/* set rect 3 sample mode*/
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MTD_SAMPLE, &tmp_data, 1, 0);
	if (ret)
		goto out;

	rect_area = (attr->blocks[2].rect.right - attr->blocks[2].rect.left) *
		(attr->blocks[2].rect.bottom - attr->blocks[2].rect.top);
	check_sample_mode(&tmp_data, 2, rect_area);
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
		ISP_MTD_SAMPLE, &tmp_data, 1, 0);
	if (ret)
		goto out;
	tmp_data = 0;

out:
	return ret;
}

int rts_cam_get_mask_attr(int fd, struct attr_mask **attr)
{
	int ret = 0;
	uint8_t mask_enabled = 0;
	uint8_t mask_tmp_buf[8] = { 0 };
	uint32_t color = 0;
	struct attr_mask *attr_priv_mask = NULL;

	attr_priv_mask = calloc(1, sizeof(*attr_priv_mask));
	memset(attr_priv_mask, 0, sizeof(*attr_priv_mask));

	attr_priv_mask->attr_id = ATTR_ID_MASK;
	attr_priv_mask->number = MAX_MASK_MODULE_NUM;
	attr_priv_mask->blocks = calloc(MAX_MASK_MODULE_NUM,
			sizeof(struct mask_block));
	memset(attr_priv_mask->blocks, 0,
		MAX_MASK_MODULE_NUM * sizeof(struct mask_block));

	attr_priv_mask->blocks[0].type = MASK_BLOCK_TYPE_RECT;
	attr_priv_mask->blocks[1].type = MASK_BLOCK_TYPE_RECT;
	attr_priv_mask->blocks[2].type = MASK_BLOCK_TYPE_RECT;
	/* get private mask enable option */
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
			ISP_MSK_CTRL, &mask_enabled, 1, 0);
	if (ret)
		goto out;

	attr_priv_mask->blocks[0].enable = mask_enabled & 0x01;
	attr_priv_mask->blocks[1].enable = (mask_enabled & 0x02) >> 1;
	attr_priv_mask->blocks[2].enable = (mask_enabled & 0x04) >> 2;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
			ISP_MSK_Y, mask_tmp_buf, 3, 0);
	if (ret)
		goto out;

	yuv2color(mask_tmp_buf[0], mask_tmp_buf[1], mask_tmp_buf[2], &color);
	attr_priv_mask->blocks[0].color = color;
	attr_priv_mask->blocks[1].color = color;
	attr_priv_mask->blocks[2].color = color;

	/* get mask module rect position */
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MSK_START0_X, mask_tmp_buf, 8, 0);
	if (ret)
		goto out;
	attr_priv_mask->blocks[0].rect.left =
		MAKEWORD(mask_tmp_buf[1], mask_tmp_buf[0] & 0x0F);
	attr_priv_mask->blocks[0].rect.top =
		MAKEWORD(mask_tmp_buf[3], mask_tmp_buf[2] & 0x0F);
	attr_priv_mask->blocks[0].rect.right =
		MAKEWORD(mask_tmp_buf[5], mask_tmp_buf[4] & 0x0F);
	attr_priv_mask->blocks[0].rect.bottom =
		MAKEWORD(mask_tmp_buf[7], mask_tmp_buf[6] & 0x0F);
	memset(mask_tmp_buf, 0, sizeof(mask_tmp_buf));

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
		ISP_MSK_START1_X, mask_tmp_buf, 8, 0);
	if (ret)
		goto out;
	attr_priv_mask->blocks[1].rect.left =
		MAKEWORD(mask_tmp_buf[1], mask_tmp_buf[0] & 0x0F);
	attr_priv_mask->blocks[1].rect.top =
		MAKEWORD(mask_tmp_buf[3], mask_tmp_buf[2] & 0x0F);
	attr_priv_mask->blocks[1].rect.right =
		MAKEWORD(mask_tmp_buf[5], mask_tmp_buf[4] & 0x0F);
	attr_priv_mask->blocks[1].rect.bottom =
		MAKEWORD(mask_tmp_buf[7], mask_tmp_buf[6] & 0x0F);
	memset(mask_tmp_buf, 0, sizeof(mask_tmp_buf));

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
			ISP_MSK_START2_X, mask_tmp_buf, 8, 0);
	if (ret)
		goto out;
	attr_priv_mask->blocks[2].rect.left =
		MAKEWORD(mask_tmp_buf[1], mask_tmp_buf[0] & 0x0F);
	attr_priv_mask->blocks[2].rect.top =
		MAKEWORD(mask_tmp_buf[3], mask_tmp_buf[2] & 0x0F);
	attr_priv_mask->blocks[2].rect.right =
		MAKEWORD(mask_tmp_buf[5], mask_tmp_buf[4] & 0x0F);
	attr_priv_mask->blocks[2].rect.bottom =
		MAKEWORD(mask_tmp_buf[7], mask_tmp_buf[6] & 0x0F);

	*attr = attr_priv_mask;
out:
	return ret;


}

int rts_cam_set_mask_attr(int fd, struct attr_mask *attr)
{
	int ret = 0;
	uint8_t mask_enabled = 0;
	uint8_t tmp_buf[8] = { 0 };
	uint32_t color = 0;
	uint8_t R = 0;
	uint8_t G = 0;
	uint8_t B = 0;

	assert(attr);
	assert(attr->number == MAX_MASK_MODULE_NUM);
	assert(attr->blocks);
	assert(attr->blocks[0].type == MASK_BLOCK_TYPE_RECT);
	assert(attr->blocks[1].type == MASK_BLOCK_TYPE_RECT);
	assert(attr->blocks[2].type == MASK_BLOCK_TYPE_RECT);

	/* set mask enable option */
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_READ,
			ISP_MSK_CTRL, &mask_enabled, 1, 0);
	if (ret)
		goto out;

	if (attr->blocks[0].enable)
		mask_enabled |= 0x01;
	else
		mask_enabled &= 0xFE;

	if (attr->blocks[1].enable)
		mask_enabled |= 0x02;
	else
		mask_enabled &= 0xFD;

	if (attr->blocks[2].enable)
		mask_enabled |= 0x04;
	else
		mask_enabled &= 0xFB;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_MSK_CTRL, &mask_enabled, 1, 0);
	if (ret)
		goto out;

	/* set mask module rect position */
	/* set rect 1 rect position */
	tmp_buf[0] = (attr->blocks[0].rect.left >> 8) & 0x0F;
	tmp_buf[1] = (attr->blocks[0].rect.left) & 0xFF;
	tmp_buf[2] = (attr->blocks[0].rect.top >> 8) & 0x0F;
	tmp_buf[3] = (attr->blocks[0].rect.top) & 0xFF;
	tmp_buf[4] = (attr->blocks[0].rect.right >> 8) & 0x0F;
	tmp_buf[5] = (attr->blocks[0].rect.right) & 0xFF;
	tmp_buf[6] = (attr->blocks[0].rect.bottom >> 8) & 0x0F;
	tmp_buf[7] = (attr->blocks[0].rect.bottom) & 0xFF;
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_MSK_START0_X, tmp_buf, 8, 0);
	if (ret)
		goto out;

	/* set rect 2 rect position */
	memset(tmp_buf, 0, sizeof(tmp_buf));
	tmp_buf[0] = (attr->blocks[1].rect.left >> 8) & 0x0F;
	tmp_buf[1] = (attr->blocks[1].rect.left) & 0xFF;
	tmp_buf[2] = (attr->blocks[1].rect.top >> 8) & 0x0F;
	tmp_buf[3] = (attr->blocks[1].rect.top) & 0xFF;
	tmp_buf[4] = (attr->blocks[1].rect.right >> 8) & 0x0F;
	tmp_buf[5] = (attr->blocks[1].rect.right) & 0xFF;
	tmp_buf[6] = (attr->blocks[1].rect.bottom >> 8) & 0x0F;
	tmp_buf[7] = (attr->blocks[1].rect.bottom) & 0xFF;
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_MSK_START1_X, tmp_buf, 8, 0);
	if (ret)
		goto out;
	/* set rect 3 rect position */
	memset(tmp_buf, 0, sizeof(tmp_buf));
	tmp_buf[0] = (attr->blocks[2].rect.left >> 8) & 0x0F;
	tmp_buf[1] = (attr->blocks[2].rect.left) & 0xFF;
	tmp_buf[2] = (attr->blocks[2].rect.top >> 8) & 0x0F;
	tmp_buf[3] = (attr->blocks[2].rect.top) & 0xFF;
	tmp_buf[4] = (attr->blocks[2].rect.right >> 8) & 0x0F;
	tmp_buf[5] = (attr->blocks[2].rect.right) & 0xFF;
	tmp_buf[6] = (attr->blocks[2].rect.bottom >> 8) & 0x0F;
	tmp_buf[7] = (attr->blocks[2].rect.bottom) & 0xFF;
	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE,
			ISP_MSK_START2_X, tmp_buf, 8, 0);
	if (ret)
		goto out;

	/* set mask color */
	color = attr->blocks[0].color;
	memset(tmp_buf, 0, sizeof(tmp_buf));
	R = (uint8_t)((color >> 16) & 0xFF);
	G = (uint8_t)((color >> 8) & 0xFF);
	B = (uint8_t)(color & 0xFF);
	tmp_buf[0] = ((66 * R  + 129 * G +  25 * B + 128) >> 8) +  16;
	tmp_buf[1] = ((-38 * R -  74 * G + 112 * B + 128) >> 8) + 128;
	tmp_buf[2] = ((112 * R -  94 * G -  18 * B + 128) >> 8) + 128;

	ret = uvc_exec_vendor_cmd(fd, VC_XMEM_WRITE, ISP_MSK_Y, tmp_buf, 3, 0);
out:
	return ret;


}

int rts_cam_get_osd_attr(int fd, struct attr_osd **attr)
{
	int ret = 0;
	uint8_t osd_enable = 0;
	uint8_t osd_bg_enable = 0;
	uint8_t osd_transparent_enable = 0;
	uint8_t osd_reg_transparent = 0;
	uint8_t osd_transparent = 0;
	uint32_t fg_color = 0;
	uint32_t bg_color = 0;
	uint8_t osd_buf[128] = {0};
	uint8_t osd_string_len = 0;
	uint8_t font_size = 0;
	uint8_t font_width = 0;
	uint8_t font_height = 0;
	struct attr_osd *osd_attr = calloc(1, sizeof(*osd_attr));
	memset(osd_attr, 0 , sizeof(*osd_attr));

	osd_attr->attr_id = ATTR_ID_OSD;
	osd_attr->count_of_supported_font_type = 2;
	osd_attr->time_am_pm_enable = ATTR_NOT_SUPPORTED;
	osd_attr->date_display_enable = ATTR_NOT_SUPPORTED;
	osd_attr->time_display_enable = ATTR_NOT_SUPPORTED;
	osd_attr->number = 1;
	osd_attr->blocks = calloc(1, sizeof(struct osd_block));
	memset(osd_attr->blocks, 0, sizeof(struct osd_block));
	ret = is_osd_enable(fd, &osd_enable);
	if (ret)
		goto out;
	ret = is_osd_bk_enable(fd, &osd_bg_enable);
	if (ret)
		goto out;
	ret = osd_char_transparent_is_enable(fd, &osd_transparent_enable);
	if (ret)
		goto out;
	ret = osd_get_transparency(fd, &osd_reg_transparent);
	if (ret)
		goto out;
	osd_transparent = (uint8_t)((osd_reg_transparent / 15.) * 100 + 0.5);

	ret = osd_get_char_color(fd, &fg_color);
	if (ret)
		goto out;
	ret = osd_get_bk_color(fd, &bg_color);
	if (ret)
		goto out;
	ret = get_osd_len_text(fd, &osd_string_len, osd_buf);
	if (ret)
		goto out;
	ret = get_cur_char_size(fd, &font_width, &font_height, &font_size);
	if (ret)
		goto out;

	osd_attr->blocks[0].enable = osd_enable;
	osd_attr->blocks[0].rect.left = 0;
	osd_attr->blocks[0].rect.right = 0;
	osd_attr->blocks[0].rect.top = 0;
	osd_attr->blocks[0].rect.bottom = 0;
	osd_attr->blocks[0].fg_mode = OSD_SINGLE_CHAR;
	osd_attr->blocks[0].flick_enable = ATTR_NOT_SUPPORTED;
	osd_attr->blocks[0].flick_speed = ATTR_NOT_SUPPORTED;
	osd_attr->blocks[0].fg_transparent_enable = osd_transparent_enable;
	osd_attr->blocks[0].fg_transparency = osd_transparent; /* 0-100*/
	osd_attr->blocks[0].fg_color = fg_color;
	osd_attr->blocks[0].bg_enable = osd_bg_enable;
	osd_attr->blocks[0].bg_color = bg_color;
	osd_attr->blocks[0].each_line_wrap_enable = ATTR_NOT_SUPPORTED;
	osd_attr->blocks[0].each_line_wrap_num = ATTR_NOT_SUPPORTED;
	osd_attr->blocks[0].osd_total_num = osd_string_len;
	osd_attr->blocks[0].osd_buffer = calloc(1,
		osd_string_len * sizeof(uint8_t));
	strncpy(osd_attr->blocks[0].osd_buffer,
		osd_buf, osd_string_len * sizeof(uint8_t));
	osd_attr->blocks[0].osd_font_size = font_size;
	osd_attr->blocks[0].osd_width = font_width;	/*user read only*/
	osd_attr->blocks[0].osd_height = font_height;	/*user read only*/
	osd_attr->blocks[0].osd_rotate_mode = ATTR_NOT_SUPPORTED;
	osd_attr->blocks[0].osd_up_shift = ATTR_NOT_SUPPORTED;
	osd_attr->blocks[0].osd_left_shift = ATTR_NOT_SUPPORTED;

	*attr = osd_attr;
out:
	return ret;
}

int rts_cam_set_osd_attr(int fd, struct attr_osd *attr)
{
	int ret = 0;
	struct attr_osd *osd_attr = (struct attr_osd *)attr;

	assert(osd_attr->number == 1);
	assert(osd_attr->blocks);

	uint8_t osd_transparent =
		osd_attr->blocks[0].fg_transparency; /* 0-100*/
	uint8_t osd_reg_transparent =
		(uint8_t)((osd_transparent / 100.) * 15 + 0.5);

	ret = osd_enable(fd, osd_attr->blocks[0].enable);
	if (ret)
		goto out;
	ret = osd_char_transparent_enable(fd,
		osd_attr->blocks[0].fg_transparent_enable);
	if (ret)
		goto out;
	ret = osd_set_transparency(fd, osd_reg_transparent);
	if (ret)
		goto out;
	ret = osd_set_char_clr(fd, osd_attr->blocks[0].fg_color);
	if (ret)
		goto out;
	ret = osd_bk_enable(fd, osd_attr->blocks[0].bg_enable);
	if (ret)
		goto out;
	ret = osd_set_bk_clr(fd, osd_attr->blocks[0].bg_color);
	if (ret)
		goto out;
	ret = osd_set_text(fd, osd_attr->blocks[0].osd_total_num,
					osd_attr->blocks[0].osd_buffer);
	if (ret)
		goto out;
	ret = osd_set_char_size(fd, osd_attr->blocks[0].osd_font_size);
	if (ret)
		goto out;
out:
	return ret;

}

int rts_cam_get_video_encoder_attr(int fd, struct attr_video_encoder **attr)
{
	int ret = 0;
	uint32_t qp = 0;
	uint32_t gop = 0;
	uint32_t bitrate = 0;
	struct attr_video_encoder *vec_attr = calloc(1, sizeof(*vec_attr));
	memset(vec_attr, 0, sizeof(*vec_attr));
	ret = h264_get_qp(fd, &qp);
	if (ret)
		goto out;
	ret = h264_get_gop(fd, &gop);
	if (ret)
		goto out;
	ret = h264_get_peak_bit_rate(fd, &bitrate);
	if (ret)
		goto out;

	vec_attr->attr_id = ATTR_ID_H264;
	vec_attr->codec_id = ATTR_NOT_SUPPORTED;
	vec_attr->support_bitrate_mode = 1;
	vec_attr->bitrate_mode = BITRATE_MODE_CBR;

	vec_attr->bitrate = bitrate;
	vec_attr->max_bitrate = 0xFFFFFFFF;
	vec_attr->min_bitrate = 0;

	vec_attr->qp = qp;
	vec_attr->max_qp = 51;
	vec_attr->min_qp = 1;

	vec_attr->gop = gop;
	vec_attr->max_gop = 255;
	vec_attr->min_gop = 0; /*0 means inf*/

	*attr = vec_attr;
out:
	return ret;
}

int rts_cam_set_video_encoder_attr(int fd, struct attr_video_encoder *attr)
{
	int ret = 0;
	struct attr_video_encoder *vec_attr = (struct attr_video_encoder *)attr;

	ret = h264_set_qp(fd, vec_attr->qp);
	if (ret)
		goto out;

	ret = h264_set_gop(fd, vec_attr->gop);
	if (ret)
		goto out;

	ret = h264_set_peak_bit_rate(fd, vec_attr->bitrate);
	if (ret)
		goto out;
out:
	return ret;
}

int rts_cam_get_events(int fd, int event_id, uint32_t *property)
{
	int ret = 0;
	switch (event_id) {
	case EVENT_MOTION_DETECT:
		ret = get_motion_detect_result(fd, property);
		break;
	default:
		break;
	}
	return ret;
}

void rts_cam_free_attr(void *attr)
{
	int ret = 0;
	struct attr_stream *attr_stream = (struct attr_stream *)attr;
	switch (attr_stream->attr_id) {
	case ATTR_ID_VIDEO:
		free_video_attr(attr);
		break;
	case ATTR_ID_AUDIO:
		free_audio_attr(attr);
		break;
	case ATTR_ID_STREAM:
		free_stream_attr(attr);
		break;
	case ATTR_ID_MOTION_DETECT:
		free_md_attr(attr);
		break;
	case ATTR_ID_MASK:
		free_mask_attr(attr);
		break;
	case ATTR_ID_H264:
		free_h264_attr(attr);
		break;
	case ATTR_ID_OSD:
		free_osd_attr(attr);
		break;
	default:
		break;
	}
}
