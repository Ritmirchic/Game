import test from 'node:test';
import assert from 'node:assert/strict';

import { LEVELS } from '../src/levels.js';
import {
  circleIntersectsRect,
  constellationIdentity,
  hazardPosition,
  isGateOpen,
  isPlateActive,
  moveCircle,
  recordingLength,
  sampleEchoRecording,
  sampleRecording,
} from '../src/logic.js';

const byId = (id) => LEVELS.find((level) => level.id === id);

test('echo playback interpolates a recording and holds its final position', () => {
  const recording = [
    { t: 0, x: 0, y: 0 },
    { t: 1, x: 10, y: 20 },
  ];
  assert.deepEqual(sampleRecording(recording, 0.5), { x: 5, y: 10, ended: false });
  assert.deepEqual(sampleRecording(recording, 4), { x: 10, y: 20, ended: true });
  assert.equal(sampleRecording([], 1), null);
});

test('reverse echo starts at the recorded ending and preserves recorded waiting time', () => {
  const recording = [
    { t: 0, x: 0, y: 0 },
    { t: 1, x: 10, y: 0 },
    { t: 4, x: 10, y: 0 },
  ];
  const plate = { x: 10, y: 0, r: 2 };
  assert.equal(sampleEchoRecording(recording, 0, 4, true).x, 10);
  assert.equal(isPlateActive(plate, [sampleEchoRecording(recording, 2, 4, true)], 0), true);
  assert.equal(sampleEchoRecording(recording, 3.5, 4, true).x, 5);
  assert.equal(sampleEchoRecording(recording, 5, 4, true).x, 0);
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
  const level = byId('remain');
  const gate = level.gates[0];
  const plate = level.plates[0];
  assert.equal(isGateOpen(gate, level.plates, [{ x: 700, y: 300 }]), false);
  assert.equal(isGateOpen(gate, level.plates, [{ x: 700, y: 300 }, plate]), true);
});

test('chapter II requires two past selves and the final chapter reverses who can finish', () => {
  const level = byId('trust');
  const gate = level.gates[0];
  assert.equal(isGateOpen(gate, level.plates, [level.plates[0]]), false);
  assert.equal(isGateOpen(gate, level.plates, level.plates), true);
  assert.equal(byId('release').echoOnly, true);
  assert.equal(LEVELS.length, 6);
});

test('all important points and moving hazard endpoints are outside solid walls', () => {
  for (const level of LEVELS) {
    const hazardPoints = level.hazards.flatMap((hazard) => [hazard.from, hazard.to]);
    for (const point of [level.spawn, level.core, ...level.plates, ...level.portals, ...hazardPoints]) {
      for (const wall of level.walls) {
        assert.equal(circleIntersectsRect(point.x, point.y, 12, wall), false, `${level.id}: point inside wall`);
      }
    }
  }
});

test('portals are paired, reciprocal and lead across the otherwise solid divider', () => {
  const level = byId('link');
  const [a, b] = level.portals;
  assert.equal(a.pair, b.id);
  assert.equal(b.pair, a.id);
  assert.ok(a.x < 320 && b.x > 348);
  assert.equal(isGateOpen(level.gates[0], level.plates, [b]), false);
  assert.equal(isGateOpen(level.gates[0], level.plates, [level.plates[0]]), true);
});

test('silence moves deterministically and returns along the same path', () => {
  const hazard = { from: { x: 0, y: 0 }, to: { x: 100, y: 0 }, speed: 10, phase: 0 };
  assert.deepEqual(hazardPosition(hazard, 5), { x: 50, y: 0 });
  assert.deepEqual(hazardPosition(hazard, 10), { x: 100, y: 0 });
  assert.deepEqual(hazardPosition(hazard, 15), { x: 50, y: 0 });
  assert.deepEqual(hazardPosition(hazard, 20), { x: 0, y: 0 });
});

test('constellation identity is deterministic and changes with the player path', () => {
  const archive = [[[{ t: 0, x: 10, y: 10 }, { t: 1, x: 80, y: 90 }]], [], [], [], [], []];
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
    let teleports = 0;
    let arrivalLock = null;
    const closedGates = level.gates.filter((gate) => !isGateOpen(gate, level.plates, activeEntities));
    const obstacles = [...level.walls, ...closedGates];

    for (const waypoint of waypoints) {
      let steps = 0;
      let teleported = false;
      while (Math.hypot(waypoint.x - position.x, waypoint.y - position.y) > 4 && steps < 2500) {
        const dx = waypoint.x - position.x;
        const dy = waypoint.y - position.y;
        const length = Math.max(0.0001, Math.hypot(dx, dy));
        const amount = Math.min(238 / 60, length);
        position = moveCircle(position, dx / length * amount, dy / length * amount, 12, obstacles);
        steps += 1;

        if (arrivalLock) {
          const arrival = level.portals.find((candidate) => candidate.id === arrivalLock);
          if (arrival && Math.hypot(arrival.x - position.x, arrival.y - position.y) < arrival.r + 12 + 6) continue;
          arrivalLock = null;
        }
        const portal = level.portals.find((candidate) =>
          Math.hypot(candidate.x - position.x, candidate.y - position.y) < candidate.r + 12 - 3);
        if (portal) {
          arrivalLock = portal.pair;
          position = { ...level.portals.find((candidate) => candidate.id === portal.pair) };
          teleports += 1;
          teleported = true;
          break;
        }
      }
      if (!teleported) {
        assert.ok(Math.hypot(waypoint.x - position.x, waypoint.y - position.y) <= 5,
          `${level.id}: could not reach ${JSON.stringify(waypoint)}; stopped at ${JSON.stringify(position)}`);
      }
    }
    return { position, teleports };
  }

  const remain = byId('remain');
  travel(remain, [remain.plates[0]]);
  travel(remain, [remain.core], [remain.plates[0]]);

  const trust = byId('trust');
  travel(trust, [trust.plates[0]]);
  travel(trust, [trust.plates[1]]);
  travel(trust, [
    { x: 190, y: 240 },
    { x: 350, y: 240 },
    { x: 450, y: 300 },
    trust.core,
  ], trust.plates);

  const link = byId('link');
  const recordingRoute = travel(link, [link.portals[0], link.plates[0]]);
  assert.equal(recordingRoute.teleports, 1);
  const exitRoute = travel(link, [link.portals[0], { x: 470, y: 250 }, link.core], link.plates);
  assert.equal(exitRoute.teleports, 1);

  const reverse = byId('reverse');
  travel(reverse, [reverse.plates[0]]);
  travel(reverse, [{ x: 450, y: 300 }, reverse.core], [reverse.plates[0]]);

  const silence = byId('silence');
  travel(silence, [silence.plates[0]]);
  travel(silence, [
    { x: 450, y: 300 },
    { x: 570, y: 300 },
    { x: 650, y: 220 },
    silence.core,
  ], [silence.plates[0]]);

  const release = byId('release');
  travel(release, [release.core]);
});
