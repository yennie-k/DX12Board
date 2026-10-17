
#include "BoardCanvas.h"

#include <algorithm>
#include <cmath>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <fstream>


// CPU 메모리의 RGBA8 픽셀 색상
// 0xAABBGGRR 형태
namespace
{
    // 칠판 기본 배경색
    constexpr std::uint32_t BoardColor = 0xFF3B4A21u;
}

// 생성자: 칠판을 초록색으로 초기화
BoardCanvas::BoardCanvas()
    : pixels(Width* Height, BoardColor)
{
}

// UV -> 픽셀 X
int BoardCanvas::ToPixelX(float u) const
{
    int x = static_cast<int>(
        std::lround(u * (Width - 1))
        );

    return std::max(0, std::min(Width - 1, x));
}

// UV -> 픽셀 Y
int BoardCanvas::ToPixelY(float v) const
{
    int y = static_cast<int>(
        std::lround(v * (Height - 1))
        );

    return std::max(0, std::min(Height - 1, y));
}

// 한 위치에 원형 분필 찍기

void BoardCanvas::DrawBrush(int x, int y)
{

    // 현재 선택한 도구에 따라 반지름 결정
    int radius = eraserMode
        ? eraserRadius
        : chalkRadius;

    // 지우개는 칠판 배경색으로 덮어쓰기
    std::uint32_t paintColor = eraserMode
        ? BoardColor
        : currentColor;

    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            // 원형 브러시
            if (dx * dx + dy * dy > radius * radius)
                continue;

            int px = x + dx;
            int py = y + dy;

            // 칠판 범위 검사
            if (px < 0 || px >= Width ||
                py < 0 || py >= Height)
            {
                continue;
            }

            pixels[py * Width + px] = paintColor;
        }
    }

    dirty = true;
}


// 이전 위치부터 현재 위치까지 연결
void BoardCanvas::DrawLine(
    int x0, int y0, int x1, int y1)
{
    int dx = x1 - x0;
    int dy = y1 - y0;

    int steps = std::max(
        std::abs(dx),
        std::abs(dy)
    );

    if (steps == 0)
    {
        DrawBrush(x0, y0);
        return;
    }

    for (int i = 0; i <= steps; ++i)
    {
        float t = static_cast<float>(i) / steps;

        int x = static_cast<int>(
            std::lround(x0 + dx * t)
            );

        int y = static_cast<int>(
            std::lround(y0 + dy * t)
            );

        DrawBrush(x, y);
    }
}

// 그리기 시작
void BoardCanvas::BeginStroke(float u, float v)
{
    previousX = ToPixelX(u);
    previousY = ToPixelY(v);

    isDrawing = true;
    ++strokeCount;

    DrawBrush(previousX, previousY);
}

// 드래그하면서 그리기
void BoardCanvas::ContinueStroke(float u, float v)
{
    if (!isDrawing)
        return;

    int x = ToPixelX(u);
    int y = ToPixelY(v);

    DrawLine(previousX, previousY, x, y);

    previousX = x;
    previousY = y;
}

// 그리기 종료
void BoardCanvas::EndStroke()
{
    isDrawing = false;
}

const std::vector<std::uint32_t>&
BoardCanvas::GetPixels() const
{
    return pixels;
}

bool BoardCanvas::IsDirty() const
{
    return dirty;
}

void BoardCanvas::ClearDirty()
{
    dirty = false;
}

unsigned int BoardCanvas::GetStrokeCount() const
{
    return strokeCount;
}

// 분필 색상 선택
void BoardCanvas::SetChalkColor(std::uint32_t color)
{
    currentColor = color;

    // 분필을 선택하면 지우개 해제
    eraserMode = false;
}

// 지우개 선택
void BoardCanvas::SelectEraser()
{
    eraserMode = true;
}

bool BoardCanvas::SaveBMP(const char* filename) const
{
    // 1. BMP 파일 열기
    std::ofstream file(
        filename,
        std::ios::binary | std::ios::trunc
    );

    if (!file)
        return false;

    // 2. BMP 파일 헤더
    BITMAPFILEHEADER fileHeader = {};

    fileHeader.bfType = 0x4D42; // BM
    fileHeader.bfOffBits =
        sizeof(BITMAPFILEHEADER) +
        sizeof(BITMAPINFOHEADER);

    const DWORD imageSize =
        Width * Height * 4;

    fileHeader.bfSize =
        fileHeader.bfOffBits + imageSize;

    // 3. BMP 이미지 정보
    BITMAPINFOHEADER infoHeader = {};

    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = Width;
    infoHeader.biHeight = Height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 32;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = imageSize;

    // 4. 파일에 헤더 작성
    file.write(
        reinterpret_cast<const char*>(&fileHeader),
        sizeof(fileHeader)
    );

    file.write(
        reinterpret_cast<const char*>(&infoHeader),
        sizeof(infoHeader)
    );

    // 5. 그림 픽셀 저장
    // BMP는 아래쪽 행부터 저장
    std::vector<std::uint32_t> row(Width);

    for (int y = Height - 1; y >= 0; --y)
    {
        for (int x = 0; x < Width; ++x)
        {
            std::uint32_t pixel =
                pixels[y * Width + x];

            // BoardCanvas RGBA -> BMP BGRA
            std::uint32_t r = pixel & 0xFF;
            std::uint32_t g = (pixel >> 8) & 0xFF;
            std::uint32_t b = (pixel >> 16) & 0xFF;

            row[x] =
                0xFF000000u |
                (r << 16) |
                (g << 8) |
                b;
        }

        file.write(
            reinterpret_cast<const char*>(row.data()),
            Width * 4
        );

        if (!file)
            return false;
    }

    file.close();
    return !file.fail();
}

bool BoardCanvas::LoadBMP(const char* filename)
{
    // 1. BMP 파일 열기
    std::ifstream file(
        filename,
        std::ios::binary
    );

    if (!file)
        return false;

    // 2. BMP 헤더 읽기
    BITMAPFILEHEADER fileHeader = {};
    BITMAPINFOHEADER infoHeader = {};

    file.read(
        reinterpret_cast<char*>(&fileHeader),
        sizeof(fileHeader)
    );

    file.read(
        reinterpret_cast<char*>(&infoHeader),
        sizeof(infoHeader)
    );

    if (!file)
        return false;

    // 3. 파일 형식 확인
    if (fileHeader.bfType != 0x4D42 ||
        infoHeader.biSize != sizeof(BITMAPINFOHEADER) ||
        infoHeader.biWidth != Width ||
        infoHeader.biHeight != Height ||
        infoHeader.biPlanes != 1 ||
        infoHeader.biBitCount != 32 ||
        infoHeader.biCompression != BI_RGB)
    {
        return false;
    }

    // 픽셀 데이터 시작 위치 확인
    const DWORD minimumOffset =
        sizeof(BITMAPFILEHEADER) +
        sizeof(BITMAPINFOHEADER);

    if (fileHeader.bfOffBits < minimumOffset)
        return false;

    file.seekg(
        fileHeader.bfOffBits,
        std::ios::beg
    );

    if (!file)
        return false;

    // 4. 임시 픽셀 배열 생성
    std::vector<std::uint32_t> loadedPixels(
        Width * Height
    );

    std::vector<std::uint32_t> row(Width);

    // 5. BMP 픽셀 읽기
    for (int y = Height - 1; y >= 0; --y)
    {
        file.read(
            reinterpret_cast<char*>(row.data()),
            Width * 4
        );

        if (!file)
            return false;

        for (int x = 0; x < Width; ++x)
        {
            std::uint32_t pixel = row[x];

            // BMP BGRA -> BoardCanvas RGBA
            std::uint32_t b = pixel & 0xFF;
            std::uint32_t g = (pixel >> 8) & 0xFF;
            std::uint32_t r = (pixel >> 16) & 0xFF;

            loadedPixels[y * Width + x] =
                0xFF000000u |
                (b << 16) |
                (g << 8) |
                r;
        }
    }

    // 6. 정상적으로 읽은 후 실제 칠판 교체
    pixels.swap(loadedPixels);

    // 7. 다시 그림을 그릴 수 있도록 상태 설정
    isDrawing = false;
    strokeCount = 0;

    // GPU 텍스처를 다시 업데이트하도록 표시
    dirty = true;

    return true;
}

void BoardCanvas::Clear()
{
    // 그리기 중이라면 종료
    EndStroke();

    // 모든 픽셀을 칠판 배경색으로 변경
    std::fill(
        pixels.begin(),
        pixels.end(),
        BoardColor
    );

    // 획 개수 초기화
    strokeCount = 0;

    // GPU 텍스처 업데이트 요청
    dirty = true;
}

const wchar_t* BoardCanvas::GetCurrentToolName() const
{
    if (eraserMode)
        return L"Eraser";

    switch (currentColor)
    {
    case 0xFFE8F4F8u:
        return L"White";

    case 0xFF8AE5FFu:
        return L"Yellow";

    case 0xFF61A2F4u:
        return L"Orange";

    case 0xFFB8A9F5u:
        return L"Pink";

    case 0xFFE5D6A9u:
        return L"Sky Blue";

    case 0xFFC7E4B7u:
        return L"Green";

    

    default:
        return L"Custom";
    }
}

// 현재 도구의 굵기 증가
void BoardCanvas::IncreaseBrushSize()
{
    if (eraserMode)
    {
        // 지우개: 최대 반지름 24
        eraserRadius = std::min(
            eraserRadius + 2, 24
        );
    }
    else
    {
        // 분필: 최대 반지름 10
        chalkRadius = std::min(
            chalkRadius + 1, 10
        );
    }
}

// 현재 도구의 굵기 감소
void BoardCanvas::DecreaseBrushSize()
{
    if (eraserMode)
    {
        // 지우개: 최소 반지름 6
        eraserRadius = std::max(
            eraserRadius - 2, 6
        );
    }
    else
    {
        // 분필: 최소 반지름 2
        chalkRadius = std::max(
            chalkRadius - 1, 2
        );
    }
}

// 현재 선택한 도구의 반지름
int BoardCanvas::GetCurrentBrushRadius() const
{
    return eraserMode
        ? eraserRadius
        : chalkRadius;
}
