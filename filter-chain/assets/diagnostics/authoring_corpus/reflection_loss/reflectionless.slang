#version 450

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;

void main()
{
   gl_Position = Position;
}

#pragma stage fragment
layout(location = 0) out vec4 FragColor;

void main()
{
   FragColor = vec4(0.25, 0.5, 0.75, 1.0);
}
