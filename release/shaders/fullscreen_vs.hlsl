// fullscreen_vs.hlsl  D3D11 全屏四边形顶点着色器
// 输入: 屏幕空间四边形顶点 [-1,1]×[-1,1]
// 输出: 裁剪空间坐标 + UV [0,1]×[0,1]
struct VSInput {
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.pos = float4(input.pos, 0.0, 1.0);
    output.uv  = input.uv;
    return output;
}
