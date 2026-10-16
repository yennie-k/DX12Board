
cbuffer SceneCB : register(b0)
{
    float4x4 worldViewProj;
};

// 정점 데이터
struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

// 정점 셰이더 결과
struct VSOutput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float2 uv : TEXCOORD0;
};


// 정점 셰이더
VSOutput VSMain(VSInput input)
{
    VSOutput output;

    
    output.position = mul(
        float4(input.position, 1.0f),
        worldViewProj
    );


    output.color = input.color;
    
    // 칠판 로컬 좌표를 UV 좌표로 변환
    output.uv = float2(
        (input.position.x + 3.1f) / 6.2f,
        (1.7f - input.position.y) / 3.4f
    );

    return output;
}

// 픽셀 셰이더
float4 PSMain(VSOutput input) : SV_TARGET
{
    return float4(input.color, 1.0f);
}

// 칠판 그림 텍스처
Texture2D boardTexture : register(t0);

// 텍스처 샘플러
SamplerState boardSampler : register(s0);

// 칠판 앞면 전용 픽셀 셰이더
float4 PSBoard(VSOutput input) : SV_TARGET
{
    return boardTexture.Sample(
        boardSampler,
        input.uv
    );
}
