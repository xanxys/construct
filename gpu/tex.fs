#version 330 core
uniform sampler2D texture;
layout(location = 0) out vec4 color;
in vec2 uv;
 
void main(){
    color = texture2D(texture, uv);
}
