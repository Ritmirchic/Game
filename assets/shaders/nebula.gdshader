shader_type canvas_item;
render_mode unshaded;

uniform vec2 camera_offset = vec2(0.0);
uniform vec4 sector_tint : source_color = vec4(0.3, 0.85, 0.8, 1.0);
uniform float ambient_motion = 1.0;

float hash21(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

void fragment() {
	vec2 uv = UV;
	vec2 drift = vec2(sin(TIME * 0.025), cos(TIME * 0.019)) * 0.004 * ambient_motion;
	vec2 warped = uv + drift + camera_offset * 0.000015;
	warped += vec2(sin(uv.y * 9.0 + TIME * 0.035), cos(uv.x * 8.0 - TIME * 0.03)) * 0.0015 * ambient_motion;
	vec4 base = texture(TEXTURE, warped);
	float dust = hash21(floor((uv + camera_offset * 0.00003) * vec2(900.0, 500.0)));
	float star = step(0.9987, dust) * (0.3 + 0.7 * sin(TIME * 1.7 + dust * 30.0) * 0.5 + 0.5);
	float scan = sin(FRAGCOORD.y * 1.55) * 0.012;
	float vignette = smoothstep(0.95, 0.25, distance(uv, vec2(0.5)));
	vec3 color = base.rgb * (0.90 + vignette * 0.14) + sector_tint.rgb * star * 0.28;
	color += scan * 0.22;
	COLOR = vec4(color, 1.0);
}
