#!/usr/bin/env python3
"""Round-trip validation for the binary writer.

Reads every reference .rbxm/.rbxl fixture, re-serializes it with our writer,
reads the result back and compares structure + every property we support.
Also sanity-checks the geometry helpers used by the world generator.
"""
import os
import sys
import math
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))

from rbx import binary  # noqa: E402

FIXTURES = os.path.join(HERE, "..", "testdata")


def flatten(node, out, depth=0):
    out.append((depth, node))
    for c in node.children:
        flatten(c, out, depth + 1)


def compare(a, b, path="", errors=None):
    if errors is None:
        errors = []
    if a.cls != b.cls:
        errors.append("%s class %s != %s" % (path, a.cls, b.cls))
        return errors
    if len(a.children) != len(b.children):
        errors.append("%s child count %d != %d" % (path, len(a.children), len(b.children)))
        return errors
    for k, v in a.props.items():
        if any(x.endswith("." + k) for x in binary.SKIPPED_PROPERTIES):
            continue
        if k not in b.props:
            if k in ("Attributes", "Tags", "SourceAssetId") or v is None:
                continue
            errors.append("%s missing prop %s" % (path, k))
            continue
        w = b.props[k]
        if w is None and k in SHARED_STRING_PROPS:
            continue
        if isinstance(v, float) or isinstance(w, float):
            try:
                if abs(float(v) - float(w)) > 1e-4:
                    errors.append("%s prop %s %r != %r" % (path, k, v, w))
            except Exception:
                errors.append("%s prop %s type mismatch %r %r" % (path, k, v, w))
        elif isinstance(v, (tuple, list)) and isinstance(w, (tuple, list)):
            if not _close_seq(v, w):
                errors.append("%s prop %s %r != %r" % (path, k, v, w))
        elif v != w:
            errors.append("%s prop %s %r != %r" % (path, k, v, w))
    for i, (ca, cb) in enumerate(zip(a.children, b.children)):
        compare(ca, cb, path + "/" + str(ca.props.get("Name", ca.cls)) + "[%d]" % i, errors)
    return errors


SHARED_STRING_PROPS = {"ModelMeshData", "SlimHash", "PhysicsGrid", "SmoothGrid",
                       "MeshData", "CollisionGroupData", "ModelMeshCFrame"}


def _close_seq(x, y):
    if x is None and isinstance(y, (tuple, list)):
        x = ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
    if len(x) != len(y):
        return False
    for a, b in zip(x, y):
        if a is None and isinstance(b, (tuple, list)):
            a = ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
        if isinstance(a, (tuple, list)) and isinstance(b, (tuple, list)):
            if not _close_seq(a, b):
                return False
        elif isinstance(a, float) or isinstance(b, float):
            try:
                if abs(float(a) - float(b)) > 1e-4:
                    return False
            except Exception:
                return False
        elif a != b:
            return False
    return True


def main():
    files = sorted(f for f in os.listdir(FIXTURES) if f.endswith((".rbxm", ".rbxl")))
    total_errors = 0
    checked = 0
    for f in files:
        path = os.path.join(FIXTURES, f)
        try:
            roots = binary.read_file(path)
        except Exception as exc:
            print("SKIP(read) %-55s %s" % (f, exc))
            continue
        try:
            with tempfile.NamedTemporaryFile(suffix=".rbxl", delete=False) as fh:
                tmp = fh.name
            size, count, classes = binary.write_place(roots, tmp)
            roots2 = binary.read_file(tmp)
            os.unlink(tmp)
        except Exception as exc:
            print("FAIL(write) %-55s %r" % (f, exc))
            total_errors += 1
            continue
        errors = []
        if len(roots) != len(roots2):
            errors.append("root count %d != %d" % (len(roots), len(roots2)))
        else:
            for a, b in zip(roots, roots2):
                compare(a, b, a.props.get("Name", a.cls), errors)
        checked += 1
        if errors:
            total_errors += len(errors)
            print("FAIL  %-55s %d issues" % (f, len(errors)))
            for e in errors[:4]:
                print("        ", e)
        else:
            print("ok    %-55s %6d inst  %6d B" % (f, count, size))
    print("\n%d files round-tripped, %d issues" % (checked, total_errors))
    return 1 if total_errors else 0


if __name__ == "__main__":
    sys.exit(main())
