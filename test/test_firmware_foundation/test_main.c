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

typedef struct
{
    bool refresh_result;
    uint32_t refresh_calls;
} fake_watchdog_t;

static fake_indicator_t fake_indicator;
static fake_watchdog_t fake_watchdog;

static bool fake_initialize(void *context)
{
    fake_indicator_t *fake = context;
    fake->initialize_calls++;
    return fake->initialize_result;
}

static void fake_toggle(void *context)
{
    fake_indicator_t *fake = context;
    fake->toggle_calls++;
}

static bool fake_refresh(void *context)
{
    fake_watchdog_t *fake = context;
    fake->refresh_calls++;
    return fake->refresh_result;
}

static status_indicator_port_t make_fake_indicator(void)
{
    status_indicator_port_t port = {
        .context = &fake_indicator,
        .initialize = fake_initialize,
        .toggle = fake_toggle,
    };
    return port;
}

static watchdog_port_t make_fake_watchdog(void)
{
    watchdog_port_t port = {
        .context = &fake_watchdog,
        .refresh = fake_refresh,
    };
    return port;
}

static system_watchdog_policy_t make_policy(void)
{
    system_watchdog_policy_t policy = {
        .heartbeat_timeout_ticks = 1500U,
        .sensor_timeout_ticks = 3000U,
    };
    return policy;
}

static void initialize_supervisor_with_watchdog(system_supervisor_t *supervisor)
{
    status_indicator_port_t indicator = make_fake_indicator();
    watchdog_port_t watchdog = make_fake_watchdog();
    system_watchdog_policy_t policy = make_policy();

    TEST_ASSERT_TRUE(system_supervisor_initialize(supervisor, &indicator));
    TEST_ASSERT_TRUE(
        system_supervisor_configure_watchdog(supervisor, &watchdog, &policy)
    );
}

void setUp(void)
{
    fake_indicator = (fake_indicator_t){0};
    fake_watchdog = (fake_watchdog_t){0};
    fake_indicator.initialize_result = true;
    fake_watchdog.refresh_result = true;
}

void tearDown(void)
{
}

static void test_indicator_port_rejects_missing_callbacks(void)
{
    status_indicator_port_t port = make_fake_indicator();

    TEST_ASSERT_TRUE(status_indicator_port_is_valid(&port));
    port.initialize = NULL;
    TEST_ASSERT_FALSE(status_indicator_port_is_valid(&port));
    port = make_fake_indicator();
    port.toggle = NULL;
    TEST_ASSERT_FALSE(status_indicator_port_is_valid(&port));
    TEST_ASSERT_FALSE(status_indicator_port_is_valid(NULL));
}

static void test_watchdog_port_rejects_missing_refresh(void)
{
    watchdog_port_t port = make_fake_watchdog();

    TEST_ASSERT_TRUE(watchdog_port_is_valid(&port));
    port.refresh = NULL;
    TEST_ASSERT_FALSE(watchdog_port_is_valid(&port));
    TEST_ASSERT_FALSE(watchdog_port_is_valid(NULL));
}

static void test_health_reset_and_heartbeat_count(void)
{
    system_health_t health = {0};

    system_health_reset(&health);
    system_health_record_heartbeat(&health);
    system_health_record_heartbeat(&health);
    TEST_ASSERT_TRUE(health.initialized);
    TEST_ASSERT_EQUAL_UINT32(2U, system_health_heartbeat_count(&health));
}

static void test_health_counter_saturates(void)
{
    system_health_t health = {.heartbeat_count = UINT32_MAX, .initialized = true};

    system_health_record_heartbeat(&health);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, health.heartbeat_count);
}

static void test_supervisor_rejects_failed_indicator_initialization(void)
{
    system_supervisor_t supervisor = {0};
    status_indicator_port_t port = make_fake_indicator();
    fake_indicator.initialize_result = false;

    TEST_ASSERT_FALSE(system_supervisor_initialize(&supervisor, &port));
    TEST_ASSERT_FALSE(supervisor.initialized);
}

static void test_supervisor_heartbeat_uses_owned_port(void)
{
    system_supervisor_t supervisor = {0};
    status_indicator_port_t port = make_fake_indicator();

    TEST_ASSERT_TRUE(system_supervisor_initialize(&supervisor, &port));
    port.toggle = NULL;
    system_supervisor_run_heartbeat(&supervisor);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_indicator.toggle_calls);
    TEST_ASSERT_EQUAL_UINT32(1U, system_health_heartbeat_count(&supervisor.health));
}

static void test_watchdog_rejects_invalid_configuration(void)
{
    system_supervisor_t supervisor = {0};
    status_indicator_port_t indicator = make_fake_indicator();
    watchdog_port_t watchdog = make_fake_watchdog();
    system_watchdog_policy_t policy = make_policy();

    TEST_ASSERT_FALSE(system_supervisor_configure_watchdog(&supervisor, &watchdog, &policy));
    TEST_ASSERT_TRUE(system_supervisor_initialize(&supervisor, &indicator));
    TEST_ASSERT_FALSE(system_supervisor_configure_watchdog(&supervisor, NULL, &policy));
    TEST_ASSERT_FALSE(system_supervisor_configure_watchdog(&supervisor, &watchdog, NULL));
    policy.heartbeat_timeout_ticks = 0U;
    TEST_ASSERT_FALSE(system_supervisor_configure_watchdog(&supervisor, &watchdog, &policy));
    policy = make_policy();
    policy.sensor_timeout_ticks = UINT32_MAX;
    TEST_ASSERT_FALSE(system_supervisor_configure_watchdog(&supervisor, &watchdog, &policy));
}

static void test_watchdog_baseline_does_not_refresh(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_NOT_READY,
        system_supervisor_service_watchdog(&supervisor, 100U, 1U, true, true)
    );
    TEST_ASSERT_EQUAL_UINT32(0U, fake_watchdog.refresh_calls);
}

static void test_watchdog_refreshes_inside_progress_windows(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    (void)system_supervisor_service_watchdog(&supervisor, 0U, 1U, true, true);
    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_REFRESHED,
        system_supervisor_service_watchdog(&supervisor, 500U, 1U, true, true)
    );
}

static void test_heartbeat_progress_refreshes_its_deadline(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    (void)system_supervisor_service_watchdog(&supervisor, 0U, 1U, true, true);
    system_supervisor_run_heartbeat(&supervisor);
    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_REFRESHED,
        system_supervisor_service_watchdog(&supervisor, 1400U, 1U, true, true)
    );
}

static void test_sensor_progress_refreshes_its_deadline(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    (void)system_supervisor_service_watchdog(&supervisor, 0U, 1U, true, true);
    system_supervisor_run_heartbeat(&supervisor);
    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_REFRESHED,
        system_supervisor_service_watchdog(&supervisor, 1400U, 2U, true, true)
    );
}

static void test_heartbeat_timeout_blocks_refresh(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    (void)system_supervisor_service_watchdog(&supervisor, 0U, 1U, true, true);
    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_BLOCKED_NO_PROGRESS,
        system_supervisor_service_watchdog(&supervisor, 1500U, 2U, true, true)
    );
}

static void test_sensor_timeout_blocks_refresh(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    (void)system_supervisor_service_watchdog(&supervisor, 0U, 1U, true, true);
    system_supervisor_run_heartbeat(&supervisor);
    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_BLOCKED_NO_PROGRESS,
        system_supervisor_service_watchdog(&supervisor, 3000U, 1U, true, true)
    );
}

static void test_stack_critical_blocks_refresh(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_BLOCKED_STACK_CRITICAL,
        system_supervisor_service_watchdog(&supervisor, 0U, 1U, false, true)
    );
}

static void test_sensor_failure_sequence_counts_as_progress(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    (void)system_supervisor_service_watchdog(&supervisor, 0U, 10U, true, true);
    system_supervisor_run_heartbeat(&supervisor);
    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_REFRESHED,
        system_supervisor_service_watchdog(&supervisor, 2000U, 11U, true, true)
    );
}

static void test_watchdog_refresh_failure_is_reported(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    (void)system_supervisor_service_watchdog(&supervisor, 0U, 1U, true, true);
    fake_watchdog.refresh_result = false;
    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_REFRESH_FAILED,
        system_supervisor_service_watchdog(&supervisor, 500U, 1U, true, true)
    );
}

static void test_watchdog_refresh_counter_saturates(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);
    supervisor.watchdog_refresh_count = UINT32_MAX;

    (void)system_supervisor_service_watchdog(&supervisor, 0U, 1U, true, true);
    (void)system_supervisor_service_watchdog(&supervisor, 500U, 1U, true, true);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, supervisor.watchdog_refresh_count);
}

static void test_watchdog_recovers_after_progress_resumes(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    (void)system_supervisor_service_watchdog(&supervisor, 0U, 1U, true, true);
    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_BLOCKED_NO_PROGRESS,
        system_supervisor_service_watchdog(&supervisor, 1500U, 1U, true, true)
    );
    system_supervisor_run_heartbeat(&supervisor);
    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_REFRESHED,
        system_supervisor_service_watchdog(&supervisor, 1600U, 2U, true, true)
    );
}

static void test_heartbeat_deadline_is_wraparound_safe(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    (void)system_supervisor_service_watchdog(&supervisor, UINT32_MAX - 999U, 1U, true, true);
    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_BLOCKED_NO_PROGRESS,
        system_supervisor_service_watchdog(&supervisor, 500U, 2U, true, true)
    );
}

static void test_sensor_deadline_is_wraparound_safe(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    (void)system_supervisor_service_watchdog(&supervisor, UINT32_MAX - 1999U, 1U, true, true);
    system_supervisor_run_heartbeat(&supervisor);
    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_BLOCKED_NO_PROGRESS,
        system_supervisor_service_watchdog(&supervisor, 1000U, 1U, true, true)
    );
}

static void test_progress_at_deadline_is_accepted_before_timeout_check(void)
{
    system_supervisor_t supervisor = {0};
    initialize_supervisor_with_watchdog(&supervisor);

    (void)system_supervisor_service_watchdog(&supervisor, 0U, 1U, true, true);
    system_supervisor_run_heartbeat(&supervisor);
    TEST_ASSERT_EQUAL(
        SYSTEM_WATCHDOG_REFRESHED,
        system_supervisor_service_watchdog(&supervisor, 1500U, 2U, true, true)
    );
}

static void test_watchdog_evaluation_does_not_refresh_hardware(void)
{
    system_supervisor_t supervisor = {0};

    initialize_supervisor_with_watchdog(&supervisor);
    system_supervisor_run_heartbeat(&supervisor);
    TEST_ASSERT_EQUAL(SYSTEM_WATCHDOG_NOT_READY,
        system_supervisor_evaluate_watchdog(&supervisor, 0U, 1U, true, true));

    system_supervisor_run_heartbeat(&supervisor);
    TEST_ASSERT_EQUAL(SYSTEM_WATCHDOG_REFRESH_ALLOWED,
        system_supervisor_evaluate_watchdog(&supervisor, 500U, 2U, true, true));
    TEST_ASSERT_EQUAL_UINT32(0U, fake_watchdog.refresh_calls);
    TEST_ASSERT_EQUAL(SYSTEM_WATCHDOG_REFRESHED,
        system_supervisor_refresh_watchdog(&supervisor));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_watchdog.refresh_calls);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_indicator_port_rejects_missing_callbacks);
    RUN_TEST(test_watchdog_port_rejects_missing_refresh);
    RUN_TEST(test_health_reset_and_heartbeat_count);
    RUN_TEST(test_health_counter_saturates);
    RUN_TEST(test_supervisor_rejects_failed_indicator_initialization);
    RUN_TEST(test_supervisor_heartbeat_uses_owned_port);
    RUN_TEST(test_watchdog_rejects_invalid_configuration);
    RUN_TEST(test_watchdog_baseline_does_not_refresh);
    RUN_TEST(test_watchdog_refreshes_inside_progress_windows);
    RUN_TEST(test_heartbeat_progress_refreshes_its_deadline);
    RUN_TEST(test_sensor_progress_refreshes_its_deadline);
    RUN_TEST(test_heartbeat_timeout_blocks_refresh);
    RUN_TEST(test_sensor_timeout_blocks_refresh);
    RUN_TEST(test_stack_critical_blocks_refresh);
    RUN_TEST(test_sensor_failure_sequence_counts_as_progress);
    RUN_TEST(test_watchdog_refresh_failure_is_reported);
    RUN_TEST(test_watchdog_refresh_counter_saturates);
    RUN_TEST(test_watchdog_recovers_after_progress_resumes);
    RUN_TEST(test_heartbeat_deadline_is_wraparound_safe);
    RUN_TEST(test_sensor_deadline_is_wraparound_safe);
    RUN_TEST(test_progress_at_deadline_is_accepted_before_timeout_check);
    RUN_TEST(test_watchdog_evaluation_does_not_refresh_hardware);
    return UNITY_END();
}
