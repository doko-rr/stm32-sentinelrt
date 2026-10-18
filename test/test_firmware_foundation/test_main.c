#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include <unity.h>

#include "status_indicator_port.h"
#include "system_health.h"
#include "system_supervisor.h"

typedef struct
{
    bool initialize_result;
    uint32_t initialize_calls;
    uint32_t toggle_calls;
} fake_indicator_t;

static fake_indicator_t fake_indicator;

static bool fake_initialize(void *context)
{
    fake_indicator_t *fake = (fake_indicator_t *)context;
    fake->initialize_calls++;
    return fake->initialize_result;
}

static void fake_toggle(void *context)
{
    fake_indicator_t *fake = (fake_indicator_t *)context;
    fake->toggle_calls++;
}

static status_indicator_port_t make_fake_port(void)
{
    status_indicator_port_t port = {
        .context = &fake_indicator,
        .initialize = fake_initialize,
        .toggle = fake_toggle,
    };
    return port;
}

void setUp(void)
{
    fake_indicator.initialize_result = true;
    fake_indicator.initialize_calls = 0U;
    fake_indicator.toggle_calls = 0U;
}

void tearDown(void)
{
}

static void test_port_rejects_missing_callbacks(void)
{
    status_indicator_port_t port = make_fake_port();

    TEST_ASSERT_TRUE(status_indicator_port_is_valid(&port));

    port.initialize = NULL;
    TEST_ASSERT_FALSE(status_indicator_port_is_valid(&port));

    port = make_fake_port();
    port.toggle = NULL;
    TEST_ASSERT_FALSE(status_indicator_port_is_valid(&port));

    TEST_ASSERT_FALSE(status_indicator_port_is_valid(NULL));
}

static void test_health_reset_and_heartbeat_count(void)
{
    system_health_t health = {0};

    TEST_ASSERT_EQUAL_UINT32(0U, system_health_heartbeat_count(&health));

    system_health_reset(&health);
    system_health_record_heartbeat(&health);
    system_health_record_heartbeat(&health);

    TEST_ASSERT_TRUE(health.initialized);
    TEST_ASSERT_EQUAL_UINT32(2U, system_health_heartbeat_count(&health));
}

static void test_health_counter_saturates(void)
{
    system_health_t health = {
        .heartbeat_count = UINT32_MAX,
        .initialized = true,
    };

    /* Saturation preserves monotonic health telemetry during long uptimes. */
    system_health_record_heartbeat(&health);

    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, health.heartbeat_count);
}

static void test_supervisor_rejects_failed_indicator_initialization(void)
{
    system_supervisor_t supervisor = {0};
    status_indicator_port_t port = make_fake_port();
    fake_indicator.initialize_result = false;

    TEST_ASSERT_FALSE(system_supervisor_initialize(&supervisor, &port));
    TEST_ASSERT_FALSE(supervisor.initialized);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_indicator.initialize_calls);
}

static void test_supervisor_heartbeat_uses_port_and_updates_health(void)
{
    system_supervisor_t supervisor = {0};
    status_indicator_port_t port = make_fake_port();

    TEST_ASSERT_TRUE(system_supervisor_initialize(&supervisor, &port));

    system_supervisor_run_heartbeat(&supervisor);
    system_supervisor_run_heartbeat(&supervisor);

    TEST_ASSERT_EQUAL_UINT32(1U, fake_indicator.initialize_calls);
    TEST_ASSERT_EQUAL_UINT32(2U, fake_indicator.toggle_calls);
    TEST_ASSERT_EQUAL_UINT32(
        2U,
        system_health_heartbeat_count(&supervisor.health)
    );
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_port_rejects_missing_callbacks);
    RUN_TEST(test_health_reset_and_heartbeat_count);
    RUN_TEST(test_health_counter_saturates);
    RUN_TEST(test_supervisor_rejects_failed_indicator_initialization);
    RUN_TEST(test_supervisor_heartbeat_uses_port_and_updates_health);
    return UNITY_END();
}
