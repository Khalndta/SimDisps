#define SIMDISP_DLL_DEF
#include "SimDisp.h"

#define SIMDISPS_HOST_DEF
#include "SiDi_Host.h"

#pragma region Variables

DWORD dwParentPid = 0;
HANDLE hpParent = nullptr;

HANDLE hInfoPage = nullptr;
tSimDisp_InfoPage *pInfoPage = nullptr;

HANDLE hBuffer = nullptr;
uint32_t *pBuffer = nullptr;

HANDLE hEventI = nullptr;
HANDLE hEventO = nullptr;

int MaxSizeX = 0;
int MaxSizeY = 0;

#pragma endregion

#pragma region Debug outputs

#include "output.inl"
static inline void _ConInit() {
	FILE *fOut, *fIn;
	AllocConsole();
	freopen_s(&fOut, "CONOUT$", "w+t", stdout);
	freopen_s(&fIn, "CONIN$", "r+t", stdin);
	hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
	sprintf_s(aNoticeId, "SiDi Host [PID:%X]", GetCurrentProcessId());
}

#pragma endregion

static bool SimDisps_Init(const char *pCmdLine) {
	auto SharedInit = [](const char *Host) {
		DWORD pid = GetCurrentProcessId();
		CreateMemoryFile(pInfoPage, hInfoPage, 
						 qstr(SIMDISPS_NAME_INFOPAGE, pid), 
						 sizeof(*pInfoPage),
						 return true);
		MaxSizeX = GetSystemMetrics(SM_CXSCREEN);
		MaxSizeY = GetSystemMetrics(SM_CYSCREEN);
		pInfoPage->BufferSizeMax = SimDisp_CalcBufferSize(MaxSizeX, MaxSizeY);
		CreateMemoryFile(pBuffer, hInfoPage,
						 qstr(SIMDISPS_NAME_BUFFER, pid),
						 pInfoPage->BufferSizeMax, 
						 return true);
		memset(pBuffer, ~0, pInfoPage->BufferSizeMax);
		CreateEventReq(hEventI, qstr(SIMDISPS_NAME_EVENTI, pid), return true);
		CreateEventReq(hEventO, qstr(SIMDISPS_NAME_EVENTO, pid), return true);
		return false;
	};
	SIMDISP_LOG("lpCmdLine:'%s'", pCmdLine);
	/* Attach to master process */
	sscanf_s(pCmdLine, "%X", &dwParentPid);
	hpParent = OpenProcess(PROCESS_QUERY_INFORMATION, false, dwParentPid);
	if (INVALID_HANDLE(hpParent)) {
		SIMDISP_ERR("OpenProcess %X GetLastError %d", dwParentPid, GetLastError());
		return true;
	}
	/* Create watchdog */
	DWORD tidWatchdogProc = 0;
	HANDLE hWatchdogProc = nullptr;
	auto pWatchdogProc = [](LPVOID) -> DWORD {
		for (;;) {
			DWORD ExitCode = 0;
			if (!GetExitCodeProcess(hpParent, &ExitCode)) {
				SIMDISP_ERR("GetExitCodeProcess GetLastError:%d", GetLastError());
				exit(0);
			}
			if (ExitCode != STILL_ACTIVE) {
				SIMDISP_LOG("master process exit ExitCode:%Xh", ExitCode);
				exit(0);
			}
		}
	};
	hWatchdogProc = CreateThread(
		nullptr,
		1024,
		pWatchdogProc,
		nullptr,
		0,
		&tidWatchdogProc);
	if (INVALID_HANDLE(hWatchdogProc)) {
		SIMDISP_ERR("CreateThread pWatchdogProc GetLastError %d", GetLastError());
		return true;
	}
	SIMDISP_LOG("CreateThread pWatchdogProc");
	/* Open initalizing signal */
	struct __ {
		HANDLE hHostInit = nullptr;
		~__() { SetEvent(hHostInit); }
	} _;
	OpenEventReq(_.hHostInit, pCmdLine, return true);
	if (SharedInit(pCmdLine))
		return true;
	/* Specialfy SimDisp configures */
	if (SimDisp_InitDll("SimDisp.dll")) {
		SIMDISP_ERR("SimDisp_InitDll Filename:'%s'", "SimDisp.dll");
		return true;
	}
	MaxSizeX = GetSystemMetrics(SM_CXFULLSCREEN);
	MaxSizeY = GetSystemMetrics(SM_CYFULLSCREEN);
	SimDisp_SetMaxSize(MaxSizeX, MaxSizeY);
//	SimDisp_SetBuffer(pBuffer);
	SimDisp_SetMouseActive([](tSimDisp_MState state, int32_t xPos, int32_t yPos, int32_t wheel_delta) {
		pInfoPage->MouseX = xPos;
		pInfoPage->MouseY = yPos;
		pInfoPage->MouseWheel = wheel_delta;
		pInfoPage->MouseState = state;
		pInfoPage->MouseActive = 1;
//		while (pInfoPage->MouseActive == 1) {}
	});
	SimDisp_SetTouchActive([](tSimDisp_Point *points, uint8_t point_count) {
		pInfoPage->TouchCount = point_count;
		memcpy(pInfoPage->TouchPoints, points, sizeof(tSimDisp_Point) * point_count);
		pInfoPage->TouchActive = 1;
//		while (pInfoPage->TouchActive == 1) {}
	});
	SimDisp_SetResizeActive([](uint16_t nWidth, uint16_t nHeight) -> uint8_t {
//		if (nWidth > MaxSizeX || nHeight > MaxSizeY)
//			return 0;
//		pInfoPage->nSizeX = nWidth;
//		pInfoPage->nSizeY = nHeight;
//		pInfoPage->SizeActive = 1;
////		while (pInfoPage->SizeActive == 1) {}
//		if (pInfoPage->SizeActive == 2) {
//			pInfoPage->SizeX = nWidth;
//			pInfoPage->SizeY = nHeight;
//			return 0;
//		}
		return 1;
	});
	SIMDISP_LOG("initialzed");
	return false;
}

static void ListenEvent() {
	auto RequirEvent = []() {
		pInfoPage->LastState = 0;
		switch (pInfoPage->Do) {
		case eSimDisp_Idle:
			return;
		case eSimDisp_Open:
			SIMDISP_LOG("SimDisp_Open");
			pInfoPage->LastState = SimDisp_Open();
			SimDisp_FillFull(0xff);
			return;
		case eSimDisp_Close:
			SIMDISP_LOG("SimDisp_Close");
			SimDisp_Close();
			return;
		case eSimDisp_Show:
			SIMDISP_LOG("SimDisp_Show");
			SimDisp_Show(pInfoPage->WindowShow);
			return;
		case eSimDisp_GetSize:
			SIMDISP_LOG("SimDisp_GetSize");
			pInfoPage->SizeX = SimDisp_GetSizeX();
			pInfoPage->SizeY = SimDisp_GetSizeY();
			return;
		case eSimDisp_SetSize:
			SIMDISP_LOG("SimDisp_SetSize");
			pInfoPage->LastState = SimDisp_SetSize(
				pInfoPage->SizeX, pInfoPage->SizeY);
			return;
		case eSimDisp_SetTitle:
			SIMDISP_LOG("SimDisp_SetTitle");
			SimDisp_SetTitle(pInfoPage->Title);
			return;
		case eSimDisp_AutoFlusher:
			SIMDISP_LOG("SimDisp_AutoFlusher");
			pInfoPage->LastState = SimDisp_AutoFlusher(pInfoPage->AutoFlusher);
			return;
		case eSimDisp_Flush:
			SIMDISP_LOG("SimDisp_Flush");
			SimDisp_Flush();
			return;
		case eSimDisp_HideCursor:
			SIMDISP_LOG("SimDisp_HideCursor");
			SimDisp_HideCursor(pInfoPage->CursorHide);
			return;
		case eSimDisp_Resizeable:
			SIMDISP_LOG("SimDisp_Resizeable");
			SimDisp_Resizeable(pInfoPage->Resizeable);
			return;
		default:
			SIMDISP_WARN("Unknown");
			break;
		}
		pInfoPage->LastState = 1;
	};
	SIMDISP_LOG("started");
	for (;;) {
		WaitForSingleObject(hEventI, INFINITE);
		ResetEvent(hEventO);
		ResetEvent(hEventI);
		RequirEvent();
		SetEvent(hEventO);
	}
}

int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nShowCmd
) {
	_ConInit();
	if (SimDisps_Init(lpCmdLine))
		return 1;
	ListenEvent();
	return 0;
}
