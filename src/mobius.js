// Математика ленты Мёбиуса.
//
// Параметризация (u — вдоль ленты, v — поперёк):
//   x = (R + v*cos(u/2)) * cos(u)
//   y = v * sin(u/2)
//   z = (R + v*cos(u/2)) * sin(u)
//
// u не замыкается: (u + 2π, -v) — та же точка пространства, что (u, v).
// Нормаль поверхности после одного витка меняет знак в той же точке мира.
// Именно поэтому у этого мира нет низа.

import * as THREE from 'three';

export const R = 9;              // радиус ленты
export const HALF_W = 2.0;       // половина ширины
export const LAP = Math.PI * 2;  // один виток

export function surfacePoint(u, v, out = new THREE.Vector3()) {
  const a = u * 0.5;
  const k = R + v * Math.cos(a);
  return out.set(k * Math.cos(u), v * Math.sin(a), k * Math.sin(u));
}

const _dU = new THREE.Vector3();
const _dV = new THREE.Vector3();
const _tan = new THREE.Vector3();
const _nrm = new THREE.Vector3();
const _side = new THREE.Vector3();

/**
 * Локальный репер поверхности в точке (u, v).
 *   tangent — вдоль ленты (вперёд)
 *   normal  — канонический «верх» поверхности
 *   side    — normal × tangent (направление поперёк)
 *   scale   — длина мировой дуги на единицу u
 */
export function surfaceFrame(u, v, out = {}) {
  const a = u * 0.5;
  const ca = Math.cos(a);
  const sa = Math.sin(a);
  const cu = Math.cos(u);
  const su = Math.sin(u);
  const k = R + v * ca;

  out.pos = (out.pos || new THREE.Vector3()).set(k * cu, v * sa, k * su);

  _dU.set(
    -0.5 * v * sa * cu - k * su,
    0.5 * v * ca,
    -0.5 * v * sa * su + k * cu
  );
  _dV.set(ca * cu, sa, ca * su);

  const scale = _dU.length();
  _tan.copy(_dU).multiplyScalar(1 / scale);
  _nrm.crossVectors(_dU, _dV).normalize();
  _side.crossVectors(_nrm, _tan).normalize();

  out.tangent = (out.tangent || new THREE.Vector3()).copy(_tan);
  out.normal = (out.normal || new THREE.Vector3()).copy(_nrm);
  out.side = (out.side || new THREE.Vector3()).copy(_side);
  out.scale = scale;
  return out;
}

/** Приводит угол к (-π, π]. */
export function wrapPi(x) {
  let a = (x + Math.PI) % LAP;
  if (a < 0) a += LAP;
  return a - Math.PI;
}
