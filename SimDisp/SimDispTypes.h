#pragma once

#include <stdint.h>

#define SIMDISP_CLK_HOLD 0
#define SIMDISP_CLK_UP   1
#define SIMDISP_CLK_DOWN 2
#define SIMDISP_CLK_DBL  3

#define SIMDISP_KEY_ALT   0x1
#define SIMDISP_KEY_CTRL  0x2
#define SIMDISP_KEY_SHIFT 0x4

typedef struct {
	uint16_t left   : 2;
	uint16_t middle : 2;
	uint16_t right  : 2;
	uint16_t mk     : 3;
	uint16_t leave  : 1;
} tSimDisp_MState;

typedef struct {
	int16_t x, y;
} tSimDisp_Point;

typedef void(*tSimDisp_MouseEventDev)(tSimDisp_MState state, int32_t xPos, int32_t yPos, int32_t wheel_delta);
typedef void(*tSimDisp_TouchEventDev)(tSimDisp_Point *points, uint8_t point_count);
typedef uint8_t(*tSimDisp_SizeEventDev)(uint16_t nWidth, uint16_t nHeight);

#define SimDisp_CalcBufferSize(SizeX, SizeY) \
	(((SizeX * /*nPlanes*/1 * /*nBitCount*/32 + 15) >> 4) << 1) * SizeY
