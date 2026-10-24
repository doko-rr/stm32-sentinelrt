#include <stddef.h>
#include <stdint.h>
#include <unity.h>
#include "dht22_protocol.h"

void setUp(void) {}
void tearDown(void) {}

static void make_bits(const uint8_t frame[5], uint8_t bits[40])
{
    for (size_t byte_index = 0U; byte_index < 5U; byte_index++)
    {
        for (size_t bit_index = 0U; bit_index < 8U; bit_index++)
        {
            bits[(byte_index * 8U) + bit_index] = (uint8_t)(
                (frame[byte_index] >> (7U - bit_index)) & 1U
            );
        }
    }
}

static void test_default_limits_are_ordered(void)
{
    dht22_pulse_limits_t limits =
        dht22_protocol_default_pulse_limits();

    /*
     * Explicit boolean comparisons keep the timing contract readable and
     * avoid confusion over expected/actual ordering in assertion macros.
     */
    TEST_ASSERT_TRUE(
        limits.zero_bit_min_microseconds
        <= limits.zero_bit_max_microseconds
    );
    TEST_ASSERT_TRUE(
        limits.zero_bit_max_microseconds
        < limits.one_bit_min_microseconds
    );
    TEST_ASSERT_TRUE(
        limits.one_bit_min_microseconds
        <= limits.one_bit_max_microseconds
    );
}

static void test_short_high_pulse_is_bit_zero(void)
{
    dht22_pulse_limits_t limits = dht22_protocol_default_pulse_limits();
    uint8_t bit = 9U;
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_OK, dht22_protocol_classify_high_pulse(28U, &limits, &bit));
    TEST_ASSERT_EQUAL_UINT8(0U, bit);
}

static void test_long_high_pulse_is_bit_one(void)
{
    dht22_pulse_limits_t limits = dht22_protocol_default_pulse_limits();
    uint8_t bit = 9U;
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_OK, dht22_protocol_classify_high_pulse(70U, &limits, &bit));
    TEST_ASSERT_EQUAL_UINT8(1U, bit);
}

static void test_too_short_pulse_preserves_output(void)
{
    dht22_pulse_limits_t limits = dht22_protocol_default_pulse_limits();
    uint8_t bit = 9U;
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_PULSE_TOO_SHORT, dht22_protocol_classify_high_pulse(10U, &limits, &bit));
    TEST_ASSERT_EQUAL_UINT8(9U, bit);
}

static void test_ambiguous_pulse_is_rejected(void)
{
    dht22_pulse_limits_t limits = dht22_protocol_default_pulse_limits();
    uint8_t bit = 9U;
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_PULSE_AMBIGUOUS, dht22_protocol_classify_high_pulse(48U, &limits, &bit));
    TEST_ASSERT_EQUAL_UINT8(9U, bit);
}

static void test_too_long_pulse_is_rejected(void)
{
    dht22_pulse_limits_t limits = dht22_protocol_default_pulse_limits();
    uint8_t bit = 9U;
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_PULSE_TOO_LONG, dht22_protocol_classify_high_pulse(100U, &limits, &bit));
}

static void test_invalid_limits_are_rejected(void)
{
    dht22_pulse_limits_t limits = {40U, 20U, 55U, 85U};
    uint8_t bit = 9U;
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_INVALID_ARGUMENT, dht22_protocol_classify_high_pulse(28U, &limits, &bit));
}

static void test_pack_bits_reconstructs_frame(void)
{
    const uint8_t expected[5] = {0x02U, 0xD0U, 0x01U, 0x09U, 0xDCU};
    uint8_t bits[40] = {0};
    uint8_t actual[5] = {0};
    make_bits(expected, bits);
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_OK, dht22_protocol_pack_bits(bits, 40U, actual));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, 5U);
}

static void test_pack_bits_rejects_incomplete_frame(void)
{
    uint8_t bits[40] = {0};
    uint8_t output[5] = {1U, 2U, 3U, 4U, 5U};
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_INVALID_FRAME_SIZE, dht22_protocol_pack_bits(bits, 39U, output));
    const uint8_t expected[5] = {1U, 2U, 3U, 4U, 5U};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, output, 5U);
}

static void test_pack_bits_rejects_non_binary_input(void)
{
    uint8_t bits[40] = {0};
    uint8_t output[5] = {1U, 2U, 3U, 4U, 5U};
    bits[12] = 2U;
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_INVALID_ARGUMENT, dht22_protocol_pack_bits(bits, 40U, output));
}

static void test_decode_positive_temperature_and_humidity(void)
{
    const uint8_t frame[5] = {0x02U, 0xD0U, 0x01U, 0x09U, 0xDCU};
    dht22_protocol_reading_t reading = {0};
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_OK, dht22_protocol_decode_frame(frame, 5U, &reading));
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 26.5F, reading.temperature_celsius);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 72.0F, reading.relative_humidity_percent);
}

static void test_decode_negative_temperature(void)
{
    const uint8_t frame[5] = {0x01U, 0xF4U, 0x80U, 0x37U, 0xACU};
    dht22_protocol_reading_t reading = {0};
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_OK, dht22_protocol_decode_frame(frame, 5U, &reading));
    TEST_ASSERT_FLOAT_WITHIN(0.01F, -5.5F, reading.temperature_celsius);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 50.0F, reading.relative_humidity_percent);
}

static void test_checksum_mismatch_preserves_output(void)
{
    const uint8_t frame[5] = {0x02U, 0xD0U, 0x01U, 0x09U, 0x00U};
    dht22_protocol_reading_t reading = {11.0F, 22.0F};
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_CHECKSUM_MISMATCH, dht22_protocol_decode_frame(frame, 5U, &reading));
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 11.0F, reading.temperature_celsius);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 22.0F, reading.relative_humidity_percent);
}

static void test_decode_rejects_wrong_frame_size(void)
{
    const uint8_t frame[5] = {0};
    dht22_protocol_reading_t reading = {0};
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_INVALID_FRAME_SIZE, dht22_protocol_decode_frame(frame, 4U, &reading));
}

static void test_null_arguments_are_rejected(void)
{
    const uint8_t frame[5] = {0};
    dht22_protocol_reading_t reading = {0};
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_INVALID_ARGUMENT, dht22_protocol_decode_frame(NULL, 5U, &reading));
    TEST_ASSERT_EQUAL(DHT22_PROTOCOL_INVALID_ARGUMENT, dht22_protocol_decode_frame(frame, 5U, NULL));
}

static void test_status_names_support_diagnostics(void)
{
    TEST_ASSERT_EQUAL_STRING("CHECKSUM_MISMATCH", dht22_protocol_status_name(DHT22_PROTOCOL_CHECKSUM_MISMATCH));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", dht22_protocol_status_name((dht22_protocol_status_t)99));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_limits_are_ordered);
    RUN_TEST(test_short_high_pulse_is_bit_zero);
    RUN_TEST(test_long_high_pulse_is_bit_one);
    RUN_TEST(test_too_short_pulse_preserves_output);
    RUN_TEST(test_ambiguous_pulse_is_rejected);
    RUN_TEST(test_too_long_pulse_is_rejected);
    RUN_TEST(test_invalid_limits_are_rejected);
    RUN_TEST(test_pack_bits_reconstructs_frame);
    RUN_TEST(test_pack_bits_rejects_incomplete_frame);
    RUN_TEST(test_pack_bits_rejects_non_binary_input);
    RUN_TEST(test_decode_positive_temperature_and_humidity);
    RUN_TEST(test_decode_negative_temperature);
    RUN_TEST(test_checksum_mismatch_preserves_output);
    RUN_TEST(test_decode_rejects_wrong_frame_size);
    RUN_TEST(test_null_arguments_are_rejected);
    RUN_TEST(test_status_names_support_diagnostics);
    return UNITY_END();
}
