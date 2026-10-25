shader_type canvas_item;

uniform vec4 sheen_color : source_color = vec4(1.0, 0.88, 0.45, 0.28);
uniform float speed : hint_range(0.2, 4.0) = 1.1;

void fragment() {
	vec4 base = texture(TEXTURE, UV);
	float sweep = fract(TIME * speed * 0.35);
	float diag = (UV.x * 0.65 + UV.y * 0.35);
	float band = smoothstep(0.14, 0.0, abs(diag - sweep));
	vec3 col = base.rgb + sheen_color.rgb * band * sheen_color.a;
	COLOR = vec4(col, base.a);
}
