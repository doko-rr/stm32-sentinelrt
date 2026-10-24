#include "dht22_protocol.h"

#include <stddef.h>

static bool pulse_limits_are_valid(const dht22_pulse_limits_t *limits)
{
    return limits != NULL
        && limits->zero_bit_min_microseconds
            <= limits->zero_bit_max_microseconds
        && limits->zero_bit_max_microseconds
            < limits->one_bit_min_microseconds
        && limits->one_bit_min_microseconds
            <= limits->one_bit_max_microseconds;
}

dht22_pulse_limits_t dht22_protocol_default_pulse_limits(void)
{
    const dht22_pulse_limits_t limits = {
        .zero_bit_min_microseconds = 18U,
        .zero_bit_max_microseconds = 40U,
        .one_bit_min_microseconds = 55U,
        .one_bit_max_microseconds = 85U,
    };
    return limits;
}

dht22_protocol_status_t dht22_protocol_classify_high_pulse(
    uint16_t duration_microseconds,
    const dht22_pulse_limits_t *limits,
    uint8_t *output_bit
)
{
    if (!pulse_limits_are_valid(limits) || output_bit == NULL)
    {
        return DHT22_PROTOCOL_INVALID_ARGUMENT;
    }

    if (duration_microseconds < limits->zero_bit_min_microseconds)
    {
        return DHT22_PROTOCOL_PULSE_TOO_SHORT;
    }

    if (duration_microseconds <= limits->zero_bit_max_microseconds)
    {
        *output_bit = 0U;
        return DHT22_PROTOCOL_OK;
    }

    if (duration_microseconds < limits->one_bit_min_microseconds)
    {
        return DHT22_PROTOCOL_PULSE_AMBIGUOUS;
    }

    if (duration_microseconds <= limits->one_bit_max_microseconds)
    {
        *output_bit = 1U;
        return DHT22_PROTOCOL_OK;
    }

    return DHT22_PROTOCOL_PULSE_TOO_LONG;
}

dht22_protocol_status_t dht22_protocol_pack_bits(
    const uint8_t *bits,
    size_t bit_count,
    uint8_t output_frame[DHT22_PROTOCOL_FRAME_SIZE_BYTES]
)
{
    if (bits == NULL || output_frame == NULL)
    {
        return DHT22_PROTOCOL_INVALID_ARGUMENT;
    }

    if (bit_count != DHT22_PROTOCOL_DATA_BITS)
    {
        return DHT22_PROTOCOL_INVALID_FRAME_SIZE;
    }

    uint8_t candidate[DHT22_PROTOCOL_FRAME_SIZE_BYTES] = {0};

    for (size_t index = 0U; index < bit_count; index++)
    {
        if (bits[index] > 1U)
        {
            return DHT22_PROTOCOL_INVALID_ARGUMENT;
        }

        size_t byte_index = index / 8U;
        candidate[byte_index] = (uint8_t)(
            (uint8_t)(candidate[byte_index] << 1U) | bits[index]
        );
    }

    for (size_t index = 0U; index < DHT22_PROTOCOL_FRAME_SIZE_BYTES; index++)
    {
        output_frame[index] = candidate[index];
    }

    return DHT22_PROTOCOL_OK;
}

dht22_protocol_status_t dht22_protocol_decode_frame(
    const uint8_t *frame,
    size_t frame_size,
    dht22_protocol_reading_t *output_reading
)
{
    if (frame == NULL || output_reading == NULL)
    {
        return DHT22_PROTOCOL_INVALID_ARGUMENT;
    }

    if (frame_size != DHT22_PROTOCOL_FRAME_SIZE_BYTES)
    {
        return DHT22_PROTOCOL_INVALID_FRAME_SIZE;
    }

    uint8_t expected_checksum = (uint8_t)(
        frame[0] + frame[1] + frame[2] + frame[3]
    );
    if (expected_checksum != frame[4])
    {
        return DHT22_PROTOCOL_CHECKSUM_MISMATCH;
    }

    uint16_t raw_humidity = (uint16_t)(
        ((uint16_t)frame[0] << 8U) | frame[1]
    );
    uint16_t raw_temperature = (uint16_t)(
        ((uint16_t)(frame[2] & 0x7FU) << 8U) | frame[3]
    );

    dht22_protocol_reading_t candidate = {
        .temperature_celsius = (float)raw_temperature / 10.0F,
        .relative_humidity_percent = (float)raw_humidity / 10.0F,
    };

    if ((frame[2] & 0x80U) != 0U)
    {
        candidate.temperature_celsius = -candidate.temperature_celsius;
    }

    *output_reading = candidate;
    return DHT22_PROTOCOL_OK;
}

const char *dht22_protocol_status_name(dht22_protocol_status_t status)
{
    switch (status)
    {
        case DHT22_PROTOCOL_OK:
            return "OK";
        case DHT22_PROTOCOL_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case DHT22_PROTOCOL_INVALID_FRAME_SIZE:
            return "INVALID_FRAME_SIZE";
        case DHT22_PROTOCOL_CHECKSUM_MISMATCH:
            return "CHECKSUM_MISMATCH";
        case DHT22_PROTOCOL_PULSE_TOO_SHORT:
            return "PULSE_TOO_SHORT";
        case DHT22_PROTOCOL_PULSE_AMBIGUOUS:
            return "PULSE_AMBIGUOUS";
        case DHT22_PROTOCOL_PULSE_TOO_LONG:
            return "PULSE_TOO_LONG";
        default:
            return "UNKNOWN";
    }
}
