#ifndef HITZ_DEBUG_BANNER_H
#define HITZ_DEBUG_BANNER_H
#include <windows.h>
#define HITZ_DEBUG_BANNER_MESSAGE (WM_APP + 0x3D1)
void hitz_debug_banner_attach(HWND game);
int hitz_debug_banner_message(HWND game, UINT message, WPARAM w, LPARAM l);
#endif
