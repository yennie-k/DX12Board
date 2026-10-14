
#include <windows.h>

// Windows에서 발생하는 이벤트를 처리하는 함수
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg,
    WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// 프로그램 시작점
int WINAPI wWinMain(HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"DX12ChalkBoardWindow";

    // 1. 창 클래스 설정
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    // 2. 창 클래스 등록
    if (!RegisterClassExW(&wc))
        return 0;

    // 3. 프로그램 창 생성
    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"DX12 Board",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        800,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (hwnd == nullptr)
        return 0;

    // 4. 창 표시
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 5. 프로그램 이벤트 처리
    MSG msg = {};

    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
