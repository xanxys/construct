#version 330
uniform vec4 HmdWarpParam;
uniform sampler2D Texture0; // side-by-side images for left and right eyes
uniform vec2 LensCenter;
in vec2 oTexCoord;
layout(location = 0) out vec4 color;

vec2 distortRadial(vec2 theta) {
	float rSq = theta.x * theta.x + theta.y * theta.y;
	return theta * (HmdWarpParam.x + HmdWarpParam.y * rSq +
		HmdWarpParam.z * rSq * rSq + HmdWarpParam.w * rSq * rSq * rSq);
}

void main() {
	if(oTexCoord.x < 0) {
		// left eye
		vec2 tc = distortRadial(oTexCoord - vec2(-0.5, 0) - LensCenter) + vec2(0.25, 0.5);
		if(!all(equal(clamp(tc, vec2(0, 0), vec2(0.5, 1)), tc)))
			color = vec4(0);
		else
			color = texture2D(Texture0, tc);
	} else {
		// right eye
		vec2 tc = distortRadial(oTexCoord - vec2(0.5, 0) + LensCenter) + vec2(0.75, 0.5);
		if(!all(equal(clamp(tc, vec2(0.5, 0), vec2(1, 1)), tc)))
			color = vec4(0);
		else
			color = texture2D(Texture0, tc);
	}
}
