#ifndef __RTS_UVC_H__
#define __RTS_UVC_H__
#include <stdint.h>
#include "uvcvideo.h"
/*just for compile, need change this later*/
#define v4l2_ioctl uvc_ioctl
/* UVC Domain */
int uvc_ioctl(int msg, uint32_t request, void *arg);
int uvc_add_xu_ctrls(int fd);
int uvc_query_xu_ctrl(int fd, struct uvc_xu_control_query *xctrl);
int uvc_get_cmd_status(int fd, uint16_t *status);
int uvc_exec_vendor_cmd(int fd, uint16_t cmd_code,
		uint16_t addr, uint8_t  *buf, uint16_t len, uint16_t resv);

#define MAKE_CMD_CODE(domain, data, dir, subcmd) \
	(uint16_t) (0 \
		| ((domain & 0xFF) << 8) \
		| ((data & 0x01) << 7) \
		| ((dir & 0x01) << 6) \
		| (subcmd & 0x3F) \
		)

enum {
	VC_XMEM_READ		= MAKE_CMD_CODE(0, 1, 1, 0x02),
	VC_XMEM_WRITE		= MAKE_CMD_CODE(0, 1, 0, 0x02),

	VC_I2C_READ		= MAKE_CMD_CODE(0, 1, 1, 0x0A),
	VC_I2C_WRITE		= MAKE_CMD_CODE(0, 1, 0, 0x0A),
	VC_I2C_NORMAL_READ	= MAKE_CMD_CODE(0, 1, 1, 0x2A),
	VC_I2C_NORMAL_WRITE	= MAKE_CMD_CODE(0, 1, 0, 0x2A),
	VC_I2C_GEN_READ	        = MAKE_CMD_CODE(0, 1, 1, 0x32),
	VC_I2C_GEN_WRITE        = MAKE_CMD_CODE(0, 1, 0, 0x32),

	VC_DEV_STATUS		= MAKE_CMD_CODE(0, 1, 1, 0x0D),

	VC_OSD_SRAM_SET		= MAKE_CMD_CODE(3, 1, 0, 0x03),
	VC_OSD_TIME_SET         = MAKE_CMD_CODE(3, 1, 0, 0x04),
	VC_OSD_CFG_STRING_READ	= MAKE_CMD_CODE(3, 1, 1, 0x07),
	VC_OSD_CFG_STRING_WRITE	= MAKE_CMD_CODE(3, 1, 0, 0x08),
	VC_OSD_LEN_GET		= MAKE_CMD_CODE(3, 1, 1, 0x22),
	VC_OSD_LEN_SET		= MAKE_CMD_CODE(3, 1, 0, 0x22),

	VC_H264_GOP_SET		= MAKE_CMD_CODE(3, 1, 0, 0x1b),
	VC_H264_GOP_GET		= MAKE_CMD_CODE(3, 1, 1, 0x1b),
	VC_H264_MAX_QP_SET	= MAKE_CMD_CODE(3, 1, 0, 0x21),
	VC_H264_MAX_QP_GET	= MAKE_CMD_CODE(3, 1, 1, 0x21),
	VC_H264_BITRATE_SET	= MAKE_CMD_CODE(3, 1, 0, 0x24),
	VC_H264_BITRATE_GET	= MAKE_CMD_CODE(3, 1, 1, 0x24),

	VC_MTD_RESULT		= MAKE_CMD_CODE(3, 1, 1, 0x02),

	VC_RESET		= MAKE_CMD_CODE(0xFF, 0, 0, 0),
};

#endif
