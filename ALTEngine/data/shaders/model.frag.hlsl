// Model fragment/pixel shader - unlit textured sample.
// Compiled to DXIL via dxc at build time (see CMakeLists.txt).
//
// Resource binding follows SDL_GPU's documented DXIL convention for
// pixel shaders: sampled textures/samplers at (t[n]/s[n], space2).

Texture2D<float4> ModelTexture : register(t0, space2);
SamplerState ModelSampler : register(s0, space2);

struct Input
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

float4 main(Input input) : SV_Target0
{
    return ModelTexture.Sample(ModelSampler, input.UV);
}
