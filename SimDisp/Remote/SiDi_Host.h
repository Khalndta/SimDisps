#pragma once

#include "SimDispTypes.h"

enum tSimDisp_Operation {
	eSimDisp_Idle,
	eSimDisp_Open,
	eSimDisp_Close,
	eSimDisp_Show,
	eSimDisp_GetSize,
	eSimDisp_SetSize,
	eSimDisp_SetTitle,
	eSimDisp_AutoFlusher,
	eSimDisp_Flush,
	eSimDisp_HideCursor,
	eSimDisp_Resizeable,
};

struct tSimDisp_InfoPage {
	// Cursor control
	/* R/W */ uint8_t MouseActive;
	/* R   */ tSimDisp_MState MouseState;
	/* R   */ int32_t MouseX, MouseY, MouseWheel;
	/* R/W */ uint8_t CursorHide;
	// Touch control
	/* R   */ uint8_t TouchActive;
	/* R   */ uint8_t TouchCount;
	/* R   */ tSimDisp_Point TouchPoints[10];
	// Size & Resize
	/*   W */ uint8_t SizeActive, Resizeable;
	/* R/W */ uint16_t SizeX, nSizeX;
	/* R/W */ uint16_t SizeY, nSizeY;
	// Flusher
	/*   W */ uint8_t AutoFlusher;
	/*   W */ uint32_t BufferSizeMax;
	// Window control
	/* R/W */ uint8_t WindowShow;
	/*   W */ char Title[255];
	// State control
	/*   W */ tSimDisp_Operation Do;
	/* R   */ uint8_t LastState; // 1:Failed, 0:Successed
};

#if defined(SIMDISPS_HOST_DEF) || defined(SIMDISPS_CLIENT_DEF)

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define SIMDISPS_NAME_EVENTI	 "SimDispI%x"
#define SIMDISPS_NAME_EVENTO	 "SimDispO%x"
#define SIMDISPS_NAME_BUFFER	 "SimDispBuffer%x"
#define SIMDISPS_NAME_INFOPAGE	 "SimDispInf%x"

#define CreateEventReq(hEvent, pName, OnErr) { \
	const char *Name = pName; \
	hEvent = CreateEventA(nullptr, false, false, Name); \
	if (INVALID_HANDLE(hEvent)) { \
		SIMDISP_ERR("%s CreateEventA '%s'", Name); \
		OnErr; } \
	SIMDISP_LOG("%s CreataEventA Name:'%s'", Name); }
#define OpenEventReq(hEvent, pName, OnErr) { \
	const char *Name = pName; \
	hEvent = OpenEventA(EVENT_ALL_ACCESS, false, Name); \
	if (INVALID_HANDLE(hEvent)) { \
		SIMDISP_ERR("OpenEventA '%s'", Name); \
		OnErr; } \
	SIMDISP_LOG("OpenEventA Name:'%s'", Name); }
#define CreateMemoryFile(ptr, hShare, Name, Size, OnErr) { \
	auto pName = Name; \
	hShare = CreateFileMappingA( \
		nullptr, nullptr, \
		PAGE_READWRITE, 0, \
		Size, Name); \
	if (INVALID_HANDLE(hShare)) { \
		SIMDISP_ERR("CreateFileMappingA '%s' Size:%d", Name, Size); \
		OnErr; } \
	SIMDISP_LOG("CreateFileMappingA Name:'%s' Size:%d", Name, Size); \
	ptr = (decltype(ptr))MapViewOfFile(hShare, \
		FILE_MAP_ALL_ACCESS, 0, 0, Size); \
	if (ptr == nullptr) { \
		SIMDISP_ERR("MapViewOfFile '%s' Size:%d", Name, Size); \
		OnErr; } \
	memset(ptr, 0, Size); }
#define OpenMemoryFile(ptr, hShare, Name, Size, OnErr) { \
	hShare = OpenFileMappingA(FILE_MAP_ALL_ACCESS, false, Name); \
	if (INVALID_HANDLE(hShare)) { \
		SIMDISP_ERR("OpenFileMappingA Name:%s Size:%d", Name, Size); \
		OnErr; } \
	SIMDISP_LOG("OpenFileMappingA Name:'%s' Size:%d", Name, Size); \
	ptr = (decltype(ptr))MapViewOfFile(hShare, \
		FILE_MAP_ALL_ACCESS, 0, 0, Size); \
	if (ptr == nullptr) { \
		SIMDISP_ERR("MapViewOfFile '%s' Size:%d", Name, Size); \
		OnErr; } }
#define INVALID_HANDLE(h) (h == INVALID_HANDLE_VALUE || h == nullptr)

#endif
