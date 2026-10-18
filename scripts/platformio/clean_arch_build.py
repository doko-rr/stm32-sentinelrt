Import("env")
from pathlib import Path
project = Path(env.subst("$PROJECT_DIR"))
build = Path(env.subst("$BUILD_DIR"))
firmware = project / "firmware"
include_paths = []
source_dirs = set()
for header in firmware.rglob("*.h"):
    include_paths.append(str(header.parent))
for source in firmware.rglob("*.c"):
    source_dirs.add(source.parent)
env.AppendUnique(CPPPATH=include_paths)
for index, directory in enumerate(sorted(source_dirs, key=lambda item: str(item).lower())):
    env.BuildSources(str(build / "sentinelrt" / str(index)), str(directory), src_filter=["+<*.c>"])
