Import("env")

from pathlib import Path

project = Path(env.subst("$PROJECT_DIR"))
build = Path(env.subst("$BUILD_DIR"))
source_roots = [
    project / "firmware" / "ports" / "status_indicator",
    project / "firmware" / "ports" / "environmental_sensor",
    project / "firmware" / "platform" / "stm32" / "environmental_sensor",
    project / "firmware" / "domain" / "system_health",
    project / "firmware" / "domain" / "sensor_sample",
    project / "firmware" / "domain" / "dht22_protocol",
    project / "firmware" / "domain" / "reset_cause",
    project / "firmware" / "application" / "system_supervisor",
    project / "firmware" / "application" / "sensor_acquisition",
]

env.AppendUnique(CPPPATH=[str(path) for path in source_roots])

for index, source_root in enumerate(source_roots):
    source_filter = ["+<*.c>"]

    # The adapter engine is host-testable, while the concrete HAL binding
    # belongs only to the STM32 firmware target.
    if source_root.name == "environmental_sensor" \
            and source_root.parent.name == "stm32":
        source_filter.append("-<stm32_dht22_hal.c>")

    env.BuildSources(
        str(build / "native_logic" / str(index)),
        str(source_root),
        src_filter=source_filter,
    )
