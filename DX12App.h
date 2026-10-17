
#pragma once

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <DirectXMath.h>
#include "BoardCanvas.h"

// DirectX 12 프로그램 클래스
class DX12App
{
public:
    // 창 크기
    static constexpr UINT WINDOW_WIDTH = 1280;
    static constexpr UINT WINDOW_HEIGHT = 720;

    DX12App() = default;
    ~DX12App();

    // DirectX 초기화
    void Initialize(HWND hwnd);

    // 화면 그리기
    void Render();

    // 우클릭 드래그로 카메라 회전
    void RotateCamera(int deltaX, int deltaY);

    // 카메라를 기본 정면으로 초기화
    void ResetCamera();

    // 마우스가 칠판 위에 있는지 검사
    // 성공하면 칠판 표면의 U, V 좌표 반환
    bool PickBoard(
        int mouseX,
        int mouseY,
        float& outU,
        float& outV
    ) const;

    // 마우스로 분필 그리기
    bool BeginStroke(int mouseX, int mouseY);
    void ContinueStroke(int mouseX, int mouseY);
    void EndStroke();

    // 현재 분필 색상 설정
    void SetChalkColor(std::uint32_t color);

    // 지우개 모드 선택
    void SelectEraser();

    // 분필 또는 지우개 굵기 조절
    void IncreaseBrushSize();
    void DecreaseBrushSize();

    // 현재 도구 반지름
    int GetCurrentBrushRadius() const;


    // 칠판 그림 저장 및 불러오기
    bool SaveBoard() const;
    bool LoadBoard();

    // 칠판 전체 지우기
    void ClearBoard();

    const wchar_t* GetCurrentToolName() const;

    // 테스트용
    unsigned int GetStrokeCount() const;


private:
    // 화면 버퍼 개수
    static constexpr UINT FRAME_COUNT = 2;

    // 칠판의 그림 픽셀 데이터
    BoardCanvas boardCanvas;

    // GPU 작업 완료 대기
    void WaitForGPU();

    // 3D 칠판 렌더링에 필요한 객체 생성
    void CreateBoardResources();

    // 변경된 카메라 행렬을 GPU에 반영
    void UpdateCameraMatrices();

    // CPU 그림 데이터를 GPU 텍스처로 업데이트
    void UpdateBoardTexture();

    // 깊이 버퍼 생성
    void CreateDepthBuffer();

    // DirectX 기본 객체
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;

    // 렌더 타겟
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT];

    // 깊이 버퍼 관련 객체
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil;

    // 명령 관련 객체
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

    // GPU 동기화
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;

    // 상태 변수
    UINT frameIndex = 0;
    UINT rtvDescriptorSize = 0;
    UINT64 fenceValue = 0;


    // 셰이더에서 사용하는 루트 서명
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;

    // 그래픽 렌더링 파이프라인
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

    // 칠판 사각형의 정점 데이터
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

    // 3D 변환 행렬을 GPU로 전달하는 버퍼
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer;

    // 피킹에 사용할 3D 변환 행렬
    DirectX::XMFLOAT4X4 boardWorld = {};
    DirectX::XMFLOAT4X4 cameraView = {};
    DirectX::XMFLOAT4X4 cameraProjection = {};

    // 칠판 전용 그래픽 파이프라인
    Microsoft::WRL::ComPtr<ID3D12PipelineState>
        boardPipelineState;

    // 칠판 그림을 저장하는 GPU 텍스처
    Microsoft::WRL::ComPtr<ID3D12Resource>
        boardTexture;

    // 픽셀 데이터를 GPU로 복사하기 위한 버퍼
    Microsoft::WRL::ComPtr<ID3D12Resource>
        boardUploadBuffer;

    // 셰이더가 텍스처를 읽기 위한 SRV Heap
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
        boardSrvHeap;

    // GPU 텍스처 복사 시 사용할 메모리 배치 정보
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT
        boardFootprint = {};

    // 카메라 회전 각도 (단위: 도)
    float cameraYaw = 0.0f;
    float cameraPitch = 0.0f;

    // 카메라와 칠판 사이의 거리
    float cameraDistance = 8.5f;

    // 3D 색상 팔레트 정점 버퍼
    Microsoft::WRL::ComPtr<ID3D12Resource>
        paletteVertexBuffer;

    D3D12_VERTEX_BUFFER_VIEW paletteVertexBufferView = {};

    // 색상 팔레트 마우스 피킹
    bool PickPalette(
        int mouseX,
        int mouseY,
        int& outIndex
    ) const;

};
