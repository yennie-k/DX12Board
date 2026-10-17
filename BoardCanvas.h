
#pragma once

#include <cstdint>
#include <vector>

class BoardCanvas
{
public:
    // 칠판 그림 해상도
    static constexpr int Width = 1024;
    static constexpr int Height = 512;

    BoardCanvas();

    // 분필로 그리기
    void BeginStroke(float u, float v);
    void ContinueStroke(float u, float v);
    void EndStroke();

    // 분필 색상 변경
    void SetChalkColor(std::uint32_t color);

    // 지우개 선택
    void SelectEraser();

    // 현재 도구의 굵기 증가
    void IncreaseBrushSize();

    // 현재 도구의 굵기 감소
    void DecreaseBrushSize();

    // 현재 도구의 반지름 반환
    int GetCurrentBrushRadius() const;

    // 현재 선택된 도구 이름
    const wchar_t* GetCurrentToolName() const;

    // 그림 데이터
    const std::vector<std::uint32_t>& GetPixels() const;

    // 현재 칠판 그림을 BMP 파일로 저장
    bool SaveBMP(const char* filename) const;

    // BMP 파일을 읽어서 칠판 그림 복원
    bool LoadBMP(const char* filename);

    // 텍스처 업데이트 여부
    bool IsDirty() const;
    void ClearDirty();

    // 테스트를 위한 획 개수
    unsigned int GetStrokeCount() const;

    // 칠판 그림 전체 지우기
    void Clear();

private:
    void DrawBrush(int x, int y);
    void DrawLine(int x0, int y0, int x1, int y1);

    int ToPixelX(float u) const;
    int ToPixelY(float v) const;

    std::vector<std::uint32_t> pixels;

    bool isDrawing = false;
    bool dirty = true;

    int previousX = 0;
    int previousY = 0;

    unsigned int strokeCount = 0;

    // 현재 선택한 분필 색상 (기본 흰색)
    std::uint32_t currentColor = 0xFFE8F4F8u;

    // 지우개 모드 여부
    bool eraserMode = false;

    // 분필과 지우개 반지름 개별 저장
    int chalkRadius = 4;
    int eraserRadius = 12;

};
