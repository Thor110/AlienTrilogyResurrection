#version 450
// Model vertex shader - the GLSL twin of model.vert.hlsl, for Vulkan
// (SPIR-V) and, through spirv-cross, Metal (MSL). Keep the two in step.
//
// Resource sets follow SDL_GPU's documented SPIR-V convention for vertex
// shaders: uniform buffers in set 1.

layout(set = 1, binding = 0) uniform UniformBlock
{
    mat4 mvp;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inColour;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outColour;
// Distance from the camera in world units, for the original's draw-distance
// fade: the clip-space W, which for a perspective projection is the
// view-space depth.
layout(location = 2) out float outDistance;

void main()
{
    gl_Position = mvp * vec4(inPosition, 1.0);
    outUV = inUV;
    outColour = inColour;
    outDistance = gl_Position.w;
}
