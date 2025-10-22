#version 450 core

// layout(push_constant) uniform PushConstants {
//     mat4 viewProjection;
// } pc;

layout(set = 0,binding = 0) uniform UniformBufferObject {
    mat4 viewProjection;
    mat4 transform;
} ubo;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
layout(location = 2) in vec2 a_texCoord;

layout(location = 0) out vec3 v_color;
layout(location = 1) out vec2 v_texCoord;

void main()
{
    gl_Position = ubo.viewProjection * ubo.transform * vec4(a_position, 1.0);
    v_texCoord = a_texCoord;
    v_color = a_color;
}
