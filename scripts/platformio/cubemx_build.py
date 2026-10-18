Import("env")

from pathlib import Path
import subprocess


PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))

PATCH_SCRIPT = (
    PROJECT_DIR
    / "scripts"
    / "platformio"
    / "patch_cubemx_freertos.py"
)

if not PATCH_SCRIPT.is_file():
    raise RuntimeError(
        "CubeMX FreeRTOS patch script was not found"
    )

patch_process = subprocess.run(
    [
        env.subst("$PYTHONEXE"),
        str(PATCH_SCRIPT),
        "--project-root",
        str(PROJECT_DIR),
        "--quiet",
    ],
    cwd=str(PROJECT_DIR),
    text=True,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
)

if patch_process.returncode != 0:
    raise RuntimeError(
        "CubeMX FreeRTOS pre-build patch failed: "
        + patch_process.stdout.strip()
    )
BUILD_DIR = Path(env.subst("$BUILD_DIR"))

FREERTOS_SOURCE = (
    PROJECT_DIR
    / "Middlewares"
    / "Third_Party"
    / "FreeRTOS"
    / "Source"
)

BSP_ROOT = PROJECT_DIR / "Drivers" / "BSP"

CMSIS_OS2_INCLUDE = Path(
    r"C:\Projects\stm32-sentinelrt\Drivers\CMSIS\RTOS2\Include"
)

PORTABLE_DIR = Path(
    r"C:\Projects\stm32-sentinelrt\Middlewares\Third_Party\FreeRTOS\Source\portable\GCC\ARM_CM0"
)


def append_unique_include_paths(paths):
    unique_paths = []
    known_paths = set()

    for path in paths:
        if not path.is_dir():
            continue

        normalized = str(path.resolve()).lower()

        if normalized in known_paths:
            continue

        known_paths.add(normalized)
        unique_paths.append(str(path))

    env.Append(CPPPATH=unique_paths)


include_paths = [
    PROJECT_DIR / "Inc",
    PROJECT_DIR / "Drivers" / "CMSIS" / "Include",
    (
        PROJECT_DIR
        / "Drivers"
        / "CMSIS"
        / "Device"
        / "ST"
        / "STM32C0xx"
        / "Include"
    ),
    (
        PROJECT_DIR
        / "Drivers"
        / "STM32C0xx_HAL_Driver"
        / "Inc"
    ),
    (
        PROJECT_DIR
        / "Drivers"
        / "STM32C0xx_HAL_Driver"
        / "Inc"
        / "Legacy"
    ),
    FREERTOS_SOURCE / "include",
    FREERTOS_SOURCE / "CMSIS_RTOS_V2",
    CMSIS_OS2_INCLUDE,
    PORTABLE_DIR,
]

for header in BSP_ROOT.rglob("*.h"):
    include_paths.append(header.parent)

append_unique_include_paths(include_paths)


rtos_build_dir = BUILD_DIR / "cubemx_freertos"

env.BuildSources(
    str(rtos_build_dir / "kernel"),
    str(FREERTOS_SOURCE),
    src_filter=[
        "+<*.c>",
    ],
)

env.BuildSources(
    str(rtos_build_dir / "cmsis_rtos2"),
    str(FREERTOS_SOURCE / "CMSIS_RTOS_V2"),
    src_filter=[
        "+<*.c>",
    ],
)

env.BuildSources(
    str(rtos_build_dir / "portable"),
    str(PORTABLE_DIR),
    src_filter=[
        "+<*.c>",
    ],
)

heap_source = (
    FREERTOS_SOURCE
    / "portable"
    / "MemMang"
    / "heap_4.c"
)

env.BuildSources(
    str(rtos_build_dir / "heap"),
    str(heap_source.parent),
    src_filter=[
        "+<heap_4.c>",
    ],
)


bsp_source_directories = set()

for source in BSP_ROOT.rglob("*.c"):
    directory_text = str(source.parent).lower()

    if "stm32c0" in directory_text:
        bsp_source_directories.add(source.parent)

ordered_bsp_directories = sorted(
    bsp_source_directories,
    key=lambda path: str(path).lower(),
)

for index, source_directory in enumerate(
    ordered_bsp_directories
):
    env.BuildSources(
        str(BUILD_DIR / "cubemx_bsp" / str(index)),
        str(source_directory),
        src_filter=[
            "+<*.c>",
        ],
    )

STARTUP_SOURCE = PROJECT_DIR / "startup_stm32c031xx.s"

if not STARTUP_SOURCE.is_file():
    raise RuntimeError("STM32 startup assembly file not found")

env.BuildSources(
    str(BUILD_DIR / "cubemx_startup"),
    str(PROJECT_DIR),
    src_filter=[
        "+<startup_stm32c031xx.s>",
    ],
)
