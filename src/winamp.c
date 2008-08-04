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
	WriteProcessMemory(hProcess, winamp_key, key, strlen(key)+1, NULL);
	SendMessage(hWnd, WM_WA_IPC, (WPARAM)winamp_info, IPC_GET_EXTENDED_FILE_INFO);

	SIZE_T bytesRead;
	int rc = ReadProcessMemory(hProcess, winamp_value, dest, STRLEN-1, &bytesRead);
	dest[bytesRead] = 0;

 	trace("Got info for key '%s' is '%s', return value %d", key, dest, rc);
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
	winamp_get("title", ti->track);

	VirtualFreeEx(hProcess, winamp_info, 0, MEM_RELEASE);

        // if these are all empty, which seems to happen when listening to a stream, try something cruder
        // XXX: really should try to work out how to get winamp to resolve it's tag %streamtitle% for us...
        if ((strlen(ti->album) == 0) && (strlen(ti->artist) == 0) && (strlen(ti->track) == 0))
          {
            pcre *re;
            re = regex("\\d*\\. (.*) - Winamp", 0);
            capture(re, title, strlen(title), ti->track);
          }

	return TRUE;
}
