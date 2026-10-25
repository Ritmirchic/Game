// Проверки математики мира. Запуск: node --test test/
import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import { HALF_W, LAP, surfacePoint, surfaceFrame, wrapPi } from '../src/mobius.js';

const near = (a, b, eps = 1e-9) => Math.abs(a - b) < eps;

test('репер поверхности ортонормирован', () => {
  for (let i = 0; i < 200; i++) {
    const u = (i / 200) * LAP * 3;
    const v = -HALF_W + (i % 7) / 6 * HALF_W * 2;
    const f = surfaceFrame(u, v, {});
    assert.ok(near(f.tangent.length(), 1, 1e-9));
    assert.ok(near(f.normal.length(), 1, 1e-9));
    assert.ok(near(f.side.length(), 1, 1e-9));
    assert.ok(Math.abs(f.tangent.dot(f.normal)) < 1e-9);
    assert.ok(Math.abs(f.tangent.dot(f.side)) < 1e-9);
    assert.ok(Math.abs(f.normal.dot(f.side)) < 1e-9);
    assert.ok(f.scale > 5 && f.scale < 13);
  }
});

test('координата v измеряется в мировых единицах', () => {
  const a = surfacePoint(1.23, 0.5);
  const b = surfacePoint(1.23, 1.5);
  assert.ok(near(a.distanceTo(b), 1, 1e-9));
});

test('после витка нормаль перевёрнута в той же точке мира', () => {
  for (let i = 0; i < 50; i++) {
    const u = (i / 50) * LAP;
    const v = (i % 5) - 2;
    const p1 = surfacePoint(u, v);
    const p2 = surfacePoint(u + LAP, -v); // та же физическая точка
    assert.ok(p1.distanceTo(p2) < 1e-9, 'точка не совпала');
    const n1 = surfaceFrame(u, v, {}).normal;
    const n2 = surfaceFrame(u + LAP, -v, {}).normal;
    assert.ok(n1.dot(n2) < -0.999999, 'нормаль не перевернулась');
  }
});

test('матрица репера — собственное вращение при любом знаке верха', () => {
  for (const upSign of [1, -1]) {
    for (let i = 0; i < 20; i++) {
      const u = (i / 20) * LAP;
      const v = 1.1 - (i % 5) * 0.5;
      const f = surfaceFrame(u, v, {});
      const up = f.normal.clone().multiplyScalar(upSign);
      const fwd = upSign > 0 ? f.tangent.clone() : f.tangent.clone().negate();
      const m = new THREE.Matrix4().makeBasis(f.side, up, fwd);
      assert.ok(near(m.determinant(), 1, 1e-9), 'det != +1 (зеркальная матрица)');
      const q = new THREE.Quaternion().setFromRotationMatrix(m);
      const back = new THREE.Matrix4().makeRotationFromQuaternion(q);
      for (let k = 0; k < 16; k++) {
        assert.ok(Math.abs(back.elements[k] - m.elements[k]) < 1e-9, 'кватернион испортил базис');
      }
    }
  }
});

test('плоская надпись остаётся на поверхности и не зеркалится', () => {
  for (const readFrom of [1, -1]) {
    const u = 2.2;
    const v = 0.4;
    const f = surfaceFrame(u, v, {});
    const xAxis = readFrom > 0 ? f.side.clone().negate() : f.side.clone();
    const zAxis = readFrom > 0 ? f.normal.clone() : f.normal.clone().negate();
    const m = new THREE.Matrix4().makeBasis(xAxis, f.tangent.clone(), zAxis);
    assert.ok(near(m.determinant(), 1, 1e-9), 'надпись зеркальная');
    // «верх» букв — по направлению бега, лицевая сторона — нужная нормаль
    const up = new THREE.Vector3(0, 1, 0).applyMatrix4(new THREE.Matrix4().extractRotation(m));
    assert.ok(up.dot(f.tangent) > 0.999999);
    const face = new THREE.Vector3(0, 0, 1).applyMatrix4(new THREE.Matrix4().extractRotation(m));
    assert.ok(face.dot(f.normal.clone().multiplyScalar(readFrom)) > 0.999999);
  }
});

test('wrapPi', () => {
  assert.ok(near(wrapPi(0), 0));
  assert.ok(near(wrapPi(Math.PI * 1.5), -Math.PI * 0.5, 1e-12));
  assert.ok(near(wrapPi(-Math.PI * 1.5), Math.PI * 0.5, 1e-12));
});

test('прыжок перекрывает окно разлома', () => {
  const GRAV = 14;
  const JUMP_V = 5.25;
  const RIFT_H = 0.55;
  const SPEED = 3.15;
  // высота h(t) = v0 t - g t²/2
  const h = (t) => JUMP_V * t - (GRAV * t * t) / 2;
  let t1 = null;
  let t2 = null;
  for (let t = 0; t < 1.2; t += 1e-4) {
    if (t1 === null && h(t) > RIFT_H) t1 = t;
    if (t1 !== null && t2 === null && h(t) < RIFT_H) t2 = t;
  }
  const window = (t2 - t1) * SPEED;
  assert.ok(window > 1.0, `окно прыжка слишком узкое: ${window.toFixed(2)} м`);
  assert.ok(h(JUMP_V / GRAV) > RIFT_H + 0.25, 'апекс слишком низкий');
});
