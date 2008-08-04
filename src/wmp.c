#include "musictracker.h"
#include "utils.h"
#include <windows.h>

void (*regserver)();
LPVOID WINAPI (*InitPlayer)(LPWSTR playerPath);
void WINAPI (*getState)(LPVOID pData, LPCWSTR cmd, LPWSTR state);
void WINAPI (*getText)(LPVOID pData, LPCWSTR cmd, LPWSTR text, int cbText);
UINT WINAPI (*getProp)(LPVOID pData, LPCWSTR cmd, UINT propID);
LPVOID wmp_data;

gboolean wmp_init()
{
	if (InitPlayer)
		return TRUE;

	HMODULE hModule = LoadLibrary("wmp9.dll");
	if (!hModule) {
		trace("Failed to load wmp9.dll");
		return FALSE;
	}

	regserver = GetProcAddress(hModule, "DllRegisterServer");
	InitPlayer = GetProcAddress(hModule, "InitPlayer");
	getState = GetProcAddress(hModule, "getState");
	getText = GetProcAddress(hModule, "getText");
	getProp = GetProcAddress(hModule, "getProp");
	if (regserver && InitPlayer && getState && getText && getProp) {
		(*regserver)();
		wmp_data = (*InitPlayer)("test");
		return TRUE;
	} else {
		InitPlayer = 0;
		return FALSE;
	}
}

#define WC_GET(buf, dest) \
	WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) buf, -1, dest, STRLEN, NULL, NULL); \
	dest[STRLEN-1] = 0; \

gboolean get_wmp_info(struct TrackInfo *ti)
{
	if (!wmp_init()) {
		trace("Failed to initialize wmp");
		return FALSE;
	}

	WCHAR buf[500];
	char buf2[STRLEN];
	(*getState)(wmp_data, L"playpause", buf);
	WC_GET(buf, buf2);
	if (strcmp(buf2, "play") == 0)
		ti->status = STATUS_NORMAL;
	else if (strcmp(buf2, "pause") == 0)
		ti->status = STATUS_PAUSED;
	else {
		ti->status = STATUS_OFF;
		return TRUE;
	}

	(*getText)(wmp_data, L"artist", buf, 500);
	WC_GET(buf, ti->artist);
	(*getText)(wmp_data, L"album", buf, 500);
	WC_GET(buf, ti->album);
	(*getText)(wmp_data, L"title", buf, 500);
	WC_GET(buf, ti->track);

	ti->totalSecs = (*getProp)(wmp_data, L"tracktime", 3);
	ti->currentSecs = (*getProp)(wmp_data, L"tracktime", 5);

	return TRUE;
}

