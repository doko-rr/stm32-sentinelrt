# STM32 SentinelRT

Reliable, non-blocking environmental sensing firmware for the STM32C031C6,
built with FreeRTOS, clean architecture, native tests, and Wokwi runtime
validation.

## Overview

STM32 SentinelRT demonstrates a production-oriented DHT22 acquisition path on
an STM32C0 target. The sensor workflow is asynchronous: the application starts
a measurement, the STM32 HAL binding captures GPIO edges through EXTI, the
adapter reconstructs the 40-bit frame, and the protocol layer validates pulse
widths and the DHT22 checksum before publishing a reading.

The current Wokwi scenario uses:

- Board: ST Nucleo-C031C6
- Sensor: DHT22 on PA4
- Simulated temperature: 24.0 C
- Simulated relative humidity: 55.0 percent
- Serial telemetry: USART2 through PA2 and PA3
- Logic Analyzer: D0 connected to PA4

## Runtime output

A successful acquisition produces output similar to:

```text
SentinelRT sensor runtime started
SENSOR status=VALID seq=1 temp=24.0C humidity=55.0% stack_sensor=124 stack_default=222
```

When the sensor data line is disconnected, the firmware reports `TIMEOUT`
without publishing false environmental data. Reconnecting the sensor restores
valid readings on the next simulation run.

## Architecture

The implementation separates responsibilities across layers:

- **Domain and protocol**: DHT22 pulse classification, frame packing, checksum,
  and decoded environmental values.
- **Application service**: non-blocking acquisition lifecycle, timeout handling,
  latest-sample publication, and recovery.
- **STM32 adapter**: measurement state machine and capture of 40 high pulses.
- **STM32 HAL binding**: GPIO direction changes, microsecond timer access, and
  rising/falling EXTI callbacks.
- **FreeRTOS composition**: task scheduling, runtime telemetry, and stack
  watermark reporting.

The STM32C0 HAL uses separate rising and falling EXTI callbacks. SentinelRT
forwards both callback directions explicitly to the DHT22 adapter and uses the
sensor response-low edge as the synchronization point for data capture.

## Reliability characteristics

- Non-blocking sensor workflow
- Explicit timeout and immediate retry support
- 40-pulse frame completeness requirement
- Pulse-width validation for DHT22 zero and one bits
- DHT22 checksum validation
- Safe unsigned timestamp wraparound handling
- No `printf` calls from interrupt context
- No GPIO level reread after EXTI direction is known
- Stack high-watermark telemetry
- Logic-analyzer VCD files excluded from Git

## Validation status

Stage 3D-6 has been validated with:

- 11 of 11 STM32 DHT22 adapter tests passing
- 58 of 58 native tests passing
- Clean Nucleo-C031C6 firmware build
- Zero compiler warnings and zero compiler errors
- Runtime normal case: `VALID`, 24.0 C, 55.0 percent
- Runtime disconnected case: `TIMEOUT`, no false reading
- Runtime recovery case: valid readings restored
- Final diff audit: 42 of 42 checks passing

Current firmware usage:

- RAM: 8,796 of 12,288 bytes, 71.6 percent
- Flash: 25,536 of 32,768 bytes, 77.9 percent

## Repository layout

```text
Core/                         STM32Cube generated headers and startup support
Drivers/                      STM32 HAL and CMSIS dependencies
Middlewares/                  FreeRTOS middleware
Src/                          STM32 application and generated source
firmware/domain/              Sensor domain and DHT22 protocol logic
firmware/application/         Sensor acquisition use cases
firmware/platform/stm32/      STM32 adapters, HAL bindings, and composition
firmware/rtos/                FreeRTOS integration
include/                      Public firmware interfaces
src/                          Portable implementation sources
test/                         PlatformIO Unity native tests
diagram.json                  Wokwi board, sensor, UART, and analyzer wiring
wokwi.toml                    Wokwi firmware and VCD configuration
platformio.ini                Native-test and STM32 build environments
```

## Prerequisites

- Python 3
- PlatformIO CLI
- Wokwi for VS Code or Wokwi-compatible simulator
- Git

## Build firmware

From the repository root:

```powershell
pio run -e nucleo_c031c6
```

Build artifacts are written under:

```text
.pio/build/nucleo_c031c6/
```

## Run native tests

Run the full native suite:

```powershell
pio test -e native
```

Run only the STM32 DHT22 adapter tests:

```powershell
pio test -e native -f test_stm32_dht22_adapter
```

## Run the Wokwi simulation

1. Build the `nucleo_c031c6` PlatformIO environment.
2. Open the repository in VS Code with Wokwi configured.
3. Start the simulation using `diagram.json` and `wokwi.toml`.
4. Observe serial telemetry and, when needed, inspect PA4 with the Logic
   Analyzer.

The committed diagram represents the normal connected-sensor configuration.
Generated `*.vcd` captures are intentionally ignored by Git.

## Development workflow

Before critical modifications:

1. Create a Git checkpoint or backup diff.
2. Keep domain logic independent from STM32 HAL details.
3. Add or update native regression tests.
4. Run all native tests and a clean firmware build.
5. Validate runtime behavior in Wokwi.
6. Review `git diff --check` and the final staged scope before pushing.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
