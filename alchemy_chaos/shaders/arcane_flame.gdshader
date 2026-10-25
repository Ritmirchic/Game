shader_type canvas_item;

uniform vec4 outer_color : source_color = vec4(1.0, 0.35, 0.06, 1.0);
uniform vec4 inner_color : source_color = vec4(1.0, 0.88, 0.35, 1.0);
uniform float intensity : hint_range(0.2, 2.5) = 1.0;
uniform float speed : hint_range(0.5, 10.0) = 4.2;

float hash21(vec2 p) {
	p = fract(p * vec2(234.12, 654.89));
	p += dot(p, p + 33.17);
	return fract(p.x * p.y);
}

float vnoise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(
		mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x),
		mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x),
		f.y
	);
}

void fragment() {
	vec2 uv = UV;
	float x = (uv.x - 0.5) * 2.0;
	float y = 1.0 - uv.y; // 0 at bottom, 1 at top

	float n1 = vnoise(vec2(x * 3.2, y * 4.5 - TIME * speed));
	float n2 = vnoise(vec2(x * 6.5 + 3.1, y * 8.0 - TIME * speed * 1.45));
	float sway = (n1 - 0.5) * 0.42 * y + (n2 - 0.5) * 0.22 * y;

	float width = mix(0.68, 0.04, pow(y, 0.85)) * clamp(intensity, 0.4, 1.45);
	float dist = abs(x + sway);
	float shape = smoothstep(width, width * 0.25, dist) * smoothstep(1.0, 0.08, y) * smoothstep(0.0, 0.12, y);
	float core = smoothstep(width * 0.52, 0.0, dist) * smoothstep(0.75, 0.05, y);

	vec3 col = mix(outer_color.rgb, inner_color.rgb, clamp(core * 1.35, 0.0, 1.0));
	col += vec3(1.0, 1.0, 0.95) * pow(core, 2.2) * 0.55;

	COLOR = vec4(col, clamp(shape * 0.95, 0.0, 1.0));
}
