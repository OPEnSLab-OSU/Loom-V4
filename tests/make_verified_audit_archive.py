"""Make a temporary, full-object audit library from an accepted cold firmware build.

All original Loom and SDK objects are included. Arduino links needed archive members
after directly compiled SDK objects. Cold builds remain the release reference because
unreferenced library initializers may not be pulled from a static archive. This changes no installed source or release package. Headers, platform files,
compiler programs and object bytes must match the accepted build manifest.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from verified_library_cache import digest


def make_archive(cache, source, output):
    manifest = json.loads((cache / "manifest.json").read_text(encoding="utf-8"))
    for name, expected in manifest["inputs"].items():
        if not Path(name).is_file() or digest(Path(name)) != expected:
            raise ValueError(f"Accepted build input changed: {name}")
    objects = []
    for entry in manifest["objects"]:
        relative = Path(entry["path"])
        if relative.is_absolute() or ".." in relative.parts or relative.parts[0] != "libraries":
            raise ValueError("Invalid object path")
        obj = cache / "objects" / relative
        if digest(obj) != entry["sha256"]:
            raise ValueError(f"Accepted build object changed: {obj}")
        objects.append(obj)
    if not objects:
        raise ValueError("No accepted objects")
    compilers = [Path(name) for name in manifest["inputs"] if name.endswith("arm-none-eabi-g++.exe")]
    if len(compilers) != 1:
        raise ValueError("Expected one accepted ARM compiler")
    ar = compilers[0].parent / "arm-none-eabi-ar.exe"
    src = output / "src"
    cpu = src / "cortex-m0plus"
    cpu.mkdir(parents=True, exist_ok=True)
    headers = []
    for header in (source / "src").rglob("*"):
        if header.is_file() and header.suffix in (".h", ".hpp", ".inc", ".inl", ".tpp"):
            relative = header.relative_to(source / "src")
            destination = src / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(header, destination)
            headers.append({"original": str(header.resolve()), "copy": str(destination.resolve()), "sha256": digest(header)})
    # q appends rather than replacing members with the same basename from different SDKs.
    # A response file avoids Windows command-length limits; it is not a shell command.
    response = output / "archive_objects.txt"
    response.write_text("\n".join('"' + path.as_posix() + '"' for path in objects), encoding="utf-8")
    archive = cpu / "libLoom.a"
    if archive.exists():
        raise ValueError("Choose a fresh output directory; an accepted archive is immutable")
    subprocess.run([str(ar), "qc", str(archive), "@" + str(response)], check=True)
    subprocess.run([str(ar), "s", str(archive)], check=True)
    members = subprocess.check_output([str(ar), "t", str(archive)], text=True).splitlines()
    if len(members) != len(objects):
        raise ValueError("Archive member count differs from accepted objects")
    properties = (source / "library.properties").read_text(encoding="utf-8")
    properties += "\nprecompiled=full\n"
    (output / "library.properties").write_text(properties, encoding="utf-8")
    # Direct include paths preserve the accepted SDK header search order. Arduino may
    # still discover and compile SDKs directly referenced by a sketch. Preserve the accepted search order; only Loom headers use our copy.
    folders = []
    includes_file = cache / "include_folders.json"
    if not includes_file.is_file():
        raise ValueError("Accepted ordered include folders missing")
    for folder in json.loads(includes_file.read_text(encoding="utf-8")):
        if Path(folder).resolve() != (source / "src").resolve():
            folders.append(folder)
    result = {"source": str(source.resolve()), "library": str(output.resolve()),
              "archive": str(archive.resolve()), "archive_sha256": digest(archive),
              "members": len(objects), "headers": headers, "include_folders": folders,
              "accepted_build": manifest["options"], "accepted_inputs": manifest["inputs"],
              "method": "Temporary precompiled Loom/SDK objects from the accepted cold build. Needed archive members are linked after directly compiled SDK objects; unreferenced initializers may be omitted. Cold builds are the release reference. No installed source is replaced. Arduino preprocesses, compiles and links each sketch."}
    (output / "audit_archive.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"Created verified audit archive: {archive} ({len(objects)} objects)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--cache", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    make_archive(args.cache, args.source, args.output)
