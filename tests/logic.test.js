import test from 'node:test';
import assert from 'node:assert/strict';

import { LEVELS } from '../src/levels.js';
import {
  circleIntersectsRect,
  constellationIdentity,
  isGateOpen,
  isPlateActive,
  moveCircle,
  recordingLength,
  sampleRecording,
} from '../src/logic.js';

test('echo playback interpolates a recording and holds its final position', () => {
  const recording = [
    { t: 0, x: 0, y: 0 },
    { t: 1, x: 10, y: 20 },
  ];
  assert.deepEqual(sampleRecording(recording, 0.5), { x: 5, y: 10, ended: false });
  assert.deepEqual(sampleRecording(recording, 4), { x: 10, y: 20, ended: true });
  assert.equal(sampleRecording([], 1), null);
});

test('an echo can hold a pressure plate while the player is elsewhere', () => {
  const plate = { x: 100, y: 100, r: 30 };
  const player = { x: 700, y: 100 };
  const echo = { x: 104, y: 98 };
  assert.equal(isPlateActive(plate, [player]), false);
  assert.equal(isPlateActive(plate, [player, echo]), true);
});

test('a double gate opens only when both plates are occupied', () => {
  const gate = { x: 400, y: 200, w: 20, h: 100, plates: ['a', 'b'] };
  const plates = [
    { id: 'a', x: 100, y: 100, r: 30 },
    { id: 'b', x: 100, y: 400, r: 30 },
  ];
  assert.equal(isGateOpen(gate, plates, [{ x: 100, y: 100 }]), false);
  assert.equal(isGateOpen(gate, plates, [{ x: 100, y: 100 }, { x: 100, y: 400 }]), true);
});

test('a closing gate cannot crush someone in its doorway', () => {
  const gate = { x: 400, y: 200, w: 20, h: 100, plates: ['a'] };
  const plates = [{ id: 'a', x: 100, y: 100, r: 30 }];
  assert.equal(isGateOpen(gate, plates, [{ x: 410, y: 250 }]), true);
});

test('circle movement collides with walls without passing through them', () => {
  const wall = { x: 50, y: 0, w: 20, h: 100 };
  assert.equal(circleIntersectsRect(45, 50, 10, wall), true);
  assert.deepEqual(moveCircle({ x: 30, y: 50 }, 15, 0, 10, [wall]), { x: 30, y: 50 });
  assert.deepEqual(moveCircle({ x: 30, y: 50 }, 5, 10, 10, [wall]), { x: 35, y: 60 });
});

test('chapter I is solved by leaving an echo on the plate', () => {
  const level = LEVELS[0];
  const gate = level.gates[0];
  const plate = level.plates[0];
  assert.equal(isGateOpen(gate, level.plates, [{ x: 700, y: 300 }]), false);
  assert.equal(isGateOpen(gate, level.plates, [{ x: 700, y: 300 }, plate]), true);
});

test('chapter II requires two past selves and chapter III reverses who can finish', () => {
  const level = LEVELS[1];
  const gate = level.gates[0];
  assert.equal(isGateOpen(gate, level.plates, [level.plates[0]]), false);
  assert.equal(isGateOpen(gate, level.plates, level.plates), true);
  assert.equal(LEVELS[2].echoOnly, true);
});

test('all spawn points, plates and cores are outside solid walls', () => {
  for (const level of LEVELS) {
    for (const point of [level.spawn, level.core, ...level.plates]) {
      for (const wall of level.walls) {
        assert.equal(circleIntersectsRect(point.x, point.y, 12, wall), false, `${level.id}: point inside wall`);
      }
    }
  }
});

test('constellation identity is deterministic and changes with the player path', () => {
  const archive = [[[{ t: 0, x: 10, y: 10 }, { t: 1, x: 80, y: 90 }]], [], []];
  const first = constellationIdentity(archive);
  const second = constellationIdentity(archive);
  assert.deepEqual(first, second);
  assert.match(first.code, /^[0-9A-F]{6}$/);
  archive[0][0][1].x += 20;
  assert.notEqual(constellationIdentity(archive).hash, first.hash);
});

test('recording length measures real movement, not waiting time', () => {
  const recording = [
    { t: 0, x: 0, y: 0 },
    { t: 4, x: 0, y: 0 },
    { t: 5, x: 3, y: 4 },
  ];
  assert.equal(recordingLength(recording), 5);
});

test('the authored solution route in every chapter is physically traversable', () => {
  function travel(level, waypoints, activeEntities = []) {
    let position = { ...level.spawn };
    const closedGates = level.gates.filter((gate) => !isGateOpen(gate, level.plates, activeEntities));
    const obstacles = [...level.walls, ...closedGates];

    for (const waypoint of waypoints) {
      let steps = 0;
      while (Math.hypot(waypoint.x - position.x, waypoint.y - position.y) > 4 && steps < 2500) {
        const dx = waypoint.x - position.x;
        const dy = waypoint.y - position.y;
        const length = Math.max(0.0001, Math.hypot(dx, dy));
        const amount = Math.min(238 / 60, length);
        position = moveCircle(position, dx / length * amount, dy / length * amount, 12, obstacles);
        steps += 1;
      }
      assert.ok(Math.hypot(waypoint.x - position.x, waypoint.y - position.y) <= 5,
        `${level.id}: could not reach ${JSON.stringify(waypoint)}; stopped at ${JSON.stringify(position)}`);
    }
  }

  travel(LEVELS[0], [LEVELS[0].plates[0]]);
  travel(LEVELS[0], [LEVELS[0].core], [LEVELS[0].plates[0]]);

  travel(LEVELS[1], [LEVELS[1].plates[0]]);
  travel(LEVELS[1], [LEVELS[1].plates[1]]);
  travel(LEVELS[1], [
    { x: 190, y: 240 },
    { x: 350, y: 240 },
    { x: 450, y: 300 },
    LEVELS[1].core,
  ], LEVELS[1].plates);

  travel(LEVELS[2], [LEVELS[2].core]);
});
