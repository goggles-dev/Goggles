#version 450

#pragma parameter BRIGHTNESS "Brightness" 0.0 0.0 1.0 0.1

layout(std140, set = 0, binding = 0) uniform UBO
{
   mat4 MVP;
   float BRIGHTNESS;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
   gl_Position = global.MVP * Position;
   vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 1) uniform sampler2D Source;

void main()
{
   vec4 source = texture(Source, vTexCoord);
   if (vTexCoord.x < 0.5) {
      FragColor = vec4(vec3(global.BRIGHTNESS), 1.0);
      return;
   }

   FragColor = source;
}
