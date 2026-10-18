from pathlib import Path
import argparse
import re
import sys


OLD_HANDLER = "SysTick_Handler"
NEW_HANDLER = "SentinelRT_FreeRTOS_SysTick_Handler"

OLD_PROTOTYPE = "void xPortSysTickHandler(void);"

NEW_PROTOTYPE = (
    "void SentinelRT_FreeRTOS_SysTick_Handler(void);"
)

CANONICAL_MAPPING = (
    "#define xPortSysTickHandler "
    "SentinelRT_FreeRTOS_SysTick_Handler"
)


def exact_identifier_count(text, identifier):
    pattern = re.compile(
        r"(?<![A-Za-z0-9_])"
        + re.escape(identifier)
        + r"(?![A-Za-z0-9_])"
    )

    return len(pattern.findall(text))


def write_if_changed(path, content):
    previous = path.read_text(
        encoding="utf-8",
        errors="replace",
    )

    if previous == content:
        return False

    path.write_text(
        content,
        encoding="utf-8",
    )

    return True


def patch_port_source(port_source):
    text = port_source.read_text(
        encoding="utf-8",
        errors="replace",
    )

    old_count = exact_identifier_count(
        text,
        OLD_HANDLER,
    )

    new_count = exact_identifier_count(
        text,
        NEW_HANDLER,
    )

    if old_count == 2 and new_count == 0:
        updated = re.sub(
            r"(?<![A-Za-z0-9_])"
            + re.escape(OLD_HANDLER)
            + r"(?![A-Za-z0-9_])",
            NEW_HANDLER,
            text,
        )

        changed = write_if_changed(
            port_source,
            updated,
        )

        return {
            "changed": changed,
            "old_before": old_count,
            "new_before": new_count,
            "old_after": 0,
            "new_after": 2,
        }

    if old_count == 0 and new_count == 2:
        return {
            "changed": False,
            "old_before": old_count,
            "new_before": new_count,
            "old_after": old_count,
            "new_after": new_count,
        }

    raise RuntimeError(
        "Unexpected FreeRTOS port.c handler layout. "
        + "Expected two old identifiers or two new identifiers. "
        + "Old count: "
        + str(old_count)
        + ", new count: "
        + str(new_count)
    )


def patch_freertos_config(config_path):
    text = config_path.read_text(
        encoding="utf-8",
        errors="replace",
    )

    old_prototype_count = text.count(
        OLD_PROTOTYPE
    )

    new_prototype_count = text.count(
        NEW_PROTOTYPE
    )

    if (
        old_prototype_count == 1
        and new_prototype_count == 0
    ):
        text = text.replace(
            OLD_PROTOTYPE,
            NEW_PROTOTYPE,
            1,
        )
    elif not (
        old_prototype_count == 0
        and new_prototype_count == 1
    ):
        raise RuntimeError(
            "Unexpected FreeRTOSConfig prototype layout. "
            + "Old prototype count: "
            + str(old_prototype_count)
            + ", new prototype count: "
            + str(new_prototype_count)
        )

    output_lines = []
    mapping_written = False
    mapping_count_before = 0

    for source_line in text.splitlines():
        stripped_line = source_line.strip()

        if stripped_line.startswith(
            "#define xPortSysTickHandler"
        ):
            mapping_count_before += 1

            if not mapping_written:
                output_lines.append(
                    CANONICAL_MAPPING
                )
                mapping_written = True

            continue

        output_lines.append(source_line)

    if not mapping_written:
        insertion_index = None
        current_index = 0

        for source_line in output_lines:
            if NEW_PROTOTYPE in source_line:
                insertion_index = current_index + 1
                break

            current_index += 1

        if insertion_index is None:
            raise RuntimeError(
                "Unable to locate handler prototype "
                + "for mapping insertion."
            )

        output_lines.insert(
            insertion_index,
            CANONICAL_MAPPING,
        )

    updated_text = "\n".join(output_lines) + "\n"

    changed = write_if_changed(
        config_path,
        updated_text,
    )

    validated_text = config_path.read_text(
        encoding="utf-8",
        errors="replace",
    )

    final_prototype_count = validated_text.count(
        NEW_PROTOTYPE
    )

    final_mapping_count = 0

    for source_line in validated_text.splitlines():
        if source_line.strip() == CANONICAL_MAPPING:
            final_mapping_count += 1

    if final_prototype_count != 1:
        raise RuntimeError(
            "Final handler prototype count is invalid: "
            + str(final_prototype_count)
        )

    if final_mapping_count != 1:
        raise RuntimeError(
            "Final handler mapping count is invalid: "
            + str(final_mapping_count)
        )

    return {
        "changed": changed,
        "old_prototype_before": old_prototype_count,
        "new_prototype_before": new_prototype_count,
        "mapping_before": mapping_count_before,
        "prototype_after": final_prototype_count,
        "mapping_after": final_mapping_count,
    }


def run_patch(project_root, quiet):
    config_path = (
        project_root
        / "Inc"
        / "FreeRTOSConfig.h"
    )

    port_source = (
        project_root
        / "Middlewares"
        / "Third_Party"
        / "FreeRTOS"
        / "Source"
        / "portable"
        / "GCC"
        / "ARM_CM0"
        / "port.c"
    )

    required_files = [
        config_path,
        port_source,
    ]

    missing_files = []

    for required_file in required_files:
        if not required_file.is_file():
            missing_files.append(
                str(required_file)
            )

    if missing_files:
        raise RuntimeError(
            "Required CubeMX FreeRTOS files are missing: "
            + ", ".join(missing_files)
        )

    port_result = patch_port_source(
        port_source
    )

    config_result = patch_freertos_config(
        config_path
    )

    changed = (
        port_result["changed"]
        or config_result["changed"]
    )

    if not quiet:
        print(
            "Patch status:",
            "CHANGED" if changed else "UNCHANGED",
        )
        print(
            "port.c handler:",
            str(port_result["old_after"]),
            "old,",
            str(port_result["new_after"]),
            "new",
        )
        print(
            "FreeRTOSConfig:",
            str(config_result["prototype_after"]),
            "prototype,",
            str(config_result["mapping_after"]),
            "mapping",
        )

    return 0


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Patch CubeMX FreeRTOS SysTick integration "
            "for STM32 SentinelRT."
        )
    )

    parser.add_argument(
        "--project-root",
        default=None,
    )

    parser.add_argument(
        "--quiet",
        action="store_true",
    )

    arguments = parser.parse_args()

    if arguments.project_root:
        project_root = Path(
            arguments.project_root
        ).resolve()
    else:
        project_root = (
            Path(__file__)
            .resolve()
            .parents[2]
        )

    try:
        return run_patch(
            project_root,
            arguments.quiet,
        )
    except Exception as error:
        print(
            "FreeRTOS patch failed:",
            str(error),
            file=sys.stderr,
        )
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
