#version 330
uniform float xoffset;
layout(location = 0) in vec4 Position;
out vec2 oTexCoord;

void main() {
    gl_Position = Position;
    oTexCoord.x = (1 + Position.x) / 4 + xoffset;
    oTexCoord.y = (1 + Position.y) / 2;
}
