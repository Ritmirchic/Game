shader_type canvas_item;
render_mode unshaded, blend_add;

uniform float impact : hint_range(0.0, 1.0) = 0.0;
uniform float shield_fraction : hint_range(0.0, 1.0) = 1.0;
uniform vec4 shield_color : source_color = vec4(0.25, 0.8, 1.0, 1.0);

void fragment() {
	vec2 p = UV * 2.0 - 1.0;
	float r = length(p);
	float a = atan(p.y, p.x);
	float ring = smoothstep(0.065, 0.005, abs(r - 0.77));
	float hex = 0.65 + 0.35 * sin(a * 18.0 + TIME * 0.8);
	float shimmer = 0.65 + 0.35 * sin(a * 4.0 - TIME * 1.7);
	float alpha = ring * (0.025 + impact * 0.72 + (1.0 - shield_fraction) * 0.035) * hex;
	alpha += smoothstep(0.03, 0.0, abs(r - 0.70)) * impact * 0.25;
	COLOR = vec4(shield_color.rgb * (0.55 + shimmer * 0.7), alpha);
}
