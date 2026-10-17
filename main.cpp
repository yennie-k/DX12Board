
#include <windows.h>
#include <windowsx.h>
#include <string>
#include "DX12App.h"


// 저장 및 불러오기 버튼 ID
constexpr int ID_SAVE_BUTTON = 1001;
constexpr int ID_LOAD_BUTTON = 1002;
constexpr int ID_STATUS_TEXT = 1003;
constexpr int ID_CLEAR_BUTTON = 1004;
constexpr int ID_TOOL_TEXT = 1005;
constexpr int ID_BRUSH_MINUS = 1006;
constexpr int ID_BRUSH_PLUS = 1007;
constexpr int ID_BRUSH_SIZE_TEXT = 1008;


// DirectX 12 렌더링 전용 자식 창
LRESULT CALLBACK RenderWndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    HWND parent = GetParent(hwnd);

    switch (msg)
    {
    case WM_ERASEBKGND:
        return DefWindowProcW(
            hwnd, msg, wParam, lParam
        );

        // 마우스 입력을 기존 WndProc으로 전달
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    {
        SetFocus(parent);

        SendMessageW(
            parent,
            msg,
            wParam,
            lParam
        );

        return 0;
    }

    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    {
        SendMessageW(
            parent,
            msg,
            wParam,
            lParam
        );

        return 0;
    }
    }

    return DefWindowProcW(
        hwnd, msg, wParam, lParam
    );
}

// Windows 메시지 처리
LRESULT CALLBACK WndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    // 우클릭 드래그 상태
    static bool rightDragging = false;

    // 마지막 마우스 위치
    static POINT lastRightMouse = {};

    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

        // 우클릭 시작: 카메라 회전 준비
    case WM_RBUTTONDOWN:
    {
        // 좌클릭 분필 사용 중에는 회전하지 않음
        if (wParam & MK_LBUTTON)
            return 0;

        DX12App* app = reinterpret_cast<DX12App*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA)
            );

        if (app != nullptr)
        {
            rightDragging = true;

            lastRightMouse.x = GET_X_LPARAM(lParam);
            lastRightMouse.y = GET_Y_LPARAM(lParam);

            SetCapture(hwnd);
        }

        return 0;
    }

    // 우클릭 종료
    case WM_RBUTTONUP:
    {
        if (rightDragging)
        {
            rightDragging = false;

            if (GetCapture() == hwnd)
                ReleaseCapture();
        }

        return 0;
    }

    // 마우스 왼쪽 버튼 누르기
    case WM_LBUTTONDOWN:
    {
        DX12App* app = reinterpret_cast<DX12App*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA)
            );

        if (app != nullptr)
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            if (app->BeginStroke(x, y))
            {
                SetCapture(hwnd);
            }
        }

        return 0;
    }

    // 마우스 왼쪽 버튼 놓기
    case WM_LBUTTONUP:
    {
        DX12App* app = reinterpret_cast<DX12App*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA)
            );

        if (app != nullptr)
            app->EndStroke();

        if (GetCapture() == hwnd)
            ReleaseCapture();

        return 0;
    }

    // 예기치 않게 마우스 캡처를 잃었을 때
    case WM_CAPTURECHANGED:
    {
        DX12App* app = reinterpret_cast<DX12App*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA)
            );

        if (app != nullptr)
            app->EndStroke();

        rightDragging = false;

        return 0;
    }

    case WM_MOUSEMOVE:
    {
        // 현재 DirectX 12 객체 가져오기
        DX12App* app = reinterpret_cast<DX12App*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA)
            );

        if (app == nullptr)
            break;

        float u = 0.0f;
        float v = 0.0f;

        int mouseX = GET_X_LPARAM(lParam);
        int mouseY = GET_Y_LPARAM(lParam);

        // 우클릭 드래그: 카메라 회전
        if (rightDragging && (wParam & MK_RBUTTON))
        {
            int deltaX = mouseX - lastRightMouse.x;
            int deltaY = mouseY - lastRightMouse.y;

            app->RotateCamera(deltaX, deltaY);

            lastRightMouse.x = mouseX;
            lastRightMouse.y = mouseY;
        }
        // 좌클릭 드래그: 분필로 그리기
        else if (wParam & MK_LBUTTON)
        {
            app->ContinueStroke(mouseX, mouseY);
        }

        if (app->PickBoard(mouseX, mouseY, u, v))
        {
            // 칠판 위에 있을 때 UV 좌표 표시
            wchar_t title[128];

            wsprintfW(
                title,
                L"DX12 Board | U: %d%% V: %d%% | Strokes: %u",
                static_cast<int>(u * 100.0f),
                static_cast<int>(v * 100.0f),
                app->GetStrokeCount()
            );


            SetWindowTextW(hwnd, title);
        }
        else
        {
            // 칠판 밖에 있을 때
            SetWindowTextW(hwnd, L"DX12 Board");
        }

        return 0;
    }


    case WM_KEYDOWN:
    {
        DX12App* app = reinterpret_cast<DX12App*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA)
            );

        if (app == nullptr)
            break;

        switch (wParam)
        {
            // 카메라 정면 초기화
        case 'R':
            app->ResetCamera();
            return 0;

            // 1: 흰색
        case '1':
            app->SetChalkColor(0xFFE8F4F8u);
            return 0;

            // 2: 노란색
        case '2':
            app->SetChalkColor(0xFF8AE5FFu);
            
            return 0;

            // 3: 주황색
        case '3':
            app->SetChalkColor(0xFF61A2F4u);
            return 0;

            // 4: 분홍색
        case '4':
            app->SetChalkColor(0xFFB8A9F5u);
            return 0;

            // 5: 하늘색
        case '5':
            app->SetChalkColor(0xFFE5D6A9u);
            return 0;

            // 6: 연두색
        case '6':
            app->SetChalkColor(0xFFC7E4B7u);
            return 0;

            // E: 지우개
        case 'E':
            app->SelectEraser();
            return 0;
        }

        break;
    }

    // Save / Load 버튼 클릭
    case WM_COMMAND:
    {
        if (HIWORD(wParam) != BN_CLICKED)
            break;

        DX12App* app = reinterpret_cast<DX12App*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA)
            );

        if (app == nullptr)
            break;

        // 오른쪽 패널 상태 텍스트
        HWND statusText = GetDlgItem(
            hwnd,
            ID_STATUS_TEXT
        );

        // 저장 버튼
        if (LOWORD(wParam) == ID_SAVE_BUTTON)
        {
            bool success = app->SaveBoard();

            if (statusText)
            {
                SetWindowTextW(
                    statusText,
                    success
                    ? L"Saved successfully!"
                    : L"Save failed!"
                );
            }

            return 0;
        }

        // 불러오기 버튼
        if (LOWORD(wParam) == ID_LOAD_BUTTON)
        {
            bool success = app->LoadBoard();

            if (statusText)
            {
                SetWindowTextW(
                    statusText,
                    success
                    ? L"Loaded successfully!"
                    : L"Load failed!"
                );
            }

            return 0;
        }

        // 전체 지우기 버튼
        if (LOWORD(wParam) == ID_CLEAR_BUTTON)
        {
            app->ClearBoard();

            if (statusText)
            {
                SetWindowTextW(
                    statusText,
                    L"Board cleared!"
                );
            }

            return 0;
        }

        // 현재 도구 굵기 감소
        if (LOWORD(wParam) == ID_BRUSH_MINUS)
        {
            app->DecreaseBrushSize();
            return 0;
        }

        // 현재 도구 굵기 증가
        if (LOWORD(wParam) == ID_BRUSH_PLUS)
        {
            app->IncreaseBrushSize();
            return 0;
        }

        break;
    }


    }
    return DefWindowProcW(
        hwnd, msg, wParam, lParam);
}

// 프로그램 시작
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int nCmdShow)
{
    const wchar_t CLASS_NAME[] =
        L"DX12BoardWindow";

    // 1. Windows 창 클래스 설정
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(
        nullptr, IDC_ARROW);
    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);


    if (!RegisterClassExW(&wc))
        return 0;

    // 2. 창 스타일 설정
    DWORD windowStyle =
        WS_OVERLAPPED |
        WS_CAPTION |
        WS_SYSMENU |
        WS_MINIMIZEBOX |
        WS_CLIPCHILDREN;

    RECT rect = {
        0,
        0,
        static_cast<LONG>(DX12App::WINDOW_WIDTH) + 180,
        static_cast<LONG>(DX12App::WINDOW_HEIGHT)
    };

    AdjustWindowRect(
        &rect,
        windowStyle,
        FALSE);

    // 3. Windows 창 생성
    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"DX12 Board",
        windowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd)
        return 0;

    // =====================================
    // DirectX 12 렌더링 전용 자식 창 생성
    // =====================================

    // 자식 창 클래스 등록
    WNDCLASSEXW renderWC = {};

    renderWC.cbSize = sizeof(WNDCLASSEXW);
    renderWC.lpfnWndProc = RenderWndProc;
    renderWC.hInstance = hInstance;
    renderWC.lpszClassName = L"DX12RenderWindow";
    renderWC.hCursor = LoadCursorW(
        nullptr, IDC_ARROW);

    if (!RegisterClassExW(&renderWC))
    {
        MessageBoxW(
            hwnd,
            L"렌더링 창 클래스 등록 실패",
            L"Error",
            MB_OK | MB_ICONERROR
        );

        return 0;
    }

    // 기존 부모 창 안에 자식 창 생성
    HWND renderHwnd = CreateWindowExW(
        0,
        L"DX12RenderWindow",
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0,
        0,
        DX12App::WINDOW_WIDTH,
        DX12App::WINDOW_HEIGHT,
        hwnd,
        nullptr,
        hInstance,
        nullptr
    );

    if (!renderHwnd)
    {
        MessageBoxW(
            hwnd,
            L"DirectX 렌더링 창 생성 실패",
            L"Error",
            MB_OK | MB_ICONERROR
        );

        return 0;
    }

    try
    {
        // 4. DirectX 12 객체 생성
        DX12App app;

        // 5. DirectX 12는 왼쪽 자식 창에 출력
        app.Initialize(renderHwnd);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(&app)
        );


        // ==============================
        // Save / Load 버튼 생성
        // ==============================

        // 오른쪽 도구 패널 위치
        constexpr int BUTTON_X = 1300;
        constexpr int BUTTON_WIDTH = 140;
        constexpr int BUTTON_HEIGHT = 38;

        // 1. Save 버튼
        HWND saveButton = CreateWindowW(
            L"BUTTON",
            L"Save BMP",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            BUTTON_X,
            80,   // Save 버튼 Y 위치
            BUTTON_WIDTH,
            BUTTON_HEIGHT,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_SAVE_BUTTON)
                ),
            hInstance,
            nullptr
        );

        // 2. Load 버튼
        HWND loadButton = CreateWindowW(
            L"BUTTON",
            L"Load BMP",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            BUTTON_X,
            130,  // Load 버튼 Y 위치
            BUTTON_WIDTH,
            BUTTON_HEIGHT,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_LOAD_BUTTON)
                ),
            hInstance,
            nullptr
        );

        // 버튼 생성 성공 여부 확인
        if (!saveButton || !loadButton)
        {
            MessageBoxW(
                hwnd,
                L"Save / Load 버튼 생성 실패",
                L"Error",
                MB_OK | MB_ICONERROR
            );
        }

        // ==============================
        // 저장 / 불러오기 상태 표시
        // ==============================

        HWND statusText = CreateWindowW(
            L"STATIC",
            L"Ready",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            1300, 245,
            150, 45,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_STATUS_TEXT)
                ),
            hInstance,
            nullptr
        );

        if (!statusText)
        {
            throw HRESULT_FROM_WIN32(GetLastError());
        }

        // 현재 선택된 도구 표시
        HWND toolText = CreateWindowW(
            L"STATIC",
            L"Selected: White",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            1300, 325,
            150, 40,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_TOOL_TEXT)
                ),
            hInstance,
            nullptr
        );

        if (!toolText)
            throw HRESULT_FROM_WIN32(GetLastError());

        // 기본 UI 글꼴 적용
        SendMessageW(
            toolText,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                GetStockObject(DEFAULT_GUI_FONT)
                ),
            TRUE
        );


        // ==============================
        // Clear Board 버튼
        // ==============================

        HWND clearButton = CreateWindowW(
            L"BUTTON",
            L"Clear Board",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1300, 180,
            140, 38,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_CLEAR_BUTTON)
                ),
            hInstance,
            nullptr
        );

        if (!clearButton)
        {
            throw HRESULT_FROM_WIN32(GetLastError());
        }


        // Windows 기본 UI 글꼴 적용
        SendMessageW(
            statusText,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                GetStockObject(DEFAULT_GUI_FONT)
                ),
            TRUE
        );

        // ==============================
        // 분필 / 지우개 굵기 조절 UI
        // ==============================

        // 굵기 감소 버튼
        HWND brushMinusButton = CreateWindowW(
            L"BUTTON",
            L"Brush -",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1300, 425,
            65, 38,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_BRUSH_MINUS)
                ),
            hInstance,
            nullptr
        );

        // 굵기 증가 버튼
        HWND brushPlusButton = CreateWindowW(
            L"BUTTON",
            L"Brush +",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1375, 425,
            65, 38,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_BRUSH_PLUS)
                ),
            hInstance,
            nullptr
        );

        // 현재 굵기 표시
        HWND brushSizeText = CreateWindowW(
            L"STATIC",
            L"Radius: 4 px",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            1300, 485,
            150, 35,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_BRUSH_SIZE_TEXT)
                ),
            hInstance,
            nullptr
        );

        if (!brushMinusButton ||
            !brushPlusButton ||
            !brushSizeText)
        {
            throw HRESULT_FROM_WIN32(GetLastError());
        }

        // 상태 텍스트에 기본 UI 글꼴 적용
        SendMessageW(
            brushSizeText,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                GetStockObject(DEFAULT_GUI_FONT)
                ),
            TRUE
        );


        // 6. 창 표시
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        MSG msg = {};

        std::wstring lastToolLabel;
        std::wstring lastBrushLabel;


        // 7. 프로그램 실행 반복
        while (msg.message != WM_QUIT)
        {
            if (PeekMessageW(
                &msg,
                nullptr,
                0,
                0,
                PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            else
            {

                if (!IsIconic(hwnd))
                {
                    // 현재 선택된 도구 이름 가져오기
                    std::wstring toolLabel =
                        std::wstring(L"Selected: ") +
                        app.GetCurrentToolName();

                    // 변경되었을 때만 UI 텍스트 갱신
                    if (toolLabel != lastToolLabel)
                    {
                        SetWindowTextW(
                            toolText,
                            toolLabel.c_str()
                        );

                        lastToolLabel = toolLabel;
                    }

                    // 현재 선택한 도구의 반지름 표시
                    std::wstring brushLabel =
                        L"Radius: " +
                        std::to_wstring(app.GetCurrentBrushRadius()) +
                        L" px";

                    // 이전에 표시한 값과 다를 때만 갱신
                    if (brushLabel != lastBrushLabel)
                    {
                        SetWindowTextW(
                            brushSizeText,
                            brushLabel.c_str()
                        );

                        lastBrushLabel = brushLabel;
                    }

                    // 기존 DirectX 렌더링
                    app.Render();
                }

            }
        }

        return static_cast<int>(msg.wParam);
    }
    catch (HRESULT hr)
    {
        wchar_t errorText[256];

        wsprintfW(
            errorText,
            L"DirectX 12 실행 오류!\nHRESULT: 0x%08X",
            static_cast<unsigned int>(hr));

        MessageBoxW(
            hwnd,
            errorText,
            L"DX12 Error",
            MB_OK | MB_ICONERROR);

        return -1;
    }
}
