// Model vertex shader - simple MVP transform, passes UV through.
// Compiled to DXIL via dxc at build time (see CMakeLists.txt).
//
// HLSL semantics follow SDL_GPU's documented D3D12 convention: non
// system-value semantics must be TEXCOORD0, TEXCOORD1, etc. Resource
// binding follows SDL_GPU's documented DXIL convention for vertex
// shaders: uniform buffers at (b[n], space1).

cbuffer UniformBlock : register(b0, space1)
{
    float4x4 mvp;
};

struct Input
{
    float3 Position : TEXCOORD0;
    float2 UV : TEXCOORD1;
};

struct Output
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

Output main(Input input)
{
    Output output;
    output.Position = mul(mvp, float4(input.Position, 1.0f));
    output.UV = input.UV;
    return output;
}
