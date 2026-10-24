#ifndef SENTINELRT_DHT22_PROTOCOL_H
#define SENTINELRT_DHT22_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DHT22_PROTOCOL_FRAME_SIZE_BYTES 5U
#define DHT22_PROTOCOL_DATA_BITS 40U

typedef enum
{
    DHT22_PROTOCOL_OK = 0,
    DHT22_PROTOCOL_INVALID_ARGUMENT,
    DHT22_PROTOCOL_INVALID_FRAME_SIZE,
    DHT22_PROTOCOL_CHECKSUM_MISMATCH,
    DHT22_PROTOCOL_PULSE_TOO_SHORT,
    DHT22_PROTOCOL_PULSE_AMBIGUOUS,
    DHT22_PROTOCOL_PULSE_TOO_LONG
} dht22_protocol_status_t;

/**
 * Decoded DHT22 measurements in explicit physical units.
 *
 * The protocol layer only converts a verified sensor frame. Deployment range
 * validation remains in sensor_sample so all adapters share the same rules.
 */
typedef struct
{
    float temperature_celsius;
    float relative_humidity_percent;
} dht22_protocol_reading_t;

/**
 * Thresholds for classifying the HIGH portion of a DHT22 data pulse.
 *
 * Values below zero_bit_min or above one_bit_max are rejected. The gap
 * between zero_bit_max and one_bit_min is deliberately ambiguous so timing
 * noise cannot silently turn into a valid bit.
 */
typedef struct
{
    uint16_t zero_bit_min_microseconds;
    uint16_t zero_bit_max_microseconds;
    uint16_t one_bit_min_microseconds;
    uint16_t one_bit_max_microseconds;
} dht22_pulse_limits_t;

/** Returns conservative timing limits suitable for initial adapter tests. */
dht22_pulse_limits_t dht22_protocol_default_pulse_limits(void);

/**
 * Classifies one HIGH pulse as bit 0 or bit 1.
 *
 * output_bit is modified only when the function returns DHT22_PROTOCOL_OK.
 */
dht22_protocol_status_t dht22_protocol_classify_high_pulse(
    uint16_t duration_microseconds,
    const dht22_pulse_limits_t *limits,
    uint8_t *output_bit
);

/**
 * Converts exactly 40 classified bits into the five-byte DHT22 frame.
 *
 * Bits are expected most-significant first, matching transmission order.
 * output_frame remains unchanged when validation fails.
 */
dht22_protocol_status_t dht22_protocol_pack_bits(
    const uint8_t *bits,
    size_t bit_count,
    uint8_t output_frame[DHT22_PROTOCOL_FRAME_SIZE_BYTES]
);

/**
 * Validates checksum and decodes humidity and signed temperature.
 *
 * output_reading remains unchanged on every failure path. This prevents a
 * previous trusted measurement from being replaced by a partial frame.
 */
dht22_protocol_status_t dht22_protocol_decode_frame(
    const uint8_t *frame,
    size_t frame_size,
    dht22_protocol_reading_t *output_reading
);

const char *dht22_protocol_status_name(dht22_protocol_status_t status);

#endif
