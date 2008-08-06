#include "musictracker.h"
#include "utils.h"
#include <windows.h>
#include "wa_ipc.h"

static HWND hWnd;
static HANDLE hProcess;

void winamp_get_w(const wchar_t *filename, const wchar_t *key, char *dest)
{
	// Allocate memory inside Winamp's address space to exchange data with it
	char *winamp_info = VirtualAllocEx(hProcess, NULL, 4096, MEM_COMMIT, PAGE_READWRITE);
	wchar_t *winamp_filename = (wchar_t*)(winamp_info+1024);
	wchar_t *winamp_key = (wchar_t*)(winamp_info+2048);
	wchar_t *winamp_value = (wchar_t*)(winamp_info+3072);

        // Setup structure with pointers into Winamp address space and store it into that space too
        extendedFileInfoStructW info;
        info.filename = winamp_filename;
        info.metadata = winamp_key;
        info.ret = winamp_value;	
        info.retlen = 1024;
        WriteProcessMemory(hProcess, winamp_info, &info, sizeof(info), NULL);

        WriteProcessMemory(hProcess, winamp_filename, filename, sizeof(wchar_t)*(wcslen(filename)+1), NULL);
	WriteProcessMemory(hProcess, winamp_key, key, sizeof(wchar_t)*(wcslen(key)+1), NULL);
	SendMessage(hWnd, WM_WA_IPC, (WPARAM)winamp_info, IPC_GET_EXTENDED_FILE_INFOW);

	SIZE_T bytesRead;
        wchar_t wdest[STRLEN];
	int rc = ReadProcessMemory(hProcess, winamp_value, wdest, STRLEN-1, &bytesRead);
	wdest[bytesRead] = 0;

        WideCharToMultiByte(CP_UTF8, 0, wdest, -1, dest, STRLEN, NULL, NULL);
 	trace("Got info '%s', return value %d", dest, rc);
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

	int position = SendMessage(hWnd, WM_WA_IPC, 0, IPC_GETLISTPOS);
        wchar_t wfilename[512];
	LPCVOID address = (LPCVOID) SendMessage(hWnd, WM_WA_IPC, position, IPC_GETPLAYLISTFILEW);
	ReadProcessMemory(hProcess, address, wfilename, 512, 0);
        char *f = wchar_to_utf8(wfilename);
	trace("Filename: %s", f);
        free(f);

        winamp_get_w(wfilename, L"ALBUM", ti->album);
        winamp_get_w(wfilename, L"ARTIST", ti->artist);
        winamp_get_w(wfilename, L"TITLE", ti->track);

        // if these are all empty, which seems to happen when listening to a stream, try something cruder
        // XXX: really should try to work out how to get winamp to resolve it's tag %streamtitle% for us...
        if ((strlen(ti->album) == 0) && (strlen(ti->artist) == 0) && (strlen(ti->track) == 0))
          {
            char *title = GetWindowTitleUtf8(hWnd);
            pcre *re;
            re = regex("\\d*\\. (.*) - Winamp", 0);
            capture(re, title, strlen(title), ti->track);
            free(title);
          }

	return TRUE;
}
