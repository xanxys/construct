#version 330 core
uniform float luminance;
uniform sampler2D texture;
layout(location = 0) out vec4 color;
in vec2 uv;
 
void main() {
	vec4 col_raw = texture2D(texture, uv);
    color = vec4(luminance * col_raw.xyz, col_raw.w);
}
