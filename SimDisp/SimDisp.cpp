#include <windows.h>
#include <stdio.h>

#define SIMDISP_FOR_DLL_EXPORTS
#include "SimDisp.h"

#pragma region Variables

constexpr auto
WCN_Box = "SIMDISP_BOX",
WCN_Scr = "SIMDISP_SCR";

static char aDispName[255] = "SimDisp";
static uint8_t IsDispShow = true;

static HWND
hwndBox = nullptr,
hwndScr = nullptr;
static RECT BoxBorder = { 10, 10, 10, 10 };

static HDC dcScreen = nullptr;
static HDC hdcBitmap = nullptr;
static HBITMAP hBmpScreen = nullptr;

static size_t bmpSize = 0;
static uint32_t *pBmpBuffer = nullptr;
static uint32_t *pBmpBufMod = nullptr;
static uint16_t
ScreenSizeX = 100,
ScreenSizeY = 100;

static uint16_t
ScreenMaxSizeX = GetSystemMetrics(SM_CXSCREEN),
ScreenMaxSizeY = GetSystemMetrics(SM_CYSCREEN);

static tSimDisp_MouseEventDev OnMouseActive = nullptr;
static tSimDisp_TouchEventDev OnTouchActive = nullptr;
static tSimDisp_SizeEventDev OnSizeActive = nullptr;

static HANDLE
hWndProc = nullptr,
hFlusher = nullptr;

static bool
HasRun = false,
IsAutoFlush = true,
IsResizeable = false,
IsCursorHide = false;

static volatile bool BufLockOnClip = false;
static volatile bool BufLockOnGraph = false;

#pragma endregion

#pragma region Debug outputs

#include "output.inl"
static inline void _ConInit() {
	hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
	sprintf_s(aNoticeId, "SimDisp [PID:%X]", GetCurrentProcessId());
}

inline bool _POS_ASSERT_X(int16_t x, const char *Function) {
	if (0 <= x && x < ScreenSizeX)
		return false;
	_WarnOut("%s x = %d range out", Function, x);
	return true;
}
inline bool _POS_ASSERT_Y(int16_t y, const char *Function) {
	if (0 <= y && y < ScreenSizeY)
		return false;
	_WarnOut("%s y = %d range out", Function, y);
	return true;
}

#if _DEBUG
#define POS_ASSERT_X(x, ...) if (_POS_ASSERT_X(x, __FUNCTION__)) return __VA_ARGS__;
#define POS_ASSERT_Y(y, ...) if (_POS_ASSERT_Y(y, __FUNCTION__)) return __VA_ARGS__;
#else
#define POS_ASSERT_X(...)
#define POS_ASSERT_Y(...)
#endif

#pragma endregion

static inline void _ClipBitmap(
	uint16_t nSizeX, uint16_t nSizeY, uint32_t *nBuffer,
	uint16_t SizeX, uint16_t SizeY, const uint32_t *oBuffer) {
	auto XSize = SizeX < nSizeX ? SizeX : nSizeX;
	auto YSize = SizeY < nSizeY ? SizeY : nSizeY;
	XSize <<= 2;
	while (YSize--) {
		memcpy_s(
			nBuffer, XSize,
			oBuffer, XSize);
		nBuffer += nSizeX;
		oBuffer += SizeX;
	}
}

// Buffer
static inline uint32_t _AllocBuffer(uint16_t SizeX, uint16_t SizeY, uint32_t *&pBuffer) {
	uint32_t bmpSize = SimDisp_CalcBufferSize(SizeX, SizeY);
	pBuffer = (uint32_t *)malloc(bmpSize);
	if (!pBuffer) {
		MessageBoxA(hwndBox, 
					"Buffer create failed", 
					"Failed", 
					MB_OK | MB_ICONERROR);
		exit(0);
	}
	memset(pBuffer, ~0, bmpSize);
	SIMDISP_LOG("screen buffer created %dx%dpx ~ %dBytes", ScreenSizeX, ScreenSizeY, bmpSize);
	return bmpSize;
}
static inline void _ClipBuffer(uint16_t nSizeX, uint16_t nSizeY) {
	bmpSize = SimDisp_CalcBufferSize(nSizeX, nSizeY);
	auto nBuffer = (uint32_t *)malloc(bmpSize);
	memset(nBuffer, ~0, bmpSize);
	_ClipBitmap(
		nSizeX, nSizeY, nBuffer,
		ScreenSizeX, ScreenSizeY, pBmpBuffer);
	free(pBmpBuffer);
	pBmpBuffer = nBuffer;
	ScreenSizeX = nSizeX;
	ScreenSizeY = nSizeY;
}
static inline void _CloseBuffer() {
	if (pBmpBuffer)
		free(pBmpBuffer),
		pBmpBuffer = nullptr;
	if (hBmpScreen)
		DeleteObject(hBmpScreen),
		hBmpScreen = nullptr;
	if (hdcBitmap)
		DeleteDC(hdcBitmap),
		hdcBitmap = nullptr;
}
static inline void _CreateHandles(void *pBuffer) {
	hBmpScreen = CreateBitmap(ScreenSizeX, ScreenSizeY, 1, 32, pBuffer);
	if (hBmpScreen == nullptr || hBmpScreen == INVALID_HANDLE_VALUE) {
		SIMDISP_ERR("CreateBitmap");
		exit(0);
	}
	hdcBitmap = CreateCompatibleDC(nullptr);
	if (hdcBitmap == nullptr || hdcBitmap == INVALID_HANDLE_VALUE) {
		SIMDISP_ERR("CreateCompatibleDC");
		exit(0);
	}
	auto hGdiBitmap = SelectObject(hdcBitmap, hBmpScreen);
	if (hGdiBitmap == nullptr || hGdiBitmap == INVALID_HANDLE_VALUE) {
		SIMDISP_ERR("SelectObject");
		exit(0);
	}
}
static inline void _CreateBuffer(uint32_t *pBuff = nullptr) {
	uint32_t *pBuffer = pBuff;
	if (!pBuffer)
		bmpSize = _AllocBuffer(ScreenSizeX, ScreenSizeY, pBuffer);
	_CreateHandles(pBuffer);
	pBmpBuffer = pBuffer;
}
static inline void _UpdateBuffer() {
	SetBitmapBits(hBmpScreen, (DWORD)bmpSize, pBmpBufMod ? pBmpBufMod : pBmpBuffer);
	BitBlt(dcScreen, 0, 0, ScreenSizeX, ScreenSizeY, hdcBitmap, 0, 0, SRCCOPY);
}
static inline void _RedrawWnd() {
	BitBlt(dcScreen, 0, 0, ScreenSizeX, ScreenSizeY, hdcBitmap, 0, 0, SRCCOPY);
}
// Flusher
volatile bool activeFlush = true;
volatile bool flushActive = false;
DWORD WINAPI pFlusher(LPVOID) {
	for (;;) {
		while (activeFlush) {
			flushActive = true;
			_UpdateBuffer();
		}
		flushActive = false;
	}
	return 0;
}
static inline void _CloseFlusher() {
	if (!hFlusher)
		return;
	DWORD code = 0;
	if (!GetExitCodeThread(hFlusher, &code)) {
		hFlusher = nullptr;
		return;
	}
	if (code != STILL_ACTIVE) {
		hFlusher = nullptr;
		return;
	}
	activeFlush = false;
	WaitForSingleObject(hFlusher, INFINITE);
	activeFlush = true;
	hFlusher = nullptr;
	SIMDISP_LOG("flusher closed");
}
static inline bool _CreateFlusher() {
	if (!IsAutoFlush)
		return false;
	if (hFlusher != nullptr && hFlusher != INVALID_HANDLE_VALUE) {
		DWORD ExitCode = 0;
		if (GetExitCodeThread(hFlusher, &ExitCode))
			if (ExitCode == STILL_ACTIVE)
				return false;
	}
	hFlusher = CreateThread(
		nullptr, 0, 
		pFlusher, nullptr,
		0, nullptr);
	if (hFlusher == nullptr || hFlusher == INVALID_HANDLE_VALUE) {
		SIMDISP_ERR("CreateThread pFlusher GetLastError %d", GetLastError());
		return true;
	}
	SIMDISP_LOG("CreateThread pFlusher");
	return false;
}
//
static inline void _ClipScreen(uint16_t nSizeX, uint16_t nSizeY) {
	// Lock
	BufLockOnClip = true;
	while (BufLockOnGraph); // wait graphing operations finished
	auto flushState = activeFlush;
	activeFlush = false; // make sure flusher not work
	if (activeFlush)
		while (flushActive); // wait flushing finished
	// Clip
	_ClipBuffer(nSizeX, nSizeY);
	DeleteObject(hBmpScreen);
	DeleteDC(hdcBitmap);
	_CreateHandles(pBmpBuffer);
	// Unlock
	activeFlush = flushState;
	if (activeFlush)
		while (!flushActive); // wait flushing started
	BufLockOnClip = false;
}
// Windows
static inline void _ResizeDispWnd(int SizeX, int SizeY) {
	RECT rClient, rWin;
	GetWindowRect(hwndBox, &rWin);
	GetClientRect(hwndBox, &rClient);
	int xBorder = (rWin.right - rWin.left) - (rClient.right - rClient.left) + BoxBorder.left + BoxBorder.right,
		yBorder = (rWin.bottom - rWin.top) - (rClient.bottom - rClient.top) + BoxBorder.top + BoxBorder.bottom;
	SetWindowPos(hwndBox, nullptr, 0, 0,
				 xBorder + SizeX - 1, yBorder + SizeY - 1,
				 SWP_NOMOVE);
	MoveWindow(hwndScr, BoxBorder.left, BoxBorder.right,
			   SizeX, SizeY,
			   false);
}
static void _CloseDispWnd() {
	if (hwndScr ? IsWindow(hwndScr) : false)
		DestroyWindow(hwndScr), hwndScr = nullptr;
	if (hwndBox ? IsWindow(hwndBox) : false)
		DestroyWindow(hwndBox), hwndBox = nullptr;
	if (hWndProc == nullptr || hWndProc == INVALID_HANDLE_VALUE)
		return;
	WaitForSingleObject(hWndProc, 100);
	TerminateThread(hWndProc, -1);
	hWndProc = nullptr;
	HasRun = false;
}
static bool _CreateDispWnd() {
	static auto BkBrush = CreateSolidBrush(RGB(0xff, 0xff, 0xff));
	static auto _MouseEvtCheck = [](UINT msg, WPARAM wParam, LPARAM lParam) {
		tSimDisp_MState state = { 0 };
		int zDelta = 0;
		static bool mbTracking = false;
		switch (msg) {
		// Mouse Moveout
		case WM_MOUSELEAVE:
			mbTracking = FALSE;
			state.left = state.middle = state.right = SIMDISP_CLK_UP;
			state.leave = 1;
//			_log("mouse move out");
			break;
		case WM_MOUSEMOVE: {
			if (mbTracking)
				break;
			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof(tme);
			tme.hwndTrack = hwndScr;
			tme.dwFlags = TME_LEAVE | TME_HOVER;
			tme.dwHoverTime = 1;
			mbTracking = TrackMouseEvent(&tme);
			break;
		}
		// Left Button
		case WM_LBUTTONDOWN:   state.left = SIMDISP_CLK_DOWN; break;
		case WM_LBUTTONUP:     state.left = SIMDISP_CLK_UP; break;
		case WM_LBUTTONDBLCLK: state.left = SIMDISP_CLK_DBL; break;
		// Mid Button
		case WM_MBUTTONDOWN:   state.middle = SIMDISP_CLK_DOWN; break;
		case WM_MBUTTONUP:     state.middle = SIMDISP_CLK_UP; break;
		case WM_MBUTTONDBLCLK: state.middle = SIMDISP_CLK_DBL; break;
		// Right Button
		case WM_RBUTTONDOWN:   state.right = SIMDISP_CLK_DOWN; break;
		case WM_RBUTTONUP:     state.right = SIMDISP_CLK_UP; break;
		case WM_RBUTTONDBLCLK: state.right = SIMDISP_CLK_DBL; break;
		case WM_MOUSEWHEEL:
			wParam = GET_KEYSTATE_WPARAM(wParam);
			zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			break;
		case WM_MOUSELAST:
			break;
		default:
			return;
		}
		int xPos = LOWORD(lParam),
			yPos = HIWORD(lParam);
		if (0 >= xPos || xPos >= (int)ScreenSizeX) {
			state.left = state.middle = state.right = SIMDISP_CLK_UP;
			state.leave = 1;
//			SIMDISP_LOG("mouse range out x");
			return;
		}
		if (0 >= yPos || yPos >= (int)ScreenSizeY) {
			state.left = state.middle = state.right = SIMDISP_CLK_UP;
			state.leave = 1;
//			SIMDISP_LOG("mouse range out y");
			return;
		}
		if (wParam & MK_ALT)     state.mk |= SIMDISP_KEY_ALT;
		if (wParam & MK_CONTROL) state.mk |= SIMDISP_KEY_CTRL;
		if (wParam & MK_SHIFT)   state.mk |= SIMDISP_KEY_SHIFT;
		OnMouseActive(state, xPos - 1, yPos - 1, zDelta);
	};
	static auto _TouchEvtCheck = [](HWND hWnd, HTOUCHINPUT tinp, UINT pcnt) {
		static TOUCHINPUT last_point = { 0 };
		TOUCHINPUT inp[MAX_TOUCH_COUNT];
		if (!GetTouchInputInfo(tinp, pcnt, inp, sizeof(TOUCHINPUT))) {
			SIMDISP_ERR("GetTouchInputInfo");
			return;
		}
		auto points = new tSimDisp_Point[pcnt];
		for (UINT i = 0; i < pcnt; ++i) {
			POINT p;
			p.x = TOUCH_COORD_TO_PIXEL(inp[i].x);
			p.y = TOUCH_COORD_TO_PIXEL(inp[i].y);
			ClientToScreen(hWnd, &p);
			points[i].x = (int16_t)p.x;
			points[i].y = (int16_t)p.y;
		}
		OnTouchActive(points, pcnt);
		delete[] points;
		CloseTouchInputHandle(tinp);
	};
	static auto pwpScr = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT {
		switch (message) {
		case WM_POINTERLEAVE:
			break;
		case WM_POINTERUP:
			break;
		case WM_TOUCH:
			if (auto tinp = (HTOUCHINPUT)lParam)
				if (OnTouchActive)
					_TouchEvtCheck(hWnd, tinp, (UINT)wParam);
			break;
		case WM_COMMAND:
			break;
		case WM_PAINT:
			_RedrawWnd();
			break;
		case WM_DESTROY:
			UnregisterTouchWindow(hWnd);
			PostQuitMessage(0);
			break;
		case WM_SETCURSOR:
			if (IsCursorHide) {
				SetCursor(nullptr);
				return true;
			}
			return DefWindowProcA(hWnd, message, wParam, lParam);
		default:
			return DefWindowProcA(hWnd, message, wParam, lParam);
		}
		return 0;
	};
	static auto pwpBox = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT {
		switch (message) {
		case WM_WINDOWPOSCHANGING: {
			auto wp = (LPWINDOWPOS)lParam;
			if (wp->flags & SWP_NOSIZE)
				break;
			RECT rClient, rWin;
			GetWindowRect(hWnd, &rWin);
			GetClientRect(hWnd, &rClient);
			int xBorder = (rWin.right - rWin.left) - (rClient.right - rClient.left) + BoxBorder.left + BoxBorder.right,
				yBorder = (rWin.bottom - rWin.top) - (rClient.bottom - rClient.top) + BoxBorder.top + BoxBorder.bottom;
			int nSizeX = wp->cx - xBorder + 1,
				nSizeY = wp->cy - yBorder + 1;
			if (nSizeX <= 0 || nSizeY <= 0)
				break;
			if (nSizeX == ScreenSizeX && nSizeY == ScreenSizeY)
				break;
			if (OnSizeActive)
				if (!OnSizeActive(nSizeX, nSizeY)) {
					wp->flags |= SWP_NOSIZE;
					break;
				}
			_ClipScreen(nSizeX, nSizeY);
			_ResizeDispWnd(nSizeX, nSizeY);
			break;
		}
		}
		return DefWindowProcA(hWnd, message, wParam, lParam);
	};
	static auto _RegClasses = [] {
		WNDCLASSEXA cls = { 0 };
		cls.cbSize = sizeof(WNDCLASSEXA);
		cls.style = CS_HREDRAW | CS_VREDRAW;
		cls.hbrBackground = BkBrush;
		cls.hCursor = LoadCursorA(nullptr, (LPCSTR)IDC_ARROW);
		// SimDisp Box
		cls.lpszClassName = WCN_Box;
		cls.lpfnWndProc = pwpBox;
		if (!RegisterClassExA(&cls)) {
			SIMDISP_ERR("RegisterClassExA ClassName:'%s' GetLastError:%dd", WCN_Box, GetLastError());
			return true;
		}
		// SimDisp Graph
		cls.lpszClassName = WCN_Scr;
		cls.lpfnWndProc = pwpScr;
		if (!RegisterClassExA(&cls)) {
			SIMDISP_ERR("RegisterClassExA '%s' GetLastError %d", WCN_Scr, GetLastError());
			return true;
		}
		return false;
	};
	static auto _CrtWindows = [] {
		hwndBox = CreateWindowExA(
			0, WCN_Box, aDispName,
			WS_CLIPCHILDREN | WS_CAPTION | WS_BORDER |
					(IsResizeable ? WS_SIZEBOX : 0),
			CW_USEDEFAULT, 0,
			ScreenSizeX + BoxBorder.left + BoxBorder.right,
			ScreenSizeY + BoxBorder.top + BoxBorder.bottom,
			nullptr, nullptr, nullptr, nullptr);
		if (hwndBox == nullptr) {
			SIMDISP_ERR("CreateWindowExA ClassName:'%s' WindowName:'%s' GetLastError:%dd", WCN_Box, aDispName, GetLastError());
			return true;
		}
		hwndScr = CreateWindowExA(
			0, WCN_Scr, nullptr,
			WS_CHILD | WS_VISIBLE,
			BoxBorder.left, BoxBorder.top,
			ScreenSizeX, ScreenSizeY,
			hwndBox, nullptr, nullptr, nullptr);
		if (hwndScr == nullptr) {
			SIMDISP_ERR("CreateWindowExA ClassName:'%s' WindowName:'' GetLastError:%dd", WCN_Scr, GetLastError());
			return true;
		}
		dcScreen = GetDC(hwndScr);
		RegisterTouchWindow(hwndScr, TWF_FINETOUCH);
		int SizeX = GetSystemMetrics(SM_CXFULLSCREEN),
			SizeY = GetSystemMetrics(SM_CYFULLSCREEN);
		_ResizeDispWnd(ScreenSizeX, ScreenSizeY);
		RECT r;
		GetWindowRect(hwndBox, &r);
		SetWindowPos(hwndBox, nullptr,
					 (SizeX - (r.right - r.left + 1)) / 2,
					 (SizeY - (r.bottom - r.top + 1)) / 2,
					 0, 0,
					 SWP_NOSIZE);
		ShowWindow(hwndBox, IsDispShow ? SW_SHOW : SW_HIDE);
		UpdateWindow(hwndBox);
		return false;
	};
	static auto pMsgProc = [](LPVOID) -> DWORD {
		if (_CrtWindows())
			return -1;
		HasRun = true;
		// Message Proccessor
		MSG msg;
		while (GetMessageA(&msg, nullptr, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
			if (OnMouseActive && msg.hwnd == hwndScr)
				_MouseEvtCheck(msg.message, msg.wParam, msg.lParam);
		}
		return 0;
	};
	if (_RegClasses())
		return false;
	HasRun = false;
	hWndProc = CreateThread(
		nullptr, 0,
		pMsgProc, nullptr,
		0, nullptr);
	if (!hWndProc) {
		SIMDISP_ERR("CreateThread pMsgProc");
		return true;
	}
	int t = 0;
	while (!HasRun) {
		Sleep(1);
		if (t > 100) {
			SIMDISP_ERR("creating timeout");
			return true;
		}
		++t;
	}
	return false;
}

#pragma region Window
DISP_DLL_EXPORT uint8_t SimDisp_Open() {
	if (hWndProc)
		return 0;
	_CreateBuffer();
	if (_CreateDispWnd())
		goto OnError;
	if (_CreateFlusher())
		goto OnError;
	SIMDISP_LOG("opened");
	return false;
OnError:
	SimDisp_Close();
	return true;
}
DISP_DLL_EXPORT void SimDisp_Close() {
	HasRun = IsAutoFlush = false;
	_CloseDispWnd();
	_CloseFlusher();
	SIMDISP_LOG("closed");
	HasRun = false;
}
// Screen & Box Size
DISP_DLL_EXPORT uint16_t SimDisp_GetSizeX() { return ScreenSizeX + 1; }
DISP_DLL_EXPORT uint16_t SimDisp_GetSizeY() { return ScreenSizeY + 1; }
DISP_DLL_EXPORT uint8_t SimDisp_SetSize(uint16_t w, uint16_t h) {
	if (w == 0 || h == 0) {
		SIMDISP_WARN("argument invalid");
		return true;
	}
	if (w > ScreenMaxSizeX)
		w = ScreenMaxSizeX;
	if (h > ScreenMaxSizeY)
		h = ScreenMaxSizeY;
	if (w == ScreenSizeX && h == ScreenSizeY) {
		SIMDISP_LOG("%dx%dpx target size same as current", w, h);
		return true;
	}
	if (!HasRun) {
		ScreenSizeX = w;
		ScreenSizeY = h;
		SIMDISP_LOG(" %dx%dpx preset", w, h);
		return false;
	}
	_ResizeDispWnd(w, h);
	SIMDISP_LOG("%dx%dpx resized", w, h);
	return false;
}
DISP_DLL_EXPORT uint8_t SimDisp_SetMaxSize(uint16_t w, uint16_t h) {
	if (w == 0 || h == 0) {
		SIMDISP_WARN("argument invalid");
		return true;
	}
	if (w == ScreenMaxSizeX && h == ScreenMaxSizeY) {
		SIMDISP_LOG("%dx%dpx target size same as current", w, h);
		return true;
	}
	ScreenMaxSizeX = w;
	ScreenMaxSizeY = h;
	if (ScreenSizeX < ScreenMaxSizeX && ScreenMaxSizeY < ScreenMaxSizeY)
		return false;
	w = ScreenSizeX;
	h = ScreenSizeX;
	if (w > ScreenMaxSizeX)
		w = ScreenMaxSizeX;
	if (h > ScreenMaxSizeY)
		h = ScreenMaxSizeY;
	if (!HasRun) {
		ScreenSizeX = w;
		ScreenSizeY = h;
		SIMDISP_LOG("%dx%dpx", ScreenMaxSizeX, ScreenMaxSizeY);
		return false;
	}
	_ResizeDispWnd(w, h);
	SIMDISP_LOG("%dx%dpx clipped", w, h);
	return false;
}
//
DISP_DLL_EXPORT void SimDisp_SetMouseActive(tSimDisp_MouseEventDev mEvent) {
	if (OnMouseActive == mEvent)
		return;
	OnMouseActive = mEvent;
	SIMDISP_LOG("%s", mEvent ? "attached" : "detached");
}
DISP_DLL_EXPORT void SimDisp_SetTouchActive(tSimDisp_TouchEventDev mEvent) {
	if (OnTouchActive == mEvent)
		return;
	OnTouchActive = mEvent;
	SIMDISP_LOG("%s", mEvent ? "attached" : "detached");
}
DISP_DLL_EXPORT void SimDisp_SetResizeActive(tSimDisp_SizeEventDev mEvent) {
	if (OnSizeActive == mEvent)
		return;
	OnSizeActive = mEvent;
	SIMDISP_LOG("%s", mEvent ? "attached" : "detached");
}
//
DISP_DLL_EXPORT uint8_t SimDisp_SetTitle(const char *name) {
	if (!name)
		return true;
	strcpy_s(aDispName, name);
	if (!hwndScr) {
		SIMDISP_LOG("'%s' preset", aDispName);
		return false;
	}
	if (SetWindowTextA(hwndScr, aDispName)) {
		SIMDISP_LOG("'%s' titled", aDispName);
		return false;
	}
	SIMDISP_ERR("'%s' error", name);
	return true;
}
DISP_DLL_EXPORT void SimDisp_Show(uint8_t s) {
	if (IsDispShow == s)
		return;
	IsDispShow = s;
	if (HasRun)
		ShowWindow(hwndScr, s ? SW_SHOW : SW_HIDE);
	SIMDISP_LOG("%s", s ? "show" : "hide");
}
DISP_DLL_EXPORT void SimDisp_HideCursor(uint8_t IsHide) {
	if (IsCursorHide == (bool)IsHide)
		return;
	IsCursorHide = (bool)IsHide;
	SIMDISP_LOG("%s", IsHide ? "show" : "hide");
}
DISP_DLL_EXPORT void SimDisp_Resizeable(uint8_t Resizeable) {
	if (IsResizeable == (bool)Resizeable)
		return;
	IsResizeable = (bool)Resizeable;
	SIMDISP_LOG("%s", Resizeable ? "resizeable" : "resizeless");
	if (!hwndBox)
		return;
	if (!IsWindow(hwndBox))
		return;
	SetWindowLongA(hwndBox, GWL_STYLE,
				   Resizeable ?
				   GetWindowLongA(hwndBox, GWL_STYLE) | WS_SIZEBOX :
				   GetWindowLongA(hwndBox, GWL_STYLE) & ~WS_SIZEBOX);
}
// Flusher
DISP_DLL_EXPORT uint8_t SimDisp_AutoFlusher(uint8_t use) {
	if (IsAutoFlush == (bool)use)
		return false;
	IsAutoFlush = use;
	if (!HasRun)
		return false;
	if (!use) {
		_CloseFlusher();
		return false;
	}
	if (_CreateFlusher())
		return true;
	return false;
}
DISP_DLL_EXPORT void SimDisp_Flush() { _UpdateBuffer(); }
DISP_DLL_EXPORT void SimDisp_FlushNow(void *pBuffer) {
	if (!HasRun)
		return;
	SetBitmapBits(hBmpScreen, (DWORD)bmpSize, pBuffer);
	BitBlt(dcScreen, 0, 0, ScreenSizeX, ScreenSizeY, hdcBitmap, 0, 0, SRCCOPY);
}
// Buffer
DISP_DLL_EXPORT void *SimDisp_GetBuffer() {
	return pBmpBuffer;
}
DISP_DLL_EXPORT void *SimDisp_SetBuffer(void *pBuffer) {
	pBmpBufMod = (uint32_t *)pBuffer;
	if (pBuffer)
		SIMDISP_LOG("attached");
	else
		SIMDISP_LOG("detached");
	return pBmpBufMod;
}
#pragma endregion

#pragma region Graphics

// Dot
DISP_DLL_EXPORT void SimDisp_SetPixel(int16_t x, int16_t y, uint32_t rgb) {
	POS_ASSERT_X(x);
	POS_ASSERT_Y(y);
	pBmpBuffer[x + y * ScreenSizeX] = rgb;
}
DISP_DLL_EXPORT uint32_t SimDisp_GetPixel(int16_t x, int16_t y) {
	POS_ASSERT_X(x, 0);
	POS_ASSERT_Y(y, 0);
	return pBmpBuffer[x + y * ScreenSizeX];
}
// Line
DISP_DLL_EXPORT void SimDisp_LineH(int16_t x1, int16_t y1, int16_t x2, uint32_t color) {
	POS_ASSERT_X(x1);
	POS_ASSERT_Y(y1);
	POS_ASSERT_X(x2);
	auto p = pBmpBuffer + x1 + y1 * ScreenSizeX;
	for (uint16_t l = x2 - x1 + 1; l > 0; --l)
		*p++ = color;
}
DISP_DLL_EXPORT void SimDisp_LineV(int16_t x1, int16_t y1, int16_t y2, uint32_t color) {
	POS_ASSERT_X(x1);
	POS_ASSERT_Y(y1);
	POS_ASSERT_Y(y2);
	auto p = pBmpBuffer + x1 + y1 * ScreenSizeX;
	for (uint16_t l = y2 - y1 + 1; l > 0; --l) {
		*p = color;
		p += ScreenSizeX;
	}
}
// Rect
DISP_DLL_EXPORT void SimDisp_FillFull(uint32_t color) {
	auto p = pBmpBuffer;
	for (uint32_t i = ScreenSizeX * ScreenSizeY; i > 0; --i)
		*p++ = color;
}
DISP_DLL_EXPORT void SimDisp_FillRect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color) {
	POS_ASSERT_X(x1);
	POS_ASSERT_Y(y1);
	POS_ASSERT_X(x2);
	POS_ASSERT_Y(y2);
	auto p = pBmpBuffer + x1 + y1 * ScreenSizeX;
	for (uint16_t w = x2 - x1 + 1, h = y2 - y1 + 1, l = ScreenSizeX - w; h > 0; --h) {
		for (auto i = w; i > 0; --i)
			*p++ = color;
		p += l;
	}
}
DISP_DLL_EXPORT void SimDisp_SetBitmap(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint32_t *bmp, uint16_t line) {
	if (!bmp) {
		SIMDISP_WARN("arguments error");
		return;
	}
	uint16_t x2 = x + w - 1, y2 = y + h - 1;
	POS_ASSERT_X(x);
	POS_ASSERT_Y(y);
	POS_ASSERT_X(x2);
	POS_ASSERT_Y(y2);
	auto scr_buff = pBmpBuffer + x + y * ScreenSizeX;
	for (uint16_t i = 0; i < h; ++i) {
		auto line_scr = scr_buff;
		auto line_buf = bmp;
		for (uint16_t j = 0; j < w; ++j)
			*line_scr++ = *line_buf++;
		scr_buff += ScreenSizeX;
		bmp += line;
	}
}
DISP_DLL_EXPORT void SimDisp_GetBitmap(int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t *bmp, uint16_t line) {
	if (!bmp) {
		SIMDISP_WARN("NULL buffer");
		return;
	}
	int16_t x2 = x + w - 1, y2 = y + h - 1;
	POS_ASSERT_X(x);
	POS_ASSERT_Y(y);
	POS_ASSERT_X(x2);
	POS_ASSERT_Y(y2);
	auto scr_buff = pBmpBuffer + x + y * ScreenSizeX;
	for (uint16_t i = 0; i < h; ++i) {
		auto line_scr = scr_buff;
		auto line_buf = bmp;
		for (uint16_t j = 0; j < w; ++j)
			*line_buf++ = *line_scr++;
		scr_buff += ScreenSizeX;
		bmp += line;
	}
}

#pragma endregion

BOOL APIENTRY DllMain(HMODULE hModule,
					  DWORD  ul_reason_for_call,
					  LPVOID lpReserved) {
	_ConInit();
	switch (ul_reason_for_call) {
	case DLL_PROCESS_DETACH:
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}
