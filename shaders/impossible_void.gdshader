shader_type spatial;
render_mode unshaded;

uniform sampler2D void_tex : source_color, filter_linear_mipmap, repeat_enable;

void fragment() {
	vec2 uv = UV;
	vec2 drift = vec2(sin(TIME * 0.22 + uv.y * 4.0) * 0.025, -TIME * 0.035);
	vec3 base = texture(void_tex, uv + drift).rgb;
	float ring = abs(length(uv - vec2(0.5, 0.38)) - (0.18 + sin(TIME * 1.4) * 0.015));
	float glow = smoothstep(0.05, 0.0, ring);
	ALBEDO = base + vec3(0.75, 0.16, 0.22) * glow * 0.85;
}
