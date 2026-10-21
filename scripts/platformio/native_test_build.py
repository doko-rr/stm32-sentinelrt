Import("env")

from pathlib import Path

project = Path(env.subst("$PROJECT_DIR"))
build = Path(env.subst("$BUILD_DIR"))
source_roots = [
    project / "firmware" / "ports" / "status_indicator",
    project / "firmware" / "ports" / "environmental_sensor",
    project / "firmware" / "domain" / "system_health",
    project / "firmware" / "domain" / "sensor_sample",
    project / "firmware" / "domain" / "dht22_protocol",
    project / "firmware" / "application" / "system_supervisor",
    project / "firmware" / "application" / "sensor_acquisition",
]

env.AppendUnique(CPPPATH=[str(path) for path in source_roots])

for index, source_root in enumerate(source_roots):
    env.BuildSources(
        str(build / "native_logic" / str(index)),
        str(source_root),
        src_filter=["+<*.c>"],
    )
