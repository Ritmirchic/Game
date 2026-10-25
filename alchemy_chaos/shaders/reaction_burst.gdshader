shader_type canvas_item;

uniform vec4 ring_color : source_color = vec4(1.0, 0.72, 0.25, 1.0);
uniform vec4 secondary_color : source_color = vec4(0.45, 0.92, 1.0, 1.0);
uniform float progress : hint_range(0.0, 1.0) = 0.0;
uniform float thickness : hint_range(0.02, 0.35) = 0.12;

void fragment() {
	if (progress <= 0.001 || progress >= 0.999) {
		COLOR = vec4(0.0);
	} else {
		vec2 p = (UV - vec2(0.5)) * 2.0;
		float r = length(p);
		float target_r = progress * 0.96;
		float ring = smoothstep(thickness, 0.0, abs(r - target_r));
		float inner_glow = smoothstep(target_r, 0.0, r) * (1.0 - progress) * 0.42;
		float ang = atan(p.y, p.x);
		float spokes = 0.7 + 0.3 * sin(ang * 8.0 + progress * 12.0);
		vec3 col = mix(ring_color.rgb, secondary_color.rgb, 0.5 + 0.5 * sin(ang * 3.0 + progress * 6.0));
		float fade = 1.0 - pow(progress, 1.6);
		COLOR = vec4(col, clamp((ring * spokes + inner_glow) * fade, 0.0, 1.0));
	}
}
