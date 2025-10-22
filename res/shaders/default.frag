#version 450 core

layout(location = 0) out vec4 o_color;
layout(location = 0) in vec3 v_color;
layout(location = 1) in vec2 v_texCoord;

layout(set = 0, binding = 1) uniform sampler2D u_texture;

void main()
{
    o_color = texture(u_texture, v_texCoord) * vec4(v_color, 1.0);
}