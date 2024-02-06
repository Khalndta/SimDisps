#define SIMDISPS_FOR_DLL_EXPORTS
#define SIMDISPS_CLIENT_DEF
#include "SiDi_Client.h"

#pragma region Debug outputs

#include "output.inl"
static inline void _ConInit() {
	hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
	sprintf_s(aNoticeId, "SiDi Client [PID:%X]", GetCurrentProcessId());
}

inline bool _POS_ASSERT_X(int16_t x, uint16_t ScreenSizeX, const char *Function) {
	if (0 <= x && x < ScreenSizeX)
		return false;
	SIMDISP_WARN("Warn %s x = %d range out", Function, x);
	return true;
}
inline bool _POS_ASSERT_Y(int16_t y, uint16_t ScreenSizeY, const char *Function) {
	if (0 <= y && y < ScreenSizeY)
		return false;
	SIMDISP_WARN("Warn %s y = %d range out", Function, y);
	return true;
}

#if _DEBUG
#	define POS_ASSERT_X(x) if (_POS_ASSERT_X(x, ScreenSizeX, __FUNCTION__)) return 1;
#	define POS_ASSERT_Y(y) if (_POS_ASSERT_Y(y, ScreenSizeY, __FUNCTION__)) return 1;
#else
#	define POS_ASSERT_X(...)
#	define POS_ASSERT_Y(...)
#endif

#define AssertStack(stack) \
	if (!stack) { \
		SIMDISP_ERR("argument error"); \
		return 1; \
	} \
	if (!stack->pInfoPage) { \
		SIMDISP_ERR("information page lost"); \
		return 1; \
	}
#define INVALID_HANDLE(h) (h == INVALID_HANDLE_VALUE || h == nullptr)

#pragma endregion

uint8_t SimDisps_Create(SimDisp_Stack *pStack, const char *pPathHost = "SiDi_Host.exe") {
	static auto OpenProcess = [](const char *cmdl) -> HANDLE {
		char aCmdl[MAX_PATH] = { 0 };
		strcpy_s(aCmdl, cmdl);
		STARTUPINFOA si = { sizeof(si) };
		PROCESS_INFORMATION pi = { 0 };
		if (!CreateProcessA(
			nullptr,
			aCmdl,
			nullptr,
			nullptr,
			false,
			DETACHED_PROCESS,
			nullptr,
			nullptr,
			&si,
			&pi)) {
			SIMDISP_ERR("CreateProcessA CommandLine:'%s' => GetLastError %d", cmdl, GetLastError());
			return nullptr;
		}
		SIMDISP_LOG("CreateProcessA CommandLine:'%s' => ProcessId:%Xh ", cmdl, pi.dwProcessId);
		return pi.hProcess;
	};
	static auto OpenSimDisp = [](SimDisp_Stack *pStack, const char *pPathHost) -> DWORD {
		// Create initalizing signal
		char hostName[MAX_PATH] = { 0 };
		strcpy_s(hostName, qstr("%X", GetCurrentProcessId()));
		struct __ {
			HANDLE hHostEvt;
			~__() { CloseHandle(hHostEvt); }
		} _;
		CreateEventReq(_.hHostEvt, hostName, return 0);
		if (!ResetEvent(_.hHostEvt)) {
			SIMDISP_ERR("ResetEvent Name:'%s'\n", hostName);
			return 0;
		}
		// Open process
		pStack->hpSimDisp = OpenProcess(qstr("%s %s", pPathHost, hostName));
		if (INVALID_HANDLE(pStack->hpSimDisp))
			return 0;
		// Wait for initialized
		SIMDISP_LOG("Wait for SimDisps initializing...");
		WaitForSingleObject(_.hHostEvt, INFINITE); ///////////////////////////
		SIMDISP_LOG("SimDisps opened");
		return GetProcessId(pStack->hpSimDisp);
	};
	if (!pStack)
		return 1;
	DWORD pid = OpenSimDisp(pStack, pPathHost);
	if (pid == 0)
		return 1;
	// Load shared memory
	OpenMemoryFile(pStack->pInfoPage, pStack->hInfoPage,
				   qstr(SIMDISPS_NAME_INFOPAGE, pid),
				   sizeof(*pStack->pInfoPage),
				   return 1);
	OpenMemoryFile(pStack->pBuffer, pStack->hBuffer,
				   qstr(SIMDISPS_NAME_BUFFER, pid),
				   pStack->pInfoPage->BufferSizeMax,
				   return 1);
	// Load events
	OpenEventReq(pStack->hEventI, qstr(SIMDISPS_NAME_EVENTI, pid), return 1);
	OpenEventReq(pStack->hEventO, qstr(SIMDISPS_NAME_EVENTO, pid), return 1);
	// Create event thread
	DWORD tid = 0;
	pStack->hWatchdog = CreateThread(nullptr, 512, [](LPVOID lpStack) -> DWORD {
		auto pStack = (SimDisp_Stack *)lpStack;
		pStack->HasRun = 1;
		for (;;) {
			DWORD ExitCode = 0;
			if (!GetExitCodeProcess(pStack->hpSimDisp, &ExitCode))
				break;
			if (ExitCode != STILL_ACTIVE)
				break;
		}
		pStack->HasRun = 0;
		return 0;
	}, pStack, 0, &tid);
	SimDisps_SetMouseActive(pStack, nullptr);
	SimDisps_SetTouchActive(pStack, nullptr);
	SimDisps_SetSizeActive(pStack, nullptr);
	return 0;
}

uint8_t SimDisps_Open(SimDisp_Stack *pStack) {
	AssertStack(pStack);
	pStack->pInfoPage->Do = eSimDisp_Open;
	SetEvent(pStack->hEventI);
	WaitForSingleObject(pStack->hEventO, INFINITE);
	return 0;
}
uint8_t SimDisps_Close(SimDisp_Stack *pStack) {
	AssertStack(pStack);
	pStack->pInfoPage->Do = eSimDisp_Close;
	SetEvent(pStack->hEventI);
	WaitForSingleObject(pStack->hEventO, INFINITE);
	return 0;
}
uint8_t SimDisps_GetSize(SimDisp_Stack *pStack, uint16_t *w, uint16_t *h) {
	AssertStack(pStack);
	pStack->pInfoPage->Do = eSimDisp_GetSize;
	SetEvent(pStack->hEventI);
	WaitForSingleObject(pStack->hEventO, INFINITE);
	*w = pStack->pInfoPage->SizeX;
	*h = pStack->pInfoPage->SizeY;
	return pStack->pInfoPage->LastState;
}
uint8_t SimDisps_SetSize(SimDisp_Stack *pStack, uint16_t w, uint16_t h) {
	AssertStack(pStack);
	pStack->pInfoPage->SizeX = w;
	pStack->pInfoPage->SizeY = h;
	pStack->pInfoPage->Do = eSimDisp_SetSize;
	SetEvent(pStack->hEventI);
	WaitForSingleObject(pStack->hEventO, INFINITE);
	return pStack->pInfoPage->LastState;
}
uint8_t SimDisps_SetTitle(SimDisp_Stack *pStack, const char *pTitle) {
	AssertStack(pStack);
	astr_t<sizeof(pStack->pInfoPage->Title)> &atitle =
		(astr_t<sizeof(pStack->pInfoPage->Title)> &)pStack->pInfoPage->Title;
	strcpy_s(atitle, pTitle);
	pStack->pInfoPage->Do = eSimDisp_SetTitle;
	SetEvent(pStack->hEventI);
	WaitForSingleObject(pStack->hEventO, INFINITE);
	return pStack->pInfoPage->LastState;
}
uint8_t SimDisps_Show(SimDisp_Stack *pStack, uint8_t s) {
	AssertStack(pStack);
	pStack->pInfoPage->WindowShow = s;
	pStack->pInfoPage->Do = eSimDisp_Show;
	SetEvent(pStack->hEventI);
	WaitForSingleObject(pStack->hEventO, INFINITE);
	return pStack->pInfoPage->LastState;
}
uint8_t SimDisps_HideCursor(SimDisp_Stack *pStack, uint8_t h) {
	AssertStack(pStack);
	pStack->pInfoPage->CursorHide = h;
	pStack->pInfoPage->Do = eSimDisp_HideCursor;
	SetEvent(pStack->hEventI);
	WaitForSingleObject(pStack->hEventO, INFINITE);
	return pStack->pInfoPage->LastState;
}
uint8_t SimDisps_Resizeable(SimDisp_Stack *pStack, uint8_t s) {
	AssertStack(pStack);
	pStack->pInfoPage->Resizeable = s;
	pStack->pInfoPage->Do = eSimDisp_Resizeable;
	SetEvent(pStack->hEventI);
	WaitForSingleObject(pStack->hEventO, INFINITE);
	return 0xff;
}
uint8_t SimDisps_SetMouseActive(SimDisp_Stack *pStack, tSimDisp_MouseEventDev pEvent) {
	AssertStack(pStack);
	pStack->pEventMouse = pEvent;
	DWORD ExitCode = 0;
	if (INVALID_HANDLE(pStack->pEventMouse))
		goto NoThread;
	if (!GetExitCodeThread(pStack->pEventMouse, &ExitCode))
		goto NoThread;
	if (ExitCode != STILL_ACTIVE)
		goto NoThread;
	return 0;
NoThread:
	DWORD tid = 0;
	pStack->hMouseProc = CreateThread(
		nullptr, 512,
		[](LPVOID lpStack) -> DWORD {
		auto pStack = (SimDisp_Stack *)lpStack;
		auto  pInfoPage = pStack->pInfoPage;
		while (pStack->HasRun) {
			if (pInfoPage->MouseActive) {
				if (pStack->pEventMouse)
					pStack->pEventMouse(
						(tSimDisp_MState &)pInfoPage->MouseState,
						pInfoPage->MouseX, pInfoPage->MouseY,
						pInfoPage->MouseWheel);
				pInfoPage->MouseActive = 0;
			}
		}
		return 0;
	}, pStack,
		0, &tid);
	if (INVALID_HANDLE(pStack->hMouseProc)) {
		SIMDISP_ERR("CreateThread pMouseProc => GetLastError:%d", GetLastError());
		return 1;
	}
	return 0;
}
uint8_t SimDisps_SetTouchActive(SimDisp_Stack *pStack, tSimDisp_TouchEventDev pEvent) {
	AssertStack(pStack);
	pStack->pEventTouch = pEvent;
	DWORD ExitCode = 0;
	if (INVALID_HANDLE(pStack->hTouchProc))
		goto NoThread;
	if (!GetExitCodeThread(pStack->hTouchProc, &ExitCode))
		goto NoThread;
	if (ExitCode != STILL_ACTIVE)
		goto NoThread;
	return 0;
NoThread:
	DWORD tid = 0;
	pStack->hTouchProc = CreateThread(
		nullptr, 512,
		[](LPVOID lpStack) -> DWORD {
		auto pStack = (SimDisp_Stack *)lpStack;
		auto  pInfoPage = pStack->pInfoPage;
		while (pStack->HasRun) {
			if (pInfoPage->TouchActive) {
				if (pStack->pEventTouch)
					pStack->pEventTouch(
						(tSimDisp_Point *)pInfoPage->TouchPoints,
						pInfoPage->TouchCount);
				pInfoPage->TouchActive = 0;
			}
		}
		return 0;
	}, pStack,
		0, &tid);
	if (INVALID_HANDLE(pStack->hTouchProc)) {
		SIMDISP_ERR("CreateThread pTouchProc => GetLastError:%d", GetLastError());
		return 1;
	}
	return 0;
}
uint8_t SimDisps_SetSizeActive(SimDisp_Stack *pStack, tSimDisp_SizeEventDev pEvent) {
	AssertStack(pStack);
	pStack->pEventSize = pEvent;
	DWORD ExitCode = 0;
	if (INVALID_HANDLE(pStack->hSizeProc))
		goto NoThread;
	if (!GetExitCodeThread(pStack->hSizeProc, &ExitCode))
		goto NoThread;
	if (ExitCode != STILL_ACTIVE)
		goto NoThread;
	return 0;
NoThread:
	DWORD tid = 0;
	pStack->hSizeProc = CreateThread(
		nullptr, 512,
		[](LPVOID lpStack) -> DWORD {
		auto pStack = (SimDisp_Stack *)lpStack;
		auto  pInfoPage = pStack->pInfoPage;
		while (pStack->HasRun) {
			if (pInfoPage->SizeActive) {
				if (pStack->pEventTouch)
					pInfoPage->SizeActive = pStack->pEventSize(
						pInfoPage->nSizeX,
						pInfoPage->nSizeY) ? 2 : 3;
				else
					pInfoPage->SizeActive = 2;
			}
		}
		return 0;
	}, pStack,
		0, &tid);
	if (INVALID_HANDLE(pStack->hSizeProc)) {
		SIMDISP_ERR("CreateThread pSize => GetLastError:%d", GetLastError());
		return 1;
	}
	return 0;
}

#pragma region Host function - 2

uint8_t SimDisps_SetPixel(SimDisp_Stack *pStack, int16_t x, int16_t y, uint32_t color) {
	AssertStack(pStack);
	auto pInfo = pStack->pInfoPage;
	uint16_t ScreenSizeX = pInfo->SizeX, ScreenSizeY = pInfo->SizeY;
	uint32_t *pBmpBuffer = pStack->pBuffer;
	POS_ASSERT_X(x);
	POS_ASSERT_Y(y);
	pBmpBuffer[x + y * ScreenSizeX] = color;
	return 0;
}
uint8_t SimDisps_GetPixel(SimDisp_Stack *pStack, int16_t x, int16_t y, uint32_t *color) {
	AssertStack(pStack);
	auto pInfo = pStack->pInfoPage;
	uint16_t ScreenSizeX = pInfo->SizeX, ScreenSizeY = pInfo->SizeY;
	uint32_t *pBmpBuffer = pStack->pBuffer;
	POS_ASSERT_X(x);
	POS_ASSERT_Y(y);
	*color = pBmpBuffer[x + y * ScreenSizeX];
	return 0;
}

uint8_t SimDisps_LineH(SimDisp_Stack *pStack, int16_t x1, int16_t y1, int16_t x2, uint32_t color) {
	AssertStack(pStack);
	auto pInfo = pStack->pInfoPage;
	uint16_t ScreenSizeX = pInfo->SizeX, ScreenSizeY = pInfo->SizeY;
	uint32_t *pBmpBuffer = pStack->pBuffer;
	POS_ASSERT_X(x1);
	POS_ASSERT_Y(y1);
	POS_ASSERT_X(x2);
	auto p = pBmpBuffer + x1 + y1 * ScreenSizeX;
	for (uint16_t l = x2 - x1 + 1; l > 0; --l)
		*p++ = color;
	return 0;
}
uint8_t SimDisps_LineV(SimDisp_Stack *pStack, int16_t x1, int16_t y1, int16_t y2, uint32_t color) {
	AssertStack(pStack);
	auto pInfo = pStack->pInfoPage;
	uint16_t ScreenSizeX = pInfo->SizeX, ScreenSizeY = pInfo->SizeY;
	uint32_t *pBmpBuffer = pStack->pBuffer;
	POS_ASSERT_X(x1);
	POS_ASSERT_Y(y1);
	POS_ASSERT_Y(y2);
	auto p = pBmpBuffer + x1 + y1 * ScreenSizeX;
	for (uint16_t l = y2 - y1 + 1; l > 0; --l) {
		*p = color;
		p += ScreenSizeX;
	}
	return 0;
}

uint8_t SimDisps_FillFull(SimDisp_Stack *pStack, uint32_t color) {
	AssertStack(pStack);
	auto pInfo = pStack->pInfoPage;
	uint16_t ScreenSizeX = pInfo->SizeX, ScreenSizeY = pInfo->SizeY;
	uint32_t *pBmpBuffer = pStack->pBuffer;
	auto p = pBmpBuffer;
	for (uint32_t i = ScreenSizeX * ScreenSizeY; i > 0; --i)
		*p++ = color;
	pStack->pBuffer[ScreenSizeX * ScreenSizeY - 1] = 0xff;
	return 0;
}
uint8_t SimDisps_FillRect(SimDisp_Stack *pStack, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color) {
	AssertStack(pStack);
	auto pInfo = pStack->pInfoPage;
	uint16_t ScreenSizeX = pInfo->SizeX, ScreenSizeY = pInfo->SizeY;
	uint32_t *pBmpBuffer = pStack->pBuffer;
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
	return 0;
}

uint8_t SimDisps_SetBitmap(SimDisp_Stack *pStack, int16_t x, int16_t y, uint16_t w, uint16_t h, const uint32_t *bmp, uint16_t line) {
	AssertStack(pStack);
	auto pInfo = pStack->pInfoPage;
	uint16_t ScreenSizeX = pInfo->SizeX, ScreenSizeY = pInfo->SizeY;
	uint32_t *pBmpBuffer = pStack->pBuffer;
	if (!bmp) {
		SIMDISP_WARN("arguments error");
		return 0;
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
			for (uint16_t j = 0; j < w; ++j)
				*line_scr++ = *line_buf++;
		scr_buff += ScreenSizeX;
		bmp += line;
	}
	return 0;
}
uint8_t SimDisps_GetBitmap(SimDisp_Stack *pStack, int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t *bmp, uint16_t line) {
	AssertStack(pStack);
	auto pInfo = pStack->pInfoPage;
	uint16_t ScreenSizeX = pInfo->SizeX, ScreenSizeY = pInfo->SizeY;
	uint32_t *pBmpBuffer = pStack->pBuffer;
	if (!bmp) {
		SIMDISP_WARN("buffer is NULL");
		return 0;
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
	return 0;
}
