shader_type canvas_item;

uniform float vignette_intensity : hint_range(0.0, 1.0) = 0.24;
uniform float grain_amount : hint_range(0.0, 0.15) = 0.022;
uniform float aberration : hint_range(0.0, 0.015) = 0.001;
uniform float danger_pulse : hint_range(0.0, 1.0) = 0.0;
uniform sampler2D screen_tex : hint_screen_texture, filter_linear_mipmap;

float hash12(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

void fragment() {
	vec2 uv = SCREEN_UV;
	vec2 centered = uv - vec2(0.5);
	float dist = length(centered);

	float ca = aberration + danger_pulse * 0.0045;
	vec2 offset = centered * ca;
	float r = texture(screen_tex, clamp(uv + offset, vec2(0.001), vec2(0.999))).r;
	float g = texture(screen_tex, uv).g;
	float b = texture(screen_tex, clamp(uv - offset, vec2(0.001), vec2(0.999))).b;
	vec3 col = vec3(r, g, b);

	// Subtle film grain that never crushes midtones into black
	float n = hash12(uv * vec2(1280.0, 720.0) + vec2(TIME * 37.0, TIME * 19.0)) - 0.5;
	col += n * grain_amount;

	// Peripheral vignette (clamped so center of screen remains 100% clear and visible)
	float vig = smoothstep(0.38, 0.92, dist) * (vignette_intensity + danger_pulse * 0.28);
	vec3 vig_tint = mix(vec3(0.02, 0.02, 0.03), vec3(0.18, 0.02, 0.02), danger_pulse);
	col = mix(col, vig_tint, clamp(vig, 0.0, 0.62));

	COLOR = vec4(col, 1.0);
}
