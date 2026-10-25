shader_type canvas_item;

uniform vec4 ray_color : source_color = vec4(0.58, 0.78, 1.0, 0.16);
uniform float ray_speed : hint_range(0.05, 2.0) = 0.25;

void fragment() {
	vec2 uv = UV;
	// Angled rays streaming from upper-center window downward-left & downward-right
	float coord = (uv.x - 0.5) * 3.8 + (uv.y - 0.1) * 0.65;
	float b1 = 0.5 + 0.5 * sin(coord * 9.0 + TIME * ray_speed);
	float b2 = 0.5 + 0.5 * sin(coord * 15.0 - TIME * ray_speed * 0.7 + 1.4);
	float beam = pow(b1 * b2, 2.0);

	float cone = smoothstep(0.58, 0.08, abs(uv.x - 0.5) / mix(0.22, 0.85, uv.y));
	float vfade = smoothstep(0.0, 0.16, uv.y) * smoothstep(0.96, 0.22, uv.y);

	float alpha = beam * cone * vfade * ray_color.a;
	COLOR = vec4(ray_color.rgb, alpha);
}
