#version 450

layout (set = 0, binding = 0) uniform UBO
{
  mat4 projection;
  mat4 view;
  mat4 model;
}
ubo;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_color;
layout (location = 2) in vec2 in_uv;

layout (location = 0) out vec3 out_color;
layout (location = 1) out vec2 out_uv;

void
main ()
{
  out_color   = in_color;
  out_uv      = in_uv;
  gl_Position = ubo.projection * ubo.view * ubo.model * vec4 (in_position, 1.0);
}
