#pragma once

#include <windows.h>
#include <stdio.h>

static HANDLE hConsoleOut = NULL;
static char aNoticeId[128] = { 0 };

static inline void _ErrorOut(const char *s, ...) {
	va_list va;
	va_start(va, s);
	SetConsoleTextAttribute(hConsoleOut, 0x0C);
	printf("%s ", aNoticeId);
	printf("Error: ");
	vprintf_s(s, va);
	putchar('\n');
	va_end(va);
}
static inline void _WarnOut(const char *s, ...) {
	va_list va;
	va_start(va, s);
	SetConsoleTextAttribute(hConsoleOut, 0x0E);
	printf("%s ", aNoticeId);
	printf("Warn: ");
	vprintf_s(s, va);
	putchar('\n');
	va_end(va);
}
static inline void _LogOut(const char *s, ...) {
	va_list va;
	va_start(va, s);
	SetConsoleTextAttribute(hConsoleOut, 0x0A);
	printf("%s ", aNoticeId);
	printf("Log: ");
	vprintf_s(s, va);
	putchar('\n');
	va_end(va);
}

#define SIMDISP_ERR(...)  _ErrorOut(__FUNCTION__ " " __VA_ARGS__)
#define SIMDISP_WARN(...) _WarnOut(__FUNCTION__ " " __VA_ARGS__)
#define SIMDISP_LOG(...)  _LogOut(__FUNCTION__ " " __VA_ARGS__)

template<size_t s>
using astr_t = char[s];

static inline char *qstr(const char *s, ...) {
	static char buff[512];
	va_list va;
	va_start(va, s);
	vsprintf_s(buff, s, va);
	va_end(va);
	return buff;
}
