#version 330
uniform mat4 Texm;
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
out vec2 oTexCoord;

void main() {
    gl_Position = Position;
    oTexCoord = Position.xy;
    // oTexCoord.y = 1.0 - oTexCoord.y;
}
