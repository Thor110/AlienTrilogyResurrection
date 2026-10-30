#version 450
// Model fragment shader - the GLSL twin of model.frag.hlsl, for Vulkan
// (SPIR-V) and, through spirv-cross, Metal (MSL). Keep the two in step; the
// HLSL carries the comments on why each step is there.
//
// Resource sets follow SDL_GPU's documented SPIR-V convention for fragment
// shaders: sampled textures in set 2, uniform buffers in set 3.

layout(set = 2, binding = 0) uniform sampler2D ModelTexture;

// x = fade enable (0 or 1), y = where the fade starts, z = its length.
// tint.rgb scales the whole draw: a model's light (1,1,1 for the rest).
layout(set = 3, binding = 0) uniform FogParams
{
    vec4 fogParams;
    vec4 tint;
};

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inColour;
layout(location = 2) in float inDistance;

layout(location = 0) out vec4 outColour;

void main()
{
    vec4 color = texture(ModelTexture, inUV);

    // Fully transparent texels write neither colour nor depth.
    if (color.a < 0.5) { discard; }

    // The level light's colour scales the texel.
    color.rgb *= inColour;
    color.rgb *= tint.rgb;

    // The original's draw-distance fade toward black (FUN_0004ecdc).
    if (fogParams.x > 0.5)
    {
        float d = abs(inDistance);
        float t = clamp((d - fogParams.y) / max(fogParams.z, 1.0), 0.0, 1.0);
        color.rgb *= (1.0 - t);
    }

    outColour = color;
}
