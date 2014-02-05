#version 330 core
uniform mat4 world_to_screen;  // projection * view
layout(location = 0) in vec3 pos_world;
layout(location = 1) in vec2 uv_tex;
out vec2 uv;

void main(){
	gl_Position = world_to_screen * vec4(pos_world, 1);
	uv = uv_tex;
}
