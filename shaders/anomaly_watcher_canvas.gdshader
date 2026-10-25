shader_type spatial;

uniform sampler2D canvas_tex : source_color, filter_linear_mipmap;
uniform float warp_amount : hint_range(0.0, 1.0) = 1.0;

void fragment() {
	vec2 uv = UV;
	// Subtle organic breathing warp around the eyes & mouth of the anomalous portrait
	float d = distance(uv, vec2(0.5, 0.42));
	float pulse = sin(TIME * 2.1) * 0.012 * warp_amount * smoothstep(0.42, 0.05, d);
	uv += (uv - vec2(0.5, 0.42)) * pulse;
	vec4 tex = texture(canvas_tex, clamp(uv, vec2(0.0), vec2(1.0)));
	ALBEDO = tex.rgb;
	ROUGHNESS = mix(0.68, 0.22, smoothstep(0.18, 0.35, tex.r - tex.g));
	EMISSION = max(vec3(0.0), tex.rgb - vec3(0.55, 0.25, 0.25)) * 1.4 * warp_amount;
}
