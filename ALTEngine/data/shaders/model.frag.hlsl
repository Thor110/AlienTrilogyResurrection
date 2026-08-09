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
    float3 Colour : TEXCOORD1;
};

float4 main(Input input) : SV_Target0
{
    float4 color = ModelTexture.Sample(ModelSampler, input.UV);

    // Discard (skip BOTH colour and depth writes) for fully-transparent
    // texels, rather than letting them through with alpha=0 - without
    // this, a transparent cutout pixel still writes its depth, which
    // incorrectly occludes whatever's behind it (other parts of the
    // SAME model, for the colour-key cutout models like the speakers/
    // Multitap) even though the pixel itself is invisible. Confirmed
    // bug (Edward, 2026): "the transparency goes through to the
    // background, but covers the model behind it with the background
    // as it spins around" - exactly this symptom.
    clip(color.a - 0.5);

    // Level light modulate. The light table's RGB scales the texel; see
    // LIGHT_COLOUR_NEUTRAL in LightTable.h for the scale this assumes.
    // Model and door geometry emits 1,1,1 and so passes through
    // unchanged.
    color.rgb *= input.Colour;

    return color;
}
