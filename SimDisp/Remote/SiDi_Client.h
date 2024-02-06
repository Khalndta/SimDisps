#include "SiDi_ClientTypes.h"

#ifndef _SIMDISPS_INIT_FINISH

#	ifdef SIMDISPS_FOR_DLL_EXPORTS			// 導出 DLL 公開接口申明宏
#		define DISP_DLL_EXPORT extern "C" __declspec(dllexport)
#		define SIMDISPS_REG_FUNC(RET, NAME, ...) DISP_DLL_EXPORT RET NAME(__VA_ARGS__)
#		define SIMDISPS_PITCH_FUNC(RET, NAME, ...) RET NAME(__VA_ARGS__);
#	elif defined _SIMDISPS_LOAD_FUNC		// 導出符號加載語句
#		define _SIMDISPS_INIT_FINISH
#		undef  SIMDISPS_REG_FUNC
#		undef  SIMDISPS_PITCH_FUNC
#		define SIMDISPS_REG_FUNC(RET, NAME, ...) \
			NAME = (t ## NAME) \
				GetProcAddress(_SimDisps_hDll, #NAME); \
			if (!NAME) return 1
#		define SIMDISPS_PITCH_FUNC(RET, NAME, ...)
#	elif defined SIMDISPS_DLL_DEF			// 定義函數簽名類型
#		define _SIMDISPS_INIT_FUNC
#		undef SIMDISPS_REG_FUNC
#		define SIMDISPS_REG_FUNC(RET, NAME, ...) \
			typedef RET(*t ## NAME)(__VA_ARGS__); \
			t##NAME NAME = NULL
#		undef SIMDISPS_PITCH_FUNC
#		define SIMDISPS_PITCH_FUNC(RET, NAME, ...)
#	else								// 申明函數指針
#		define SIMDISPS_REG_FUNC(RET, NAME, ...) \
			extern RET(*NAME)(__VA_ARGS__);
#		define SIMDISPS_PITCH_FUNC(RET, NAME, ...) RET NAME(__VA_ARGS__);
#	endif

// SimDisps 窗軆控制函數

SIMDISPS_REG_FUNC(uint8_t, SimDisps_Open, void);
SIMDISPS_REG_FUNC(void, SimDisps_Close, void);

SIMDISPS_REG_FUNC(uint16_t, SimDisps_GetSizeX, void);
SIMDISPS_REG_FUNC(uint16_t, SimDisps_GetSizeY, void);
SIMDISPS_REG_FUNC(uint8_t, SimDisps_SetSize, uint16_t w, uint16_t h);
SIMDISPS_REG_FUNC(uint8_t, SimDisps_SetMaxSize, uint16_t w, uint16_t h);

SIMDISPS_REG_FUNC(uint8_t, SimDisps_SetTitle, const char *name);
SIMDISPS_REG_FUNC(void, SimDisps_Show, uint8_t s);
SIMDISPS_REG_FUNC(void, SimDisps_HideCursor, uint8_t IsHide);
SIMDISPS_REG_FUNC(void, SimDisps_Resizeable, uint8_t Resizeable);

SIMDISPS_REG_FUNC(void, SimDisps_SetMouseActive, tSimDisps_MouseEventDev);
SIMDISPS_REG_FUNC(void, SimDisps_SetTouchActive, tSimDisps_TouchEventDev);
SIMDISPS_REG_FUNC(void, SimDisps_SetResizeActive, tSimDisps_SizeEventDev);

SIMDISPS_REG_FUNC(uint8_t, SimDisps_AutoFlusher, uint8_t useFlush);
SIMDISPS_REG_FUNC(void, SimDisps_Flush, void);

SIMDISPS_REG_FUNC(void *, SimDisps_GetBuffer, void);
SIMDISPS_REG_FUNC(void *, SimDisps_SetBuffer, void *);

// SimDisps 繪圖函數

SIMDISPS_REG_FUNC(void, SimDispsSetPixel, int16_t x, int16_t y, uint32_t color);
SIMDISPS_REG_FUNC(uint32_t, SimDisps_GetPixel, int16_t x, int16_t y);

SIMDISPS_REG_FUNC(void, SimDisps_LineH, int16_t x1, int16_t y1, int16_t x2, uint32_t color);
SIMDISPS_REG_FUNC(void, SimDisps_LineV, int16_t x1, int16_t y1, int16_t y2, uint32_t color);

SIMDISPS_REG_FUNC(void, SimDisps_FillFull, uint32_t color);
SIMDISPS_REG_FUNC(void, SimDisps_FillRect, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color);

SIMDISPS_REG_FUNC(void, SimDisps_SetBitmap, int16_t x0, int16_t y0, uint16_t w, uint16_t h, const uint32_t *bmp, uint16_t line);
SIMDISPS_REG_FUNC(void, SimDisps_GetBitmap, int16_t x0, int16_t y0, uint16_t w, uint16_t h, uint32_t *bmp, uint16_t line);

#	if defined(_SIMDISPS_INIT_FUNC) && !defined(_SIMDISPS_INIT_FINISH)
#include <windows.h>
static uint8_t SimDisps_InitDll(const char *pFilename) {
	static HINSTANCE _SimDisps_hDll = NULL;
	if (_SimDisps_hDll)
		return 0;
	_SimDisps_hDll = LoadLibraryA(pFilename);
	if (!_SimDisps_hDll)
		return 1;
#		define _SIMDISPS_LOAD_FUNC
#		include "SimDisps.h"
	return 0;
}
#	endif

#endif
