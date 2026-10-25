shader_type canvas_item;
render_mode unshaded, blend_add;

uniform vec4 beam_color : source_color = vec4(0.25, 1.0, 0.82, 1.0);
uniform float power : hint_range(0.0, 1.0) = 1.0;

void fragment() {
	float pulse = 0.82 + sin(TIME * 38.0 + UV.x * 24.0) * 0.18;
	float core = pow(max(0.0, 1.0 - abs(UV.y - 0.5) * 2.0), 2.0);
	COLOR = vec4(beam_color.rgb * (1.0 + core * 1.8), beam_color.a * pulse * power);
}
