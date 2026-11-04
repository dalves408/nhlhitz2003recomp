#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/hitz_debug_banner.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s (error %lu)\n",__LINE__,#x,GetLastError());exit(1);}}while(0)
static LRESULT CALLBACK fixture(HWND h,UINT m,WPARAM w,LPARAM l)
{
    if(hitz_debug_banner_message(h,m,w,l))return 0;
    return DefWindowProcA(h,m,w,l);
}
static void pump(void)
{
    MSG m;while(PeekMessageA(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}
}
static void contained(HWND game)
{
    RECT b,c;POINT p={0,0};
    CHECK(hitz_banner_window&&IsChild(game,hitz_banner_window));
    CHECK(GetParent(hitz_banner_window)==game);
    CHECK((GetWindowLongPtrA(hitz_banner_window,GWL_STYLE)&WS_CHILD)!=0);
    CHECK((GetWindowLongPtrA(hitz_banner_window,GWL_EXSTYLE)&WS_EX_TOPMOST)==0);
    CHECK(GetClientRect(game,&c)&&ClientToScreen(game,&p)&&GetWindowRect(hitz_banner_window,&b));
    CHECK(b.left>=p.x&&b.top>=p.y&&b.right<=p.x+c.right&&b.bottom<=p.y+c.bottom);
}
int main(void)
{
    WNDCLASSA wc={0};wc.lpfnWndProc=fixture;wc.hInstance=GetModuleHandleA(NULL);wc.lpszClassName="HitzBannerFixture";
    CHECK(RegisterClassA(&wc));
    show_banner("DEBUG BUILD","NHL Hitz 2003 v1.0.1 (debug)","Queued until the game window exists",5000,RGB(255,122,0));
    CHECK(!hitz_banner_window&&hitz_banner_pending);
    HWND h=CreateWindowA(wc.lpszClassName,"Banner test",WS_OVERLAPPEDWINDOW,100,100,656,519,NULL,NULL,wc.hInstance,NULL);CHECK(h);
    hitz_debug_banner_attach(h);contained(h);
    CHECK(hitz_banner_pending==NULL);
    CHECK(SetWindowPos(h,NULL,240,180,350,300,SWP_NOZORDER|SWP_NOACTIVATE));pump();contained(h);
    SetWindowLongPtrA(h,GWL_STYLE,WS_POPUP|WS_CLIPCHILDREN);
    CHECK(SetWindowPos(h,NULL,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED));pump();contained(h);
    CHECK(SetWindowPos(h,NULL,80,80,30,10,SWP_NOZORDER|SWP_NOACTIVATE));pump();contained(h);
    show_banner("MARKED","v1.0.1","Replaces the previous banner within the same parent",5000,RGB(10,200,80));contained(h);
    PostMessageA(hitz_banner_window,WM_TIMER,1,0);pump();CHECK(!hitz_banner_window);
    show_banner("DEBUG BUILD","v1.0.1","Parent destruction cleans up the child",5000,RGB(255,122,0));contained(h);
    CHECK(DestroyWindow(h));pump();CHECK(!hitz_banner_game&&!hitz_banner_window);
    puts("PASS: deferred startup, child containment, move/resize/fullscreen/tiny-window bounds, replacement, expiry and parent cleanup");return 0;
}
