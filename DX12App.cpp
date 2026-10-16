
#include "DX12App.h"

#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <cstring>
#include <cmath>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// DirectX 실행 결과 검사
namespace
{
    void CheckHR(HRESULT hr)
    {
        if (FAILED(hr))
            throw hr;
    }
}

// 소멸자
DX12App::~DX12App()
{
    if (fenceEvent)
    {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }
}

// DirectX 12 초기화
void DX12App::Initialize(HWND hwnd)
{
    // 1. DXGI Factory 생성
    CheckHR(CreateDXGIFactory2(
        0, IID_PPV_ARGS(&factory)));

    // 2. DirectX 12 Device 생성
    CheckHR(D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device)));

    // 3. Command Queue 생성
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    CheckHR(device->CreateCommandQueue(
        &queueDesc,
        IID_PPV_ARGS(&commandQueue)));

    // 4. SwapChain 설정
    DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
    swapDesc.Width = WINDOW_WIDTH;
    swapDesc.Height = WINDOW_HEIGHT;
    swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = FRAME_COUNT;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;

    CheckHR(factory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        hwnd,
        &swapDesc,
        nullptr,
        nullptr,
        &swapChain1));

    CheckHR(swapChain1.As(&swapChain));

    frameIndex = swapChain->GetCurrentBackBufferIndex();

    // 5. RTV Descriptor Heap 생성
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.NumDescriptors = FRAME_COUNT;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    CheckHR(device->CreateDescriptorHeap(
        &rtvDesc,
        IID_PPV_ARGS(&rtvHeap)));

    rtvDescriptorSize =
        device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 6. Render Target 생성
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        CheckHR(swapChain->GetBuffer(
            i, IID_PPV_ARGS(&renderTargets[i])));

        device->CreateRenderTargetView(
            renderTargets[i].Get(),
            nullptr,
            rtvHandle);

        rtvHandle.ptr += rtvDescriptorSize;
    }

    // 7. Command Allocator 생성
    CheckHR(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator)));

    // 8. Command List 생성
    CheckHR(device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList)));

    CheckHR(commandList->Close());

    // 9. Fence 생성
    CheckHR(device->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence)));

    fenceEvent = CreateEventW(
        nullptr,
        FALSE,
        FALSE,
        nullptr);

    if (!fenceEvent)
        throw HRESULT_FROM_WIN32(GetLastError());


    // 깊이 버퍼 생성
    CreateDepthBuffer();

    // 칠판 리소스 생성
    CreateBoardResources();
}

// GPU 작업 완료 대기
void DX12App::WaitForGPU()
{
    const UINT64 value = ++fenceValue;

    CheckHR(commandQueue->Signal(
        fence.Get(), value));

    if (fence->GetCompletedValue() < value)
    {
        CheckHR(fence->SetEventOnCompletion(
            value, fenceEvent));

        WaitForSingleObject(
            fenceEvent,
            INFINITE);
    }
}

// 화면 렌더링
void DX12App::Render()
{
    CheckHR(commandAllocator->Reset());

    CheckHR(commandList->Reset(
        commandAllocator.Get(),
        nullptr));

    // 그림 데이터가 변경되었을 때 GPU 텍스처 업데이트
    UpdateBoardTexture();

    // 화면 버퍼를 렌더링 가능한 상태로 변경
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

    barrier.Transition.pResource =
        renderTargets[frameIndex].Get();

    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_PRESENT;

    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_RENDER_TARGET;

    barrier.Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);

    // RTV 위치 계산
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        rtvHeap->GetCPUDescriptorHandleForHeapStart();

    rtvHandle.ptr +=
        static_cast<SIZE_T>(frameIndex) *
        rtvDescriptorSize;

    // 3D 칠판 뒤에 보이는 배경색
    const float clearColor[4] =
    {
        0.16f,
        0.17f,
        0.19f,
        1.0f
    };


    // 깊이 버퍼 핸들 가져오기
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
        dsvHeap->GetCPUDescriptorHandleForHeapStart();

    // 색상 버퍼와 깊이 버퍼를 함께 설정
    commandList->OMSetRenderTargets(
        1,
        &rtvHandle,
        FALSE,
        &dsvHandle
    );


    // 배경색 출력
    commandList->ClearRenderTargetView(
        rtvHandle,
        clearColor,
        0,
        nullptr);

    // 매 프레임 깊이 버퍼 초기화
    commandList->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr
    );


    // ==============================
    // 3D 칠판 앞면 그리기
    // ==============================

    // 화면에 그릴 영역
    D3D12_VIEWPORT viewport = {
        0.0f,
        0.0f,
        static_cast<float>(WINDOW_WIDTH),
        static_cast<float>(WINDOW_HEIGHT),
        0.0f,
        1.0f
    };

    D3D12_RECT scissorRect = {
        0,
        0,
        static_cast<LONG>(WINDOW_WIDTH),
        static_cast<LONG>(WINDOW_HEIGHT)
    };

    commandList->RSSetViewports(
        1, &viewport);

    commandList->RSSetScissorRects(
        1, &scissorRect);

    // 그래픽 파이프라인 설정
    commandList->SetPipelineState(
        pipelineState.Get());

    commandList->SetGraphicsRootSignature(
        rootSignature.Get());

    // 3D 변환 행렬 전달
    commandList->SetGraphicsRootConstantBufferView(
        0,
        constantBuffer->GetGPUVirtualAddress());

    // 삼각형을 이용한 렌더링
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 정점 버퍼 연결
    commandList->IASetVertexBuffers(
        0,
        1,
        &vertexBufferView);


    // 1. 나무 프레임 및 측면 36개 정점 그리기
    commandList->DrawInstanced(
        36,
        1,
        0,
        0
    );

    // ==============================
    // 3D 색상 팔레트 출력
    // ==============================

    // 나무 프레임과 같은 색상 셰이더 사용
    commandList->SetPipelineState(
        pipelineState.Get()
    );

    // 팔레트 정점 버퍼로 변경
    commandList->IASetVertexBuffers(
        0,
        1,
        &paletteVertexBufferView
    );

    // 받침대 + 일곱 개 선택 영역 출력
    commandList->DrawInstanced(
        48,
        1,
        0,
        0
    );

    // 중요: 다시 원래 칠판 정점 버퍼 연결
    commandList->IASetVertexBuffers(
        0,
        1,
        &vertexBufferView
    );

    // 2. 칠판 표면 전용 파이프라인 설정
    commandList->SetPipelineState(
        boardPipelineState.Get()
    );

    // 3. 텍스처 Descriptor Heap 연결
    ID3D12DescriptorHeap* heaps[] =
    {
        boardSrvHeap.Get()
    };

    commandList->SetDescriptorHeaps(
        1,
        heaps
    );

    // 4. 텍스처를 루트 파라미터 1번에 연결
    commandList->SetGraphicsRootDescriptorTable(
        1,
        boardSrvHeap->GetGPUDescriptorHandleForHeapStart()
    );

    // 5. 칠판 앞면 6개 정점에 텍스처 출력
    commandList->DrawInstanced(
        6,
        1,
        36,
        0
    );


    // 화면 표시 상태로 변경
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_RENDER_TARGET;

    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_PRESENT;

    commandList->ResourceBarrier(1, &barrier);

    CheckHR(commandList->Close());

    // GPU에 렌더링 명령 전달
    ID3D12CommandList* lists[] =
    {
        commandList.Get()
    };

    commandQueue->ExecuteCommandLists(1, lists);

    // 화면 출력
    CheckHR(swapChain->Present(1, 0));

    // 현재 단계에서는 매 프레임 GPU 작업을 기다림
    WaitForGPU();

    frameIndex =
        swapChain->GetCurrentBackBufferIndex();
}

void DX12App::CreateBoardResources()
{
    using namespace DirectX;

    // ==============================
    // 1. HLSL 셰이더 컴파일
    // ==============================

    Microsoft::WRL::ComPtr<ID3DBlob> vs;
    Microsoft::WRL::ComPtr<ID3DBlob> ps;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;

    HRESULT hr = D3DCompileFromFile(
        L"Shader.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "VSMain",
        "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        &vs,
        &errors
    );

    if (FAILED(hr))
    {
        if (errors)
        {
            MessageBoxA(
                nullptr,
                (const char*)errors->GetBufferPointer(),
                "Vertex Shader Error",
                MB_OK | MB_ICONERROR
            );
        }
        CheckHR(hr);
    }

    errors.Reset();

    hr = D3DCompileFromFile(
        L"Shader.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "PSMain",
        "ps_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        &ps,
        &errors
    );

    // 칠판 표면 전용 픽셀 셰이더 컴파일
    Microsoft::WRL::ComPtr<ID3DBlob> boardPS;

    errors.Reset();

    hr = D3DCompileFromFile(
        L"Shader.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "PSBoard",
        "ps_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        &boardPS,
        &errors
    );

    if (FAILED(hr))
    {
        if (errors)
        {
            MessageBoxA(
                nullptr,
                (const char*)errors->GetBufferPointer(),
                "Board Shader Error",
                MB_OK | MB_ICONERROR
            );
        }

        CheckHR(hr);
    }

    if (FAILED(hr))
    {
        if (errors)
        {
            MessageBoxA(
                nullptr,
                (const char*)errors->GetBufferPointer(),
                "Pixel Shader Error",
                MB_OK | MB_ICONERROR
            );
        }
        CheckHR(hr);
    }

    // ==============================
    // 2. 루트 서명 생성
    // ==============================


    // 첫 번째 루트 파라미터: 변환 행렬(CBV)
    D3D12_ROOT_PARAMETER rootParams[2] = {};

    rootParams[0].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_CBV;

    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;

    // 두 번째 루트 파라미터: 그림 텍스처(SRV)
    D3D12_DESCRIPTOR_RANGE srvRange = {};

    srvRange.RangeType =
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParams[1].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;

    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges =
        &srvRange;

    rootParams[1].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;

    // 텍스처의 픽셀을 읽을 때 사용할 샘플러
    D3D12_STATIC_SAMPLER_DESC sampler = {};

    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;

    sampler.ComparisonFunc =
        D3D12_COMPARISON_FUNC_ALWAYS;

    sampler.MaxAnisotropy = 1;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;

    // 루트 서명 생성 정보
    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};

    rootDesc.NumParameters = 2;
    rootDesc.pParameters = rootParams;

    rootDesc.NumStaticSamplers = 1;
    rootDesc.pStaticSamplers = &sampler;

    rootDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


    Microsoft::WRL::ComPtr<ID3DBlob> serializedRoot;
    errors.Reset();

    hr = D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRoot,
        &errors
    );

    if (FAILED(hr))
    {
        if (errors)
            OutputDebugStringA(
                (const char*)errors->GetBufferPointer());

        CheckHR(hr);
    }

    CheckHR(device->CreateRootSignature(
        0,
        serializedRoot->GetBufferPointer(),
        serializedRoot->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    ));

    // ==============================
    // 3. 정점 데이터 구조 설정
    // ==============================

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        {
            "POSITION", 0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },
        {
            "COLOR", 0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0, 12,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        }
    };

    // ==============================
    // 4. 그래픽 파이프라인 상태 생성
    // ==============================

    D3D12_RASTERIZER_DESC rasterizer = {};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.DepthClipEnable = TRUE;

    D3D12_RENDER_TARGET_BLEND_DESC targetBlend = {};
    targetBlend.BlendEnable = FALSE;
    targetBlend.LogicOpEnable = FALSE;
    targetBlend.SrcBlend = D3D12_BLEND_ONE;
    targetBlend.DestBlend = D3D12_BLEND_ZERO;
    targetBlend.BlendOp = D3D12_BLEND_OP_ADD;
    targetBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
    targetBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
    targetBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    targetBlend.LogicOp = D3D12_LOGIC_OP_NOOP;
    targetBlend.RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0] = targetBlend;

    D3D12_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthDesc.StencilEnable = FALSE;
    depthDesc.StencilReadMask =
        D3D12_DEFAULT_STENCIL_READ_MASK;
    depthDesc.StencilWriteMask =
        D3D12_DEFAULT_STENCIL_WRITE_MASK;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

    psoDesc.InputLayout = { inputLayout, 2 };
    psoDesc.pRootSignature = rootSignature.Get();

    psoDesc.VS = {
        vs->GetBufferPointer(),
        vs->GetBufferSize()
    };

    psoDesc.PS = {
        ps->GetBufferPointer(),
        ps->GetBufferSize()
    };

    psoDesc.RasterizerState = rasterizer;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState = depthDesc;
    psoDesc.SampleMask = 0xFFFFFFFF;
    psoDesc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.RTVFormats[0] =
        DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    CheckHR(device->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&pipelineState)
    ));

    // 칠판 표면을 그릴 때는 PSBoard 사용
    psoDesc.PS = {
        boardPS->GetBufferPointer(),
        boardPS->GetBufferSize()
    };

    CheckHR(device->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&boardPipelineState)
    ));


    // ==============================
    // 5. 칠판 사각형 정점 생성
    // ==============================

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT3 color;
    };



    // 칠판과 나무 프레임 색상
    const XMFLOAT3 green = { 0.13f, 0.29f, 0.23f };
    const XMFLOAT3 wood = { 0.47f, 0.29f, 0.15f };
    const XMFLOAT3 woodTop = { 0.58f, 0.37f, 0.20f };
    const XMFLOAT3 woodSide = { 0.33f, 0.20f, 0.10f };
    const XMFLOAT3 woodBottom = { 0.28f, 0.17f, 0.09f };
    const XMFLOAT3 woodBack = { 0.37f, 0.23f, 0.12f };

    // 앞면 Z = 0.05, 뒷면 Z = 0.45
    // 칠판 표면은 Z = 0.0으로 앞쪽에 배치

    Vertex vertices[] =
    {
        // 1. 나무 프레임 앞면
        { {-3.3f, -1.95f, 0.05f}, wood },
        { {-3.3f,  1.95f, 0.05f}, wood },
        { { 3.3f,  1.95f, 0.05f}, wood },

        { {-3.3f, -1.95f, 0.05f}, wood },
        { { 3.3f,  1.95f, 0.05f}, wood },
        { { 3.3f, -1.95f, 0.05f}, wood },

        // 2. 나무 프레임 뒷면
        { {-3.3f, -1.95f, 0.45f}, woodBack },
        { { 3.3f,  1.95f, 0.45f}, woodBack },
        { {-3.3f,  1.95f, 0.45f}, woodBack },

        { {-3.3f, -1.95f, 0.45f}, woodBack },
        { { 3.3f, -1.95f, 0.45f}, woodBack },
        { { 3.3f,  1.95f, 0.45f}, woodBack },

        // 3. 위쪽 면
        { {-3.3f, 1.95f, 0.05f}, woodTop },
        { {-3.3f, 1.95f, 0.45f}, woodTop },
        { { 3.3f, 1.95f, 0.45f}, woodTop },

        { {-3.3f, 1.95f, 0.05f}, woodTop },
        { { 3.3f, 1.95f, 0.45f}, woodTop },
        { { 3.3f, 1.95f, 0.05f}, woodTop },

        // 4. 아래쪽 면
        { {-3.3f, -1.95f, 0.05f}, woodBottom },
        { { 3.3f, -1.95f, 0.45f}, woodBottom },
        { {-3.3f, -1.95f, 0.45f}, woodBottom },

        { {-3.3f, -1.95f, 0.05f}, woodBottom },
        { { 3.3f, -1.95f, 0.05f}, woodBottom },
        { { 3.3f, -1.95f, 0.45f}, woodBottom },

        // 5. 왼쪽 면
        { {-3.3f, -1.95f, 0.05f}, woodSide },
        { {-3.3f, -1.95f, 0.45f}, woodSide },
        { {-3.3f,  1.95f, 0.45f}, woodSide },

        { {-3.3f, -1.95f, 0.05f}, woodSide },
        { {-3.3f,  1.95f, 0.45f}, woodSide },
        { {-3.3f,  1.95f, 0.05f}, woodSide },

        // 6. 오른쪽 면
        { {3.3f, -1.95f, 0.05f}, woodSide },
        { {3.3f,  1.95f, 0.45f}, woodSide },
        { {3.3f, -1.95f, 0.45f}, woodSide },

        { {3.3f, -1.95f, 0.05f}, woodSide },
        { {3.3f,  1.95f, 0.05f}, woodSide },
        { {3.3f,  1.95f, 0.45f}, woodSide },

        // 7. 실제 초록색 칠판 표면
        { {-3.1f, -1.7f, 0.0f}, green },
        { {-3.1f,  1.7f, 0.0f}, green },
        { { 3.1f,  1.7f, 0.0f}, green },

        { {-3.1f, -1.7f, 0.0f}, green },
        { { 3.1f,  1.7f, 0.0f}, green },
        { { 3.1f, -1.7f, 0.0f}, green }
    };



    // GPU에서 읽을 수 있는 업로드 힙
    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(vertices);
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    CheckHR(device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer)
    ));

    // 정점 데이터를 버퍼에 복사
    void* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };

    CheckHR(vertexBuffer->Map(
        0, &readRange, &mappedData));

    std::memcpy(
        mappedData,
        vertices,
        sizeof(vertices)
    );

    vertexBuffer->Unmap(0, nullptr);

    vertexBufferView.BufferLocation =
        vertexBuffer->GetGPUVirtualAddress();

    vertexBufferView.StrideInBytes = sizeof(Vertex);
    vertexBufferView.SizeInBytes = sizeof(vertices);

    // ==============================
    // 3D 분필 색상 팔레트
    // ==============================

    const XMFLOAT3 paletteColors[7] =
    {
        {0.973f, 0.957f, 0.910f}, // 흰색
        {1.000f, 0.898f, 0.541f}, // 노란색
        {0.961f, 0.663f, 0.722f}, // 분홍색
        {0.663f, 0.839f, 0.898f}, // 하늘색
        {0.718f, 0.894f, 0.780f}, // 연두색
        {0.957f, 0.635f, 0.380f}, // 주황색
        {0.650f, 0.650f, 0.650f}  // 지우개
    };

    // 받침대 6개 정점 + 색상 7개 * 6개 정점
    Vertex paletteVertices[48] = {};

    // 사각형 두 삼각형을 생성하는 함수
    auto AddQuad = [&](
        int start,
        float left,
        float right,
        float bottom,
        float top,
        float z,
        const XMFLOAT3& color)
        {
            paletteVertices[start + 0] =
            { {left, bottom, z}, color };

            paletteVertices[start + 1] =
            { {left, top, z}, color };

            paletteVertices[start + 2] =
            { {right, top, z}, color };

            paletteVertices[start + 3] =
            { {left, bottom, z}, color };

            paletteVertices[start + 4] =
            { {right, top, z}, color };

            paletteVertices[start + 5] =
            { {right, bottom, z}, color };
        };

    // 갈색 팔레트 받침대
    AddQuad(
        0,
        -2.9f, 2.9f,
        -2.82f, -2.06f,
        0.10f,
        { 0.38f, 0.22f, 0.12f }
    );

    // 일곱 가지 선택 영역
    for (int i = 0; i < 7; ++i)
    {
        float centerX = -2.4f + i * 0.8f;

        AddQuad(
            6 + i * 6,
            centerX - 0.30f,
            centerX + 0.30f,
            -2.69f,
            -2.18f,
            -0.08f,
            paletteColors[i]
        );
    }

    // 정점 버퍼 생성
    D3D12_RESOURCE_DESC paletteBufferDesc =
        bufferDesc;

    paletteBufferDesc.Width =
        sizeof(paletteVertices);

    CheckHR(device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &paletteBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&paletteVertexBuffer)
    ));

    void* paletteMapped = nullptr;
    D3D12_RANGE paletteReadRange = { 0, 0 };

    CheckHR(paletteVertexBuffer->Map(
        0,
        &paletteReadRange,
        &paletteMapped
    ));

    std::memcpy(
        paletteMapped,
        paletteVertices,
        sizeof(paletteVertices)
    );

    paletteVertexBuffer->Unmap(0, nullptr);

    paletteVertexBufferView.BufferLocation =
        paletteVertexBuffer->GetGPUVirtualAddress();

    paletteVertexBufferView.StrideInBytes =
        sizeof(Vertex);

    paletteVertexBufferView.SizeInBytes =
        sizeof(paletteVertices);

    // ==============================
    // 6. 3D 변환 행렬 준비
    // ==============================

    // 칠판 자체는 회전시키지 않고 정면에 고정
    XMMATRIX world = XMMatrixIdentity();


    // 칠판 정면을 바라보는 기본 카메라
    XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(0.0f, 0.0f, -cameraDistance, 1.0f),
        XMVectorZero(),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );


    XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(45.0f),
        static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT,
        0.1f,
        100.0f
    );

    // 렌더링에 사용한 행렬을 피킹에서도 사용
    XMStoreFloat4x4(&boardWorld, world);
    XMStoreFloat4x4(&cameraView, view);
    XMStoreFloat4x4(&cameraProjection, projection);


    XMFLOAT4X4 wvp;

    XMStoreFloat4x4(
        &wvp,
        XMMatrixTranspose(world * view * projection)
    );

    // Constant Buffer는 256바이트 정렬
    bufferDesc.Width = 256;

    CheckHR(device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constantBuffer)
    ));

    mappedData = nullptr;

    CheckHR(constantBuffer->Map(
        0, &readRange, &mappedData));

    std::memcpy(
        mappedData,
        &wvp,
        sizeof(wvp)
    );

    constantBuffer->Unmap(0, nullptr);

    // ==============================
    // 7. 칠판 그림 텍스처 생성
    // ==============================

    D3D12_RESOURCE_DESC textureDesc = {};

    textureDesc.Dimension =
        D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    textureDesc.Width = BoardCanvas::Width;
    textureDesc.Height = BoardCanvas::Height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;

    textureDesc.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout =
        D3D12_TEXTURE_LAYOUT_UNKNOWN;

    // GPU 전용 메모리
    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    CheckHR(device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&boardTexture)
    ));

    // ==============================
    // 8. 텍스처 업로드 버퍼 생성
    // ==============================

    UINT64 uploadSize = 0;

    device->GetCopyableFootprints(
        &textureDesc,
        0,
        1,
        0,
        &boardFootprint,
        nullptr,
        nullptr,
        &uploadSize
    );

    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension =
        D3D12_RESOURCE_DIMENSION_BUFFER;

    uploadDesc.Width = uploadSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;

    uploadDesc.Layout =
        D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    CheckHR(device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&boardUploadBuffer)
    ));

    // ==============================
    // 9. Shader Resource View 생성
    // ==============================

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};

    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type =
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    srvHeapDesc.Flags =
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    CheckHR(device->CreateDescriptorHeap(
        &srvHeapDesc,
        IID_PPV_ARGS(&boardSrvHeap)
    ));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

    srvDesc.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    srvDesc.ViewDimension =
        D3D12_SRV_DIMENSION_TEXTURE2D;

    srvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    srvDesc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(
        boardTexture.Get(),
        &srvDesc,
        boardSrvHeap->GetCPUDescriptorHandleForHeapStart()
    );

}

void DX12App::CreateDepthBuffer()
{
    // 1. DSV Heap 생성
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

    CheckHR(device->CreateDescriptorHeap(
        &dsvHeapDesc,
        IID_PPV_ARGS(&dsvHeap)
    ));

    // 2. 깊이 텍스처 설정
    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = WINDOW_WIDTH;
    depthDesc.Height = WINDOW_HEIGHT;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    // 3. 깊이 버퍼 초기값
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    // 4. GPU 메모리 설정
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    // 5. 깊이 버퍼 생성
    CheckHR(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&depthStencil)
    ));

    // 6. Depth Stencil View 생성
    device->CreateDepthStencilView(
        depthStencil.Get(),
        nullptr,
        dsvHeap->GetCPUDescriptorHandleForHeapStart()
    );
}


#include <cmath>

bool DX12App::PickBoard(
    int mouseX,
    int mouseY,
    float& outU,
    float& outV
) const
{
    using namespace DirectX;

    // 1. 창 내부 좌표인지 확인
    if (mouseX < 0 || mouseY < 0 ||
        mouseX >= static_cast<int>(WINDOW_WIDTH) ||
        mouseY >= static_cast<int>(WINDOW_HEIGHT))
    {
        return false;
    }

    // 2. 기존에 저장한 변환 행렬 불러오기
    XMMATRIX world = XMLoadFloat4x4(&boardWorld);
    XMMATRIX view = XMLoadFloat4x4(&cameraView);
    XMMATRIX projection =
        XMLoadFloat4x4(&cameraProjection);

    // 3. 화면의 마우스 좌표
    XMVECTOR screenNear = XMVectorSet(
        static_cast<float>(mouseX),
        static_cast<float>(mouseY),
        0.0f,
        1.0f
    );

    XMVECTOR screenFar = XMVectorSet(
        static_cast<float>(mouseX),
        static_cast<float>(mouseY),
        1.0f,
        1.0f
    );

    // 4. 화면 좌표를 칠판 로컬 좌표로 역변환
    XMVECTOR nearPoint = XMVector3Unproject(
        screenNear,
        0.0f, 0.0f,
        static_cast<float>(WINDOW_WIDTH),
        static_cast<float>(WINDOW_HEIGHT),
        0.0f, 1.0f,
        projection, view, world
    );

    XMVECTOR farPoint = XMVector3Unproject(
        screenFar,
        0.0f, 0.0f,
        static_cast<float>(WINDOW_WIDTH),
        static_cast<float>(WINDOW_HEIGHT),
        0.0f, 1.0f,
        projection, view, world
    );

    // 5. 광선 방향 계산
    XMVECTOR direction =
        XMVectorSubtract(farPoint, nearPoint);

    float dz = XMVectorGetZ(direction);

    // 칠판 평면과 평행한 광선은 제외
    if (std::fabs(dz) < 0.000001f)
        return false;

    // 6. 칠판 표면 Z = 0 과의 교차점 계산
    float t = -XMVectorGetZ(nearPoint) / dz;

    if (t < 0.0f || t > 1.0f)
        return false;

    XMVECTOR hitPoint = XMVectorAdd(
        nearPoint,
        XMVectorScale(direction, t)
    );

    float x = XMVectorGetX(hitPoint);
    float y = XMVectorGetY(hitPoint);

    // 7. 초록색 칠판 영역 내부인지 확인
    if (x < -3.1f || x > 3.1f ||
        y < -1.7f || y > 1.7f)
    {
        return false;
    }

    // 8. 칠판 위치를 UV 좌표로 변환
    outU = (x + 3.1f) / 6.2f;
    outV = (1.7f - y) / 3.4f;

    return true;
}



bool DX12App::BeginStroke(int mouseX, int mouseY)
{
    // 1. 먼저 팔레트 클릭인지 확인
    int selectedIndex = -1;

    if (PickPalette(mouseX, mouseY, selectedIndex))
    {
        // 기존 키보드 색상과 같은 색상 사용
        constexpr std::uint32_t colors[6] =
        {
            0xFFE8F4F8u, // 흰색
            0xFF8AE5FFu, // 노란색
            0xFFB8A9F5u, // 분홍색
            0xFFE5D6A9u, // 하늘색
            0xFFC7E4B7u, // 연두색
            0xFF61A2F4u  // 주황색
        };

        if (selectedIndex == 6)
        {
            SelectEraser();
        }
        else
        {
            SetChalkColor(colors[selectedIndex]);
        }

        // 팔레트 클릭은 그림 그리기로 처리하지 않음
        return false;
    }

    // 2. 팔레트가 아니면 기존 칠판 피킹
    float u = 0.0f;
    float v = 0.0f;

    if (!PickBoard(mouseX, mouseY, u, v))
        return false;

    // 3. 칠판 위에서만 분필 그리기 시작
    boardCanvas.BeginStroke(u, v);

    return true;
}


void DX12App::ContinueStroke(int mouseX, int mouseY)
{
    float u = 0.0f;
    float v = 0.0f;

    if (PickBoard(mouseX, mouseY, u, v))
    {
        boardCanvas.ContinueStroke(u, v);
    }
    else
    {
        // 칠판 밖으로 나가면 선 끊기
        boardCanvas.EndStroke();
    }
}

void DX12App::EndStroke()
{
    boardCanvas.EndStroke();
}

unsigned int DX12App::GetStrokeCount() const
{
    return boardCanvas.GetStrokeCount();
}

void DX12App::UpdateBoardTexture()
{
    if (!boardCanvas.IsDirty())
        return;

    // 1. CPU에 있는 칠판 픽셀 데이터 가져오기
    const auto& pixels = boardCanvas.GetPixels();

    const UINT rowBytes =
        BoardCanvas::Width * sizeof(std::uint32_t);

    // 2. 업로드 버퍼에 픽셀 데이터 복사
    BYTE* mappedData = nullptr;

    D3D12_RANGE readRange = { 0, 0 };

    CheckHR(boardUploadBuffer->Map(
        0,
        &readRange,
        reinterpret_cast<void**>(&mappedData)
    ));

    const BYTE* source =
        reinterpret_cast<const BYTE*>(pixels.data());

    const UINT rowPitch =
        boardFootprint.Footprint.RowPitch;

    for (UINT y = 0; y < BoardCanvas::Height; ++y)
    {
        BYTE* destination =
            mappedData +
            static_cast<SIZE_T>(boardFootprint.Offset) +
            static_cast<SIZE_T>(y) * rowPitch;

        const BYTE* sourceRow =
            source + static_cast<SIZE_T>(y) * rowBytes;

        std::memcpy(
            destination,
            sourceRow,
            rowBytes
        );
    }

    boardUploadBuffer->Unmap(0, nullptr);

    // 3. 텍스처를 복사 가능한 상태로 변경
    D3D12_RESOURCE_BARRIER barrier = {};

    barrier.Type =
        D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

    barrier.Transition.pResource =
        boardTexture.Get();

    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_COPY_DEST;

    barrier.Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);

    // 4. 업로드 버퍼에서 GPU 텍스처로 복사
    D3D12_TEXTURE_COPY_LOCATION destination = {};

    destination.pResource = boardTexture.Get();
    destination.Type =
        D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    destination.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION sourceLocation = {};

    sourceLocation.pResource = boardUploadBuffer.Get();
    sourceLocation.Type =
        D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

    sourceLocation.PlacedFootprint = boardFootprint;

    commandList->CopyTextureRegion(
        &destination,
        0, 0, 0,
        &sourceLocation,
        nullptr
    );

    // 5. 텍스처를 셰이더에서 읽을 수 있는 상태로 복구
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_COPY_DEST;

    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    commandList->ResourceBarrier(1, &barrier);

    // 6. 텍스처 갱신 완료 표시
    boardCanvas.ClearDirty();
}

void DX12App::RotateCamera(int deltaX, int deltaY)
{
    // 마우스 이동 1픽셀당 0.25도 회전
    const float rotationSpeed = 0.25f;

    cameraYaw += deltaX * rotationSpeed;
    cameraPitch += deltaY * rotationSpeed;

    // 너무 많이 회전하지 않도록 제한
    if (cameraYaw > 60.0f)
        cameraYaw = 60.0f;

    if (cameraYaw < -60.0f)
        cameraYaw = -60.0f;

    if (cameraPitch > 35.0f)
        cameraPitch = 35.0f;

    if (cameraPitch < -35.0f)
        cameraPitch = -35.0f;

    // 변경된 카메라 위치를 적용
    UpdateCameraMatrices();
}

// 정면 시점으로 돌아가기
void DX12App::ResetCamera()
{
    cameraYaw = 0.0f;
    cameraPitch = 0.0f;

    UpdateCameraMatrices();
}

void DX12App::UpdateCameraMatrices()
{
    using namespace DirectX;

    // 1. 회전 각도를 라디안으로 변환
    float yaw = XMConvertToRadians(cameraYaw);
    float pitch = XMConvertToRadians(cameraPitch);

    // 2. 회전 각도에 따른 카메라 위치 계산
    float x = cameraDistance
        * std::sin(yaw) * std::cos(pitch);

    float y = cameraDistance
        * std::sin(pitch);

    float z = -cameraDistance
        * std::cos(yaw) * std::cos(pitch);

    // 3. 새 카메라 위치
    XMVECTOR cameraPosition = XMVectorSet(
        x, y, z, 1.0f
    );

    // 항상 칠판의 중심을 바라봄
    XMMATRIX view = XMMatrixLookAtLH(
        cameraPosition,
        XMVectorZero(),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );

    // 4. 피킹에서 사용할 View 행렬 갱신
    XMStoreFloat4x4(&cameraView, view);

    // 5. 기존 World / Projection 행렬 불러오기
    XMMATRIX world =
        XMLoadFloat4x4(&boardWorld);

    XMMATRIX projection =
        XMLoadFloat4x4(&cameraProjection);

    // 6. 새로운 World-View-Projection 계산
    XMFLOAT4X4 wvp;

    XMStoreFloat4x4(
        &wvp,
        XMMatrixTranspose(
            world * view * projection
        )
    );

    // 7. GPU Constant Buffer에 변경된 행렬 전달
    void* mappedData = nullptr;

    D3D12_RANGE readRange = { 0, 0 };

    CheckHR(constantBuffer->Map(
        0,
        &readRange,
        &mappedData
    ));

    std::memcpy(
        mappedData,
        &wvp,
        sizeof(wvp)
    );

    constantBuffer->Unmap(0, nullptr);
}

void DX12App::SetChalkColor(std::uint32_t color)
{
    boardCanvas.SetChalkColor(color);
}

void DX12App::SelectEraser()
{
    boardCanvas.SelectEraser();
}

bool DX12App::PickPalette(
    int mouseX,
    int mouseY,
    int& outIndex
) const
{
    using namespace DirectX;

    if (mouseX < 0 || mouseY < 0 ||
        mouseX >= static_cast<int>(WINDOW_WIDTH) ||
        mouseY >= static_cast<int>(WINDOW_HEIGHT))
    {
        return false;
    }

    // 현재 카메라와 칠판의 변환 행렬
    XMMATRIX world =
        XMLoadFloat4x4(&boardWorld);

    XMMATRIX view =
        XMLoadFloat4x4(&cameraView);

    XMMATRIX projection =
        XMLoadFloat4x4(&cameraProjection);

    // 마우스 위치에서 근거리/원거리 점 계산
    XMVECTOR nearPoint = XMVector3Unproject(
        XMVectorSet(
            static_cast<float>(mouseX),
            static_cast<float>(mouseY),
            0.0f, 1.0f
        ),
        0.0f, 0.0f,
        static_cast<float>(WINDOW_WIDTH),
        static_cast<float>(WINDOW_HEIGHT),
        0.0f, 1.0f,
        projection, view, world
    );

    XMVECTOR farPoint = XMVector3Unproject(
        XMVectorSet(
            static_cast<float>(mouseX),
            static_cast<float>(mouseY),
            1.0f, 1.0f
        ),
        0.0f, 0.0f,
        static_cast<float>(WINDOW_WIDTH),
        static_cast<float>(WINDOW_HEIGHT),
        0.0f, 1.0f,
        projection, view, world
    );

    XMVECTOR direction =
        XMVectorSubtract(farPoint, nearPoint);

    float dz = XMVectorGetZ(direction);

    if (std::fabs(dz) < 0.000001f)
        return false;

    // 색상 사각형의 평면 Z = -0.08
    float t =
        (-0.08f - XMVectorGetZ(nearPoint)) / dz;

    if (t < 0.0f || t > 1.0f)
        return false;

    XMVECTOR hitPoint = XMVectorAdd(
        nearPoint,
        XMVectorScale(direction, t)
    );

    float x = XMVectorGetX(hitPoint);
    float y = XMVectorGetY(hitPoint);

    // 팔레트 선택 영역의 높이 검사
    if (y < -2.69f || y > -2.18f)
        return false;

    // 일곱 개 색상 영역 검사
    for (int i = 0; i < 7; ++i)
    {
        float centerX = -2.4f + i * 0.8f;

        if (x >= centerX - 0.30f &&
            x <= centerX + 0.30f)
        {
            outIndex = i;
            return true;
        }
    }

    return false;
}

// 칠판 그림을 BMP 파일로 저장
bool DX12App::SaveBoard() const
{
    return boardCanvas.SaveBMP("Board.bmp");
}

// 저장된 BMP 파일 불러오기
bool DX12App::LoadBoard()
{
    // 진행 중인 그림 그리기 종료
    boardCanvas.EndStroke();

    return boardCanvas.LoadBMP("Board.bmp");
}
