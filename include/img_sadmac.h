#include <stdint.h>

#define SADMAC_FRAME_WIDTH 15
#define SADMAC_FRAME_HEIGHT 15

static const uint8_t sadmac[30] PROGMEM = {
	0x00,0x00,0x50,0x14,0x20,0x08,0x50,0x14,
	0x00,0x00,0x04,0x40,0x03,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x07,0x40,0x18,0x30,
	0x60,0x0c,0x00,0x02,0x00,0x00
};
