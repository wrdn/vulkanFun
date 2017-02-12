#version 450

struct PSInput
{
    vec4 position;
    vec2 uv;
};

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 uv;
layout(location = 0) out vec2 uv_1;

void main()
{
    PSInput result;
    result.position = position;
    result.uv = vec2(uv.xy);
    gl_Position = result.position;
    uv_1 = result.uv;
}

