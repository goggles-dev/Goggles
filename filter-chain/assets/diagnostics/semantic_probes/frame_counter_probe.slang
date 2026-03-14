#version 450

layout(std140, set = 0, binding = 0) uniform UBO
{
   mat4 MVP;
   uint FrameCount;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;

void main()
{
   gl_Position = global.MVP * Position;
}

#pragma stage fragment
layout(location = 0) out vec4 FragColor;

void main()
{
   float value = float(global.FrameCount % 4u) / 3.0;
   FragColor = vec4(value, value, value, 1.0);
}
