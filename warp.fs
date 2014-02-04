#version 330
uniform float diffusion;
uniform vec2 LensCenter;
uniform vec2 ScreenCenter;
uniform vec2 Scale;
uniform vec2 ScaleIn;
uniform vec4 HmdWarpParam;
uniform sampler2D Texture0;  // side-by-side images for left and right eyes
in vec2 oTexCoord;
layout(location = 0) out vec4 color;


vec2 HmdWarp(vec2 in01) {
	vec2  theta = (in01 - LensCenter) * ScaleIn;  // Scale to [-1,1]
	float rSq = theta.x * theta.x + theta.y * theta.y;
	vec2  theta1 = theta * (HmdWarpParam.x + HmdWarpParam.y * rSq +
		HmdWarpParam.z * rSq * rSq + HmdWarpParam.w * rSq * rSq * rSq);
	return LensCenter + Scale * theta1;
}


void main() {
	vec2 tc = HmdWarp(oTexCoord);
	if(!all(equal(clamp(tc, ScreenCenter-vec2(0.25,0.5), ScreenCenter+vec2(0.25,0.5)), tc)))
		color = vec4(0);
	else
		color = (texture2D(Texture0, tc) +
			texture2D(Texture0, tc + vec2(diffusion, 0)) +
			texture2D(Texture0, tc + vec2(-diffusion, 0)) +
			texture2D(Texture0, tc + vec2(0, diffusion)) +
			texture2D(Texture0, tc + vec2(0, -diffusion))) / 5;
}
