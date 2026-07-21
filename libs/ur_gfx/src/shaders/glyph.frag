#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(binding = 1) uniform sampler2D alphaAtlas;
layout(location = 0) out vec4 fragColor;

void main()
{
    float coverage = texture(alphaAtlas, v_uv).r;
    fragColor = vec4(v_color.rgb, v_color.a * coverage);
}
