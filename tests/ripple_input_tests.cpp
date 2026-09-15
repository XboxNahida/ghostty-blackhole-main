#include "../src/ripple_input.h"
#include <cstdio>
static int delivered=0;
static LRESULT CALLBACK windowProc(HWND h,UINT m,WPARAM w,LPARAM l) {
    if (m==WM_LBUTTONDOWN) ++delivered;
    return DefWindowProcW(h,m,w,l);
}
static void pump() {
    const ULONGLONG end=GetTickCount64()+100;
    while(GetTickCount64()<end) {
        MSG msg;
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(1);
    }
}
int main() {
    SetProcessDPIAware();
    POINT old{}; if (!GetCursorPos(&old)) return 1;
    WNDCLASSW wc{}; wc.lpfnWndProc=windowProc; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"V3RippleInputTest";
    RegisterClassW(&wc);
    HWND h=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,wc.lpszClassName,L"V3 click test",WS_POPUP,
        20,20,160,100,nullptr,nullptr,wc.hInstance,nullptr);
    if (!h) return 2;
    ShowWindow(h,SW_SHOWNOACTIVATE); pump();
    RippleInput input; bool ok=input.enable(true);
    std::printf("hook enabled=%d error=%lu\n",ok,GetLastError());
    POINT target{80,50}; ClientToScreen(h,&target);
    SetCursorPos(target.x,target.y); pump();
    if(WindowFromPoint(target)!=h) { std::printf("test window occluded at %ld,%ld\n",target.x,target.y); ok=false; }
    if(ok) {
        INPUT events[2]{};
        events[0].type=INPUT_MOUSE; events[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
        events[1].type=INPUT_MOUSE; events[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        for(int i=0;i<3;++i) { const UINT sent=SendInput(2,events,sizeof(INPUT)); std::printf("sent=%u error=%lu\n",sent,GetLastError()); if(sent!=2) ok=false; pump(); }
    }
    RippleState state; input.drain(state,20,20,160,100,1.0);
    std::printf("ripple count=%d\n",state.count);
    ok=ok && delivered==3 && state.count==3;
    if(state.count) ok=ok && std::abs(state.events[0].x-0.5f)<0.02f && std::abs(state.events[0].y-0.5f)<0.02f;
    input.enable(false); state.clear();
    input.drain(state,20,20,160,100,2); ok=ok && state.count==0;
    DestroyWindow(h); SetCursorPos(old.x,old.y);
    std::printf("clicks delivered=%d; hook %s\n",delivered,ok?"PASS":"FAIL");
    return ok?0:3;
}
