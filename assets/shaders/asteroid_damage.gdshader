shader_type canvas_item;
render_mode unshaded;

uniform float damage : hint_range(0.0, 1.0) = 0.0;
uniform float hit_flash : hint_range(0.0, 1.0) = 0.0;
uniform float seed = 1.0;
uniform vec4 vein_color : source_color = vec4(0.4, 0.9, 0.85, 1.0);

float hash21(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7)) + seed * 17.0) * 43758.5453);
}

void fragment() {
	vec4 rock = texture(TEXTURE, UV);
	vec2 p = UV - 0.5;
	float fracture_a = abs(sin((p.x * 13.0 + p.y * 7.0 + sin(p.y * 19.0) * 0.7 + seed) * 3.0));
	float fracture_b = abs(sin((p.y * 16.0 - p.x * 5.0 + sin(p.x * 15.0) * 0.8) * 2.5));
	float crack = smoothstep(0.075, 0.005, min(fracture_a, fracture_b));
	crack *= smoothstep(0.18, 0.65, damage) * smoothstep(0.65, 0.1, length(p));
	float pits = step(0.975 - damage * 0.05, hash21(floor(UV * 32.0))) * damage;
	vec3 color = rock.rgb * (1.0 - damage * 0.24 - pits * 0.32);
	color = mix(color, vec3(0.018, 0.022, 0.028), crack * 0.68);
	color += vein_color.rgb * crack * damage * 0.38;
	color = mix(color, vec3(1.0, 0.87, 0.60), hit_flash * 0.72);
	COLOR = vec4(color, rock.a);
}
