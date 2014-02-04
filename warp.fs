#version 330
uniform vec4 HmdWarpParam;
uniform sampler2D Texture0; // side-by-side images for left and right eyes
uniform vec2 LensCenter;
uniform vec2 ScaleOut;
in vec2 oTexCoord;
layout(location = 0) out vec4 color;

vec2 distortRadial(vec2 r, vec2 d) {
	r -= d;

	float r_sq = r.x * r.x + r.y * r.y;
	vec2 r_new = r * (HmdWarpParam.x + HmdWarpParam.y * r_sq +
		HmdWarpParam.z * r_sq * r_sq + HmdWarpParam.w * r_sq * r_sq * r_sq);

	return d + ScaleOut * r_new;
}

void main() {
	if(oTexCoord.x < 0) {
		// left eye
		vec2 tc = distortRadial(oTexCoord - vec2(-0.5, 0), -LensCenter) + vec2(0.25, 0.5);
		if(!all(equal(clamp(tc, vec2(0, 0), vec2(0.5, 1)), tc)))
			color = vec4(0);
		else
			color = texture2D(Texture0, tc);
	} else {
		// right eye
		vec2 tc = distortRadial(oTexCoord - vec2(0.5, 0), LensCenter) + vec2(0.75, 0.5);
		if(!all(equal(clamp(tc, vec2(0.5, 0), vec2(1, 1)), tc)))
			color = vec4(0);
		else
			color = texture2D(Texture0, tc);
	}
}
