#ifndef __UVC_INTERNAL_H__
#define __UVC_INTERNAL_H__
#include <stdint.h>

struct v4l2_msg_p {
	uint32_t request;
	uint8_t args[256];
};

#endif
