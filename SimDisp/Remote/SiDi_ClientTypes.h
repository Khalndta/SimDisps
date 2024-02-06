#pragma once

#include "SiDi_Host.h"
#include "SimDispTypes.h"

struct SimDisp_Stack {

	HANDLE hpSimDisp;

	HANDLE hInfoPage;
	tSimDisp_InfoPage volatile *pInfoPage; // On shared

	HANDLE hBuffer;
	uint32_t *pBuffer;

	HANDLE hWatchdog;
	uint8_t HasRun;

	HANDLE hEventI;
	HANDLE hEventO;

	HANDLE hMouseProc;
	tSimDisp_MouseEventDev pEventMouse;

	HANDLE hTouchProc;
	tSimDisp_TouchEventDev pEventTouch;

	HANDLE hSizeProc;
	tSimDisp_SizeEventDev pEventSize;

};
