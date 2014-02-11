#version 330 core
uniform mat4 world_to_screen;  // projection * view
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec3 vertexColor;
out vec3 co;

void main(){
	gl_Position = world_to_screen * vec4(vertexPosition_modelspace, 1);
	co = vertexColor;
}
