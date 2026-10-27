#include <unity.h>

#include "reset_cause_classifier.h"

void setUp(void) {}
void tearDown(void) {}

static void test_no_flags_is_unknown(void)
{
    TEST_ASSERT_EQUAL(RESET_CAUSE_UNKNOWN, reset_cause_classify(0U));
}

static void test_individual_flags_are_classified(void)
{
    TEST_ASSERT_EQUAL(RESET_CAUSE_POWER_ON, reset_cause_classify(RESET_FLAG_POWER_ON));
    TEST_ASSERT_EQUAL(RESET_CAUSE_BROWNOUT, reset_cause_classify(RESET_FLAG_BROWNOUT));
    TEST_ASSERT_EQUAL(RESET_CAUSE_PIN, reset_cause_classify(RESET_FLAG_PIN));
    TEST_ASSERT_EQUAL(RESET_CAUSE_SOFTWARE, reset_cause_classify(RESET_FLAG_SOFTWARE));
    TEST_ASSERT_EQUAL(RESET_CAUSE_IWDG, reset_cause_classify(RESET_FLAG_IWDG));
    TEST_ASSERT_EQUAL(RESET_CAUSE_WWDG, reset_cause_classify(RESET_FLAG_WWDG));
    TEST_ASSERT_EQUAL(RESET_CAUSE_LOW_POWER, reset_cause_classify(RESET_FLAG_LOW_POWER));
    TEST_ASSERT_EQUAL(RESET_CAUSE_OPTION_BYTE, reset_cause_classify(RESET_FLAG_OPTION_BYTE));
}

static void test_priority_is_deterministic(void)
{
    TEST_ASSERT_EQUAL(RESET_CAUSE_IWDG, reset_cause_classify(RESET_FLAG_IWDG | RESET_FLAG_PIN));
    TEST_ASSERT_EQUAL(RESET_CAUSE_SOFTWARE, reset_cause_classify(RESET_FLAG_SOFTWARE | RESET_FLAG_PIN));
    TEST_ASSERT_EQUAL(RESET_CAUSE_BROWNOUT, reset_cause_classify(RESET_FLAG_BROWNOUT | RESET_FLAG_POWER_ON));
}

static void test_names_are_compact_and_deterministic(void)
{
    TEST_ASSERT_EQUAL_STRING("UNK", reset_cause_name(RESET_CAUSE_UNKNOWN));
    TEST_ASSERT_EQUAL_STRING("POR", reset_cause_name(RESET_CAUSE_POWER_ON));
    TEST_ASSERT_EQUAL_STRING("BOR", reset_cause_name(RESET_CAUSE_BROWNOUT));
    TEST_ASSERT_EQUAL_STRING("PIN", reset_cause_name(RESET_CAUSE_PIN));
    TEST_ASSERT_EQUAL_STRING("SW", reset_cause_name(RESET_CAUSE_SOFTWARE));
    TEST_ASSERT_EQUAL_STRING("IWDG", reset_cause_name(RESET_CAUSE_IWDG));
    TEST_ASSERT_EQUAL_STRING("WWDG", reset_cause_name(RESET_CAUSE_WWDG));
    TEST_ASSERT_EQUAL_STRING("LP", reset_cause_name(RESET_CAUSE_LOW_POWER));
    TEST_ASSERT_EQUAL_STRING("OBL", reset_cause_name(RESET_CAUSE_OPTION_BYTE));
    TEST_ASSERT_EQUAL_STRING("UNK", reset_cause_name((reset_cause_t)99));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_flags_is_unknown);
    RUN_TEST(test_individual_flags_are_classified);
    RUN_TEST(test_priority_is_deterministic);
    RUN_TEST(test_names_are_compact_and_deterministic);
    return UNITY_END();
}
