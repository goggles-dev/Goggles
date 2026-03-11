#version 450

layout(std140, set = 0, binding = 0) uniform UBO
{
   mat4 MVP;
   vec4 SourceSize;
   vec4 OutputSize;
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
   FragColor = vec4(global.SourceSize.x / 255.0,
                    global.SourceSize.y / 255.0,
                    global.OutputSize.x / 255.0,
                    global.OutputSize.y / 255.0);
}
