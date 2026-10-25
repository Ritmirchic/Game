shader_type canvas_item;

uniform vec4 color_a : source_color = vec4(0.18, 0.72, 0.65, 1.0);
uniform vec4 color_b : source_color = vec4(0.45, 0.25, 0.88, 1.0);
uniform vec4 color_c : source_color = vec4(0.95, 0.75, 0.25, 1.0);
uniform float swirl_speed : hint_range(0.1, 8.0) = 1.35;
uniform float boil_intensity : hint_range(0.0, 2.0) = 0.25;
uniform float freeze_amount : hint_range(0.0, 1.0) = 0.0;
uniform float heat_amount : hint_range(0.0, 1.0) = 0.0;
uniform float electric_amount : hint_range(0.0, 1.0) = 0.0;
uniform float reaction_flash : hint_range(0.0, 1.0) = 0.0;

float hash21(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

float noise2(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = hash21(i);
	float b = hash21(i + vec2(1.0, 0.0));
	float c = hash21(i + vec2(0.0, 1.0));
	float d = hash21(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void fragment() {
	vec4 mask_tex = texture(TEXTURE, UV);
	if (mask_tex.a < 0.02) {
		discard;
	}

	vec2 p = (UV - vec2(0.5)) * 2.0;
	float r = length(p);
	float ang = atan(p.y, p.x);

	float t = TIME * swirl_speed;
	float spiral = sin(ang * 3.0 + r * 7.5 - t * 2.1);
	float spiral2 = cos(ang * 2.0 - r * 5.2 + t * 1.4);
	float n = noise2(p * 3.8 + vec2(cos(t * 0.4), sin(t * 0.4)) * 1.6);

	float blend1 = clamp(0.5 + 0.35 * spiral + 0.25 * (n - 0.5), 0.0, 1.0);
	float blend2 = clamp(0.5 + 0.35 * spiral2, 0.0, 1.0);
	vec3 col = mix(color_a.rgb, color_b.rgb, blend1);
	col = mix(col, color_c.rgb, blend2 * 0.35);

	// Boiling surface rings
	float boil_rings = sin(r * 22.0 - TIME * (4.0 + boil_intensity * 6.0) + n * 6.0);
	col += vec3(0.18, 0.22, 0.25) * max(0.0, boil_rings) * boil_intensity * (1.0 - r * 0.6);

	// Thermal incandescent glow
	if (heat_amount > 0.01) {
		float core = pow(clamp(1.0 - r * 0.85, 0.0, 1.0), 1.6);
		col = mix(col, vec3(1.0, 0.52, 0.08), heat_amount * 0.65);
		col += vec3(1.0, 0.92, 0.55) * core * heat_amount * 0.7;
	}

	// Cryogenic frost lattice creeping inward from rim
	if (freeze_amount > 0.01) {
		float frost_n = noise2(p * 11.0);
		float rim_frost = smoothstep(1.05 - freeze_amount * 0.95, 1.05, r + frost_n * 0.35);
		col = mix(col, vec3(0.78, 0.96, 1.0), rim_frost * freeze_amount * 0.92);
	}

	// Galvanic lightning filaments
	if (electric_amount > 0.01) {
		float arc = abs(sin((p.x * 9.0 + noise2(p * 7.0 + TIME * 9.0) * 3.2) - TIME * 5.0));
		float bolt = smoothstep(0.09, 0.0, arc) * electric_amount;
		col += vec3(0.55, 0.95, 1.0) * bolt * 1.4;
	}

	// Meniscus rim highlight & inner cauldron wall shadow
	float rim_glow = smoothstep(0.72, 0.96, r) * (1.0 - smoothstep(0.96, 1.02, r));
	col += col * rim_glow * 0.55 + vec3(0.22) * rim_glow;
	col *= mix(1.05, 0.62, smoothstep(0.78, 1.0, r));

	// Reaction flash pulse
	col = mix(col, vec3(1.0, 0.98, 0.92), reaction_flash * 0.78);

	COLOR = vec4(col, mask_tex.a);
}
