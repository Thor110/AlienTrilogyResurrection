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
    float3 Colour : TEXCOORD2;
};

struct Output
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float3 Colour : TEXCOORD1;
    // Distance from the camera in world units, for the original's
    // draw-distance fade. Taken from the clip-space W, which for a standard
    // perspective projection is exactly the view-space depth.
    float Distance : TEXCOORD2;
};

Output main(Input input)
{
    Output output;
    output.Position = mul(mvp, float4(input.Position, 1.0f));
    output.UV = input.UV;
    // Interpolated across the face, which is what makes light mode 4
    // (gouraud) work - it gives each corner a different colour. Every
    // other mode sets all four corners equal, so this interpolates to a
    // constant and costs nothing.
    output.Colour = input.Colour;
    output.Distance = output.Position.w;
    return output;
}
