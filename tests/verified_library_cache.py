"""Optional local audit cache. Never reuse an object with changed inputs or header resolution.

Arduino CLI clears every library object when include-folder order changes. This hook restores
only objects whose source/header hashes, board options and header search winners are unchanged.
It runs after Arduino's library discovery. Arduino still compiles sketches, checks dependencies,
builds new/changed libraries and links every firmware. Cold audits need no hook.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def include_folders(build):
    entries = json.loads((build / "includes.cache").read_text(encoding="utf-8"))
    return [Path(entry["added_include_path"]).resolve() for entry in entries
            if "added_include_path" in entry]


def header_winners(folders):
    # Compare the first matching file for every header spelling, including nested paths.
    # Header-relative quoted includes have the same source directory in both builds.
    winners = {}
    for folder in folders:
        for path in folder.rglob("*"):
            if path.is_file() and path.suffix.lower() in (".h", ".hpp", ".hh", ".inc", ".inl", ".tpp", ".def", ".c", ".cpp"):
                name = path.relative_to(folder).as_posix().lower()
                winners.setdefault(name, str(path.resolve()).lower())
    return winners


def options(build):
    return json.loads((build / "build.options.json").read_text(encoding="utf-8"))


def dependencies(depfile):
    # GCC emits escaped spaces and continuation lines. Drive-letter colons are not separators.
    text = depfile.read_text(encoding="utf-8").replace("\\\n", " ")
    _, text = text.split(": ", 1)
    marker = "\x01"
    tokens = text.replace("\\ ", marker).split()
    paths = [Path(token.replace(marker, " ")) for token in tokens]
    if not paths or not all(path.is_file() for path in paths):
        raise ValueError("Unrecognized/missing dependency; leave this object for Arduino to rebuild")
    return paths


def platform_inputs(configuration):
    inputs = []
    for folder in configuration["hardwareFolders"].split(","):
        for name in ("platform.txt", "platform.local.txt", "boards.txt", "boards.local.txt"):
            path = Path(folder) / name
            if path.is_file():
                inputs.append(path)
    return inputs


def save(build, cache):
    configuration = options(build)
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    tools = set()
    for command in commands:
        executable = Path(command["arguments"][0])
        if not executable.is_file():
            executable = executable.with_suffix(".exe")
        if not executable.is_file():
            raise ValueError("Compiler executable missing; cannot save verified cache")
        tools.add(executable)
        # GCC delegates parsing to these programs; include them in compiler identity.
        tool_root = executable.parent.parent
        tools.update(tool_root.glob("libexec/**/cc1*.exe"))
    inputs = {str(path.resolve()): digest(path) for path in platform_inputs(configuration) + list(tools)}
    objects = []
    for obj in (build / "libraries").rglob("*.o"):
        depfile = obj.with_suffix(".d")
        if not depfile.is_file():
            continue
        try:
            for path in dependencies(depfile):
                inputs[str(path.resolve())] = digest(path)
        except (ValueError, OSError):
            continue
        relative = obj.relative_to(build)
        destination = cache / "objects" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(obj, destination)
        shutil.copy2(depfile, destination.with_suffix(".d"))
        objects.append({"path": relative.as_posix(), "sha256": digest(obj), "dep_sha256": digest(depfile)})
    manifest = {"options": configuration, "headers": header_winners(include_folders(build)),
                "inputs": inputs, "objects": objects}
    cache.mkdir(parents=True, exist_ok=True)
    (cache / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    (cache / "include_folders.json").write_text(
        json.dumps([str(folder) for folder in include_folders(build)], indent=2), encoding="utf-8")
    print(f"Saved {len(objects)} verified library objects")


def restore(build, cache):
    manifest_path = cache / "manifest.json"
    if not manifest_path.is_file():
        print("No verified cache yet; Arduino will perform a cold library build")
        return
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if options(build) != manifest["options"]:
        print("Build options changed; Arduino will rebuild libraries")
        return
    current_headers = header_winners(include_folders(build))
    # New libraries may add new names. Existing names must still resolve to the same file.
    if any(current_headers.get(name) != path for name, path in manifest["headers"].items()):
        print("Header resolution changed; Arduino will rebuild libraries")
        return
    for name, expected in manifest["inputs"].items():
        path = Path(name)
        if not path.is_file() or digest(path) != expected:
            print(f"Build input changed; Arduino will rebuild libraries: {path}")
            return
    restored = 0
    for entry in manifest["objects"]:
        relative = Path(entry["path"])
        if relative.is_absolute() or ".." in relative.parts or relative.parts[0] != "libraries":
            raise ValueError("Invalid cache object path")
        source = cache / "objects" / relative
        depfile = source.with_suffix(".d")
        if digest(source) != entry["sha256"] or digest(depfile) != entry["dep_sha256"]:
            raise ValueError("Cache product changed")
        destination = build / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        shutil.copy2(depfile, destination.with_suffix(".d"))
        restored += 1
    print(f"Restored {restored} verified library objects; Arduino still compiles and links the sketch")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("save", "restore"))
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--cache", required=True, type=Path)
    args = parser.parse_args()
    {"save": save, "restore": restore}[args.action](args.build, args.cache)
