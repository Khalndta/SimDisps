#include "SimDispTypes.h"

#ifndef _SIMDISP_INIT_FINISH

#	ifdef SIMDISP_FOR_DLL_EXPORTS			// 導出 DLL 公開接口申明宏
#		define DISP_DLL_EXPORT extern "C" __declspec(dllexport)
#		define SIMDISP_REG_FUNC(RET, NAME, ...) DISP_DLL_EXPORT RET NAME(__VA_ARGS__)
#		define SIMDISP_PITCH_FUNC(RET, NAME, ...) RET NAME(__VA_ARGS__);
#	elif defined _SIMDISP_LOAD_FUNC		// 導出符號加載語句
#		define _SIMDISP_INIT_FINISH
#		undef  SIMDISP_REG_FUNC
#		undef  SIMDISP_PITCH_FUNC
#		define SIMDISP_REG_FUNC(RET, NAME, ...) \
			NAME = (t ## NAME) \
				GetProcAddress(_SimDisp_hDll, #NAME); \
			if (!NAME) return 1
#		define SIMDISP_PITCH_FUNC(RET, NAME, ...)
#	elif defined SIMDISP_DLL_DEF			// 定義函數簽名類型
#		define _SIMDISP_INIT_FUNC
#		undef SIMDISP_REG_FUNC
#		define SIMDISP_REG_FUNC(RET, NAME, ...) \
			typedef RET(*t ## NAME)(__VA_ARGS__); \
			t##NAME NAME = NULL
#		undef SIMDISP_PITCH_FUNC
#		define SIMDISP_PITCH_FUNC(RET, NAME, ...)
#	else								// 申明函數指針
#		define SIMDISP_REG_FUNC(RET, NAME, ...) \
			extern RET(*NAME)(__VA_ARGS__);
#		define SIMDISP_PITCH_FUNC(RET, NAME, ...) RET NAME(__VA_ARGS__);
#	endif

// SimDisp 窗軆控制函數

SIMDISP_REG_FUNC(uint8_t, SimDisp_Open, void);
SIMDISP_REG_FUNC(void, SimDisp_Close, void);

SIMDISP_REG_FUNC(uint16_t, SimDisp_GetSizeX, void);
SIMDISP_REG_FUNC(uint16_t, SimDisp_GetSizeY, void);
SIMDISP_REG_FUNC(uint8_t, SimDisp_SetSize, uint16_t w, uint16_t h);
SIMDISP_REG_FUNC(uint8_t, SimDisp_SetMaxSize, uint16_t w, uint16_t h);

SIMDISP_REG_FUNC(uint8_t, SimDisp_SetTitle, const char *name);
SIMDISP_REG_FUNC(void, SimDisp_Show, uint8_t s);
SIMDISP_REG_FUNC(void, SimDisp_HideCursor, uint8_t IsHide);
SIMDISP_REG_FUNC(void, SimDisp_Resizeable, uint8_t Resizeable);

SIMDISP_REG_FUNC(void, SimDisp_SetMouseActive, tSimDisp_MouseEventDev);
SIMDISP_REG_FUNC(void, SimDisp_SetTouchActive, tSimDisp_TouchEventDev);
SIMDISP_REG_FUNC(void, SimDisp_SetResizeActive, tSimDisp_SizeEventDev);

SIMDISP_REG_FUNC(uint8_t, SimDisp_AutoFlusher, uint8_t useFlush);
SIMDISP_REG_FUNC(void,  SimDisp_Flush, void);

SIMDISP_REG_FUNC(void*, SimDisp_GetBuffer, void);
SIMDISP_REG_FUNC(void*, SimDisp_SetBuffer, void*);

// SimDisp 繪圖函數

SIMDISP_REG_FUNC(void, SimDisp_SetPixel, int16_t x, int16_t y, uint32_t color);
SIMDISP_REG_FUNC(uint32_t, SimDisp_GetPixel, int16_t x, int16_t y);

SIMDISP_REG_FUNC(void, SimDisp_LineH, int16_t x1, int16_t y1, int16_t x2, uint32_t color);
SIMDISP_REG_FUNC(void, SimDisp_LineV, int16_t x1, int16_t y1, int16_t y2, uint32_t color);

SIMDISP_REG_FUNC(void, SimDisp_FillFull, uint32_t color);
SIMDISP_REG_FUNC(void, SimDisp_FillRect, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color);

SIMDISP_REG_FUNC(void, SimDisp_SetBitmap, int16_t x0, int16_t y0, uint16_t w, uint16_t h, const uint32_t* bmp, uint16_t line);
SIMDISP_REG_FUNC(void, SimDisp_GetBitmap, int16_t x0, int16_t y0, uint16_t w, uint16_t h, uint32_t* bmp, uint16_t line);

#	if defined(_SIMDISP_INIT_FUNC) && !defined(_SIMDISP_INIT_FINISH)
#include <windows.h>
static uint8_t SimDisp_InitDll(const char* pFilename) {
	static HINSTANCE _SimDisp_hDll = NULL;
	if (_SimDisp_hDll)
		return 0;
	_SimDisp_hDll = LoadLibraryA(pFilename);
	if (!_SimDisp_hDll)
		return 1;
#		define _SIMDISP_LOAD_FUNC
#		include "SimDisp.h"
	return 0;
}
#	endif

#endif
