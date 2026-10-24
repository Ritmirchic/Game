import { PBRCustomMaterial } from '@babylonjs/materials'

/**
 * World-space sand variation. The regular PBR texture remains responsible for
 * grain and bump; this layer gives each dune a different mineral tone instead
 * of repeating one flat colour across the whole streamed map.
 */
export function installSandShader(material: PBRCustomMaterial): void {
  material.CustomParts.Fragment_Definitions = /* glsl */`
    float mirageSandHash(vec2 p) {
      return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
    }
    float mirageSandNoise(vec2 p) {
      vec2 i = floor(p);
      vec2 f = fract(p);
      f = f * f * (3.0 - 2.0 * f);
      float a = mirageSandHash(i);
      float b = mirageSandHash(i + vec2(1.0, 0.0));
      float c = mirageSandHash(i + vec2(0.0, 1.0));
      float d = mirageSandHash(i + vec2(1.0, 1.0));
      return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
    }
  `
  material.CustomParts.Fragment_Custom_Albedo = /* glsl */`
    vec2 sandPosition = vPositionW.xz;
    float duneTone = mirageSandNoise(sandPosition * 0.035);
    float mineralTone = mirageSandNoise(sandPosition * 0.42);
    float windBands = sin(sandPosition.x * 1.65 + sandPosition.y * 0.24
      + mirageSandNoise(sandPosition * 0.075) * 5.0);
    float rippleTone = smoothstep(0.15, 0.92, windBands) * 0.12;
    float slopeTone = 1.0 - clamp(vNormalW.y, 0.0, 1.0);
    vec3 sandTint = mix(vec3(0.76, 0.71, 0.61), vec3(1.08, 1.02, 0.91), duneTone);
    surfaceAlbedo *= sandTint;
    surfaceAlbedo *= 0.95 + mineralTone * 0.085 + rippleTone;
    surfaceAlbedo = mix(surfaceAlbedo, surfaceAlbedo * vec3(0.74, 0.68, 0.59), slopeTone * 0.28);
  `
}

/** Animated colour and glints complement the existing PBR normal/reflection passes. */
export function installWaterShader(material: PBRCustomMaterial): void {
  material.AddUniform('mirageTime', 'float', 0)
  material.CustomParts.Fragment_Definitions = /* glsl */`
    float mirageWaterWave(vec2 p, float t) {
      float a = sin(p.x * 2.7 + p.y * 1.35 + t * 1.35);
      float b = sin(p.x * -1.4 + p.y * 3.8 - t * 0.92);
      float c = sin((p.x + p.y) * 6.2 + t * 1.8);
      return a * 0.48 + b * 0.32 + c * 0.2;
    }
  `
  material.CustomParts.Fragment_Custom_Albedo = /* glsl */`
    vec2 waterPosition = vPositionW.xz;
    float broadWave = mirageWaterWave(waterPosition * 0.72, mirageTime);
    float smallWave = mirageWaterWave(waterPosition * 2.4, mirageTime * 1.35);
    float waterMix = clamp(0.5 + broadWave * 0.22 + smallWave * 0.08, 0.0, 1.0);
    vec3 deepWater = vec3(0.018, 0.18, 0.21);
    vec3 shallowWater = vec3(0.095, 0.46, 0.43);
    surfaceAlbedo = mix(deepWater, shallowWater, waterMix);
  `
  material.CustomParts.Fragment_Before_Fog = /* glsl */`
    float glintWave = mirageWaterWave(vPositionW.xz * 1.9, mirageTime * 1.15);
    float sunGlint = pow(max(0.0, glintWave), 18.0);
    finalColor.rgb += vec3(0.38, 0.82, 0.72) * sunGlint * 0.22;
  `
}

export function setWaterTime(material: PBRCustomMaterial, time: number): void {
  material._newUniformInstances['float-mirageTime'] = time
}
