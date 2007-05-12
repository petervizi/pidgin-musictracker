#include "musictracker.h"
#include "utils.h"
#include <windows.h>
#include "wa_ipc.h"

LPVOID winamp_info;
char *winamp_filename;
char *winamp_key;
char *winamp_value;
HWND hWnd;
HANDLE hProcess;

void winamp_get(const char *key, char *dest)
{
	WriteProcessMemory(hProcess, winamp_key, key, strlen(key), NULL);
	SendMessage(hWnd, WM_WA_IPC, winamp_info, IPC_GET_EXTENDED_FILE_INFO);

	SIZE_T bytesRead;
	ReadProcessMemory(hProcess, winamp_value, dest, STRLEN-1, &bytesRead);
	dest[bytesRead] = 0;
}

gboolean get_winamp_info(struct TrackInfo* ti)
{
	hWnd = FindWindow("Winamp v1.x", NULL);
	if (!hWnd) {
		trace("Failed to find winamp window. Assuming player is off");
		ti->status = STATUS_OFF;
		return TRUE;
	}
	int version = SendMessage(hWnd, WM_WA_IPC, 0, IPC_GETVERSION);

	char title[STRLEN*5];
	GetWindowText(hWnd, title, STRLEN*5);
	trace("Got window title: %s", title);
	
	DWORD processId;
	GetWindowThreadProcessId(hWnd, &processId);
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, 0, processId);

	int playing = SendMessage(hWnd, WM_WA_IPC, 1, IPC_ISPLAYING);
	if (playing == 0)
		ti->status = STATUS_OFF;
	else if (playing == 3)
		ti->status = STATUS_PAUSED;
	else
		ti->status = STATUS_NORMAL;

	ti->totalSecs = SendMessage(hWnd, WM_WA_IPC, 1, IPC_GETOUTPUTTIME);
	ti->currentSecs = SendMessage(hWnd, WM_WA_IPC, 0, IPC_GETOUTPUTTIME)/1000;

	char filename[512];
	int position = SendMessage(hWnd, WM_WA_IPC, 0, IPC_GETLISTPOS);
	LPCVOID address = (LPCVOID) SendMessage(hWnd, WM_WA_IPC, position, IPC_GETPLAYLISTFILE);
	ReadProcessMemory(hProcess, address, filename, 512, 0);
	trace("Filename: %s", filename);

	/*BOOL unicode;
	if (version < 20517)
		unicode = FALSE;
	else
		unicode = TRUE;*/

	char buf[512];
	address = SendMessage(hWnd, WM_WA_IPC, position, IPC_GETPLAYLISTTITLE);
	ReadProcessMemory(hProcess, address, buf, 512, 0);
	static pcre *re;
	if (!re)
		re = regex("(.*) - (.*)", 0);
	capture(re, buf, strlen(buf), ti->artist, ti->track);

	// Allocate memory inside Winamp's address space to exchange data with it
	winamp_info = VirtualAllocEx(hProcess, NULL, 4096, MEM_COMMIT, PAGE_READWRITE);
	winamp_filename = ((char*)winamp_info+1024);
	winamp_key = ((char*)winamp_info+2048);
	winamp_value = ((char*)winamp_info+3072);

	// Setup structure with pointers into Winamp address space and store it into that space too
	extendedFileInfoStruct info;
	info.filename = winamp_filename;
	info.metadata = winamp_key;
	info.ret = winamp_value;	
	info.retlen = 1024;
	WriteProcessMemory(hProcess, winamp_info, &info, sizeof(info), NULL);
	WriteProcessMemory(hProcess, winamp_filename, filename, strlen(filename)+1, NULL);

	winamp_get("ALBUM", ti->album);
	winamp_get("ARTIST", ti->artist);
	//winamp_get("title", ti->track);
	VirtualFreeEx(hProcess, winamp_info, 0, MEM_RELEASE);
	return TRUE;
}
