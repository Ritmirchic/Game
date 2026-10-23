#!/usr/bin/env python3
"""Cross-module consistency audit for the Luau sources.

Parses every server/client module for `ModuleName.Method(` usage, then checks
that ModuleName actually defines Method. Catches the class of mistake that a
syntax check cannot: calls into services or helpers that do not exist.

    python3 tools/audit_refs.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")


def read_tree(folder):
    files = {}
    for base, _dirs, names in os.walk(folder):
        for name in names:
            if name.endswith(".lua"):
                path = os.path.join(base, name)
                with open(path, "r", encoding="utf8") as fh:
                    files[path] = fh.read()
    return files


def module_name(path):
    base = os.path.basename(path)
    for suffix in (".server.lua", ".client.lua", ".lua"):
        if base.endswith(suffix):
            return base[: -len(suffix)]
    return base


def defined_members(src):
    names = set()
    for match in re.finditer(r"function\s+([A-Za-z_]\w*)\.([A-Za-z_]\w*)\s*\(", src):
        names.add((match.group(1), match.group(2)))
    return names


def main():
    shared = read_tree(os.path.join(SRC, "shared"))
    server = read_tree(os.path.join(SRC, "server"))
    client = read_tree(os.path.join(SRC, "client"))
    everything = {}
    everything.update(shared)
    everything.update(server)
    everything.update(client)

    # map of module name -> set of defined members. Shared modules export under
    # an alias (local CM = {} for CombatMath), so every alias is also
    # registered under the file's module name.
    defined = {}
    for path, src in everything.items():
        members = defined_members(src)
        for table, member in members:
            defined.setdefault(table, set()).add(member)
        name = module_name(path)
        for table, member in members:
            defined.setdefault(name, set()).add(member)

    # resolve "registryRef.Foo.Bar" and "Module.Member" patterns
    service_names = {module_name(p) for p in server}
    shared_names = {module_name(p) for p in shared}
    problems = []
    checked = 0

    registry_call = re.compile(r"registryRef\.([A-Za-z_]\w*)\.([A-Za-z_]\w*)\s*\(")
    module_call = re.compile(r"(?<![\w.])([A-Z][A-Za-z_]\w*)\.([A-Za-z_]\w*)\s*\(")

    for path, src in everything.items():
        for service, member in registry_call.finditer(src) and (
            (m.group(1), m.group(2)) for m in registry_call.finditer(src)
        ) or []:
            checked += 1
            if service not in service_names:
                problems.append(f"{os.path.relpath(path, ROOT)}: unknown service {service}")
                continue
            if not source_defines(service, member, everything):
                if member not in {"Init", "Start", "Shutdown"}:
                    problems.append(
                        f"{os.path.relpath(path, ROOT)}: {service}.{member} is not defined"
                    )
        for match in module_call.finditer(src):
            table, member = match.group(1), match.group(2)
            if table not in shared_names:
                continue
            checked += 1
            if table not in defined:
                problems.append(f"{os.path.relpath(path, ROOT)}: {table} defines nothing")
            elif member not in defined[table]:
                problems.append(
                    f"{os.path.relpath(path, ROOT)}: {table}.{member} is not defined"
                )

    print(f"references checked : {checked}")
    if problems:
        seen = set()
        for problem in problems:
            if problem in seen:
                continue
            seen.add(problem)
            print("  - " + problem)
        print(f"problems           : {len(seen)}")
        return 1
    print("problems           : 0")
    return 0


def source_defines(service, member, files):
    for path, src in files.items():
        if module_name(path) == service:
            return re.search(
                r"function\s+%s\.%s\s*\(" % (re.escape(service), re.escape(member)), src
            ) is not None
    return False


if __name__ == "__main__":
    sys.exit(main())
