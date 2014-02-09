#version 330 core
in vec3 co;
layout(location = 0) out vec4 color;
 
void main(){
    color = vec4(co, 1);
}
