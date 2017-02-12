#version 450

layout(set = 0, binding = 0, std140) uniform RootConstants
{
    uint activeMip;
} _27;

layout(set = 0, binding = 0) uniform texture2D g_texture;
layout(set = 0, binding = 0) uniform sampler g_sampler;

layout(location = 0) out vec4 _entryPointOutput;
layout(location = 0) in vec2 uv;

void main()
{
    _entryPointOutput = textureLod(sampler2D(g_texture, g_sampler), uv, float(_27.activeMip));
}

