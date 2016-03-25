#include <rts_cam.h>
#include "rts_uvc.h"
#include <octopus.h>

#define MAX_PIXEL_IN_RECT (8192)

#define safe_free(p) do { \
			if (p) { \
				free(p); \
				p = NULL; \
			} \
		} while (0)

uint32_t yuv2color(uint8_t Y, uint8_t U, uint8_t V, uint32_t *color)
{
	int C = Y - 16;
	int D = U - 128;
	int E = V - 128;
	int R = (298 * C           + 409 * E + 128) >> 8;
	int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
	int B = (298 * C + 516 * D           + 128) >> 8;
	R = R > 255 ? 255 : R;
	R = R < 0 ? 0 : R;
	G = G > 255 ? 255 : G;
	G = G < 0 ? 0 : G;
	B = B > 255 ? 255 : B;
	B = B < 0 ? 0 : B;

	uint32_t clr = (R << 16) + (G << 8) + B;
	if (color)
		*color = clr;
	return clr;
}

void check_sample_mode(uint8_t *u8_data, uint8_t rect_index, int area)
{
	if (0 == rect_index) {
		if (area/4 < MAX_PIXEL_IN_RECT) {
			*u8_data &= 0xFC;
			*u8_data |= 0x00;	/* Mode : 1 */
		} else if (area/4/4 < MAX_PIXEL_IN_RECT) {
			*u8_data &= 0xFC;
			*u8_data |= 0x01;	/* Mode : 1/4 */
		} else if (area/16/4 < MAX_PIXEL_IN_RECT) {
			*u8_data &= 0xFC;
			*u8_data |= 0x02;	/* Mode : 1/16 */
		} else {
			*u8_data &= 0xFC;
			*u8_data |= 0x03;	/* Mode : 1/64 */
		}
	} else if (1 == rect_index) {
		if (area/4 < MAX_PIXEL_IN_RECT) {
			*u8_data &= 0xF3;
			*u8_data |= 0x00;	/* Mode : 1 */
		} else if (area/4/4 < MAX_PIXEL_IN_RECT) {
			*u8_data &= 0xF3;
			*u8_data |= 0x04;	/* Mode : 1/4 */
		} else if (area/16/4 < MAX_PIXEL_IN_RECT) {
			*u8_data &= 0xF3;
			*u8_data |= 0x08;	/* Mode : 1/16 */
		} else {
			*u8_data &= 0xF3;
			*u8_data |= 0x0C;	/* Mode : 1/64 */
		}
	} else if (2 == rect_index) {
		if (area/4 < MAX_PIXEL_IN_RECT) {
			*u8_data &= 0xCF;
			*u8_data |= 0x00;	/* Mode : 1 */
		} else if (area/4/4 < MAX_PIXEL_IN_RECT) {
			*u8_data &= 0xCF;
			*u8_data |= 0x10;	/* Mode : 1/4 */
		} else if (area/16/4 < MAX_PIXEL_IN_RECT) {
			*u8_data &= 0xCF;
			*u8_data |= 0x20;	/* Mode : 1/16 */
		} else {
			*u8_data &= 0xCF;
			*u8_data |= 0x30;	/* Mode : 1/64 */
		}
	}
}

int get_motion_detect_result(int fd, uint32_t *property)
{
	int ret = 0;
	uint8_t buf[4] = {0};
	ret = uvc_exec_vendor_cmd(fd, VC_MTD_RESULT, 0, buf, 1, 0);
	if (property)
		*property = buf[0];
	return ret;
}

void free_video_attr(void *attr)
{
	struct attr_video *video = (struct attr_video *)attr;
	safe_free(video);
}

void free_audio_attr(void *attr)
{

}

void free_stream_attr(void *attr)
{
	int i = 0, j = 0;
	struct attr_stream *stream_attr = (struct attr_stream *)attr;
	for (i = 0; i < stream_attr->number; i++) {
		for (j = 0; j < stream_attr->formats[i].number; j++) {
			safe_free(stream_attr->formats[i].resolutions[j].frmivals);
		}
		safe_free(stream_attr->formats[i].resolutions);
	}
	safe_free(stream_attr->formats);
	safe_free(stream_attr);
}

void free_md_attr(void *attr)
{
	struct attr_md *md = (struct attr_md *)attr;
	safe_free(md->blocks);
	safe_free(md);
}

void free_mask_attr(void *attr)
{
	struct attr_mask *mask = (struct attr_mask *)attr;
	safe_free(mask->blocks);
	safe_free(mask);
}

void free_h264_attr(void *attr)
{
	struct attr_video_encoder *vec_attr = (struct attr_video_encoder *)attr;
	safe_free(vec_attr);
}

void free_osd_attr(void *attr)
{
	int i = 0;
	struct attr_osd *osd = (struct attr_osd *)attr;
	for (i = 0; i < osd->number; i++)
		safe_free(osd->blocks[i].osd_buffer);
	safe_free(osd);
}
