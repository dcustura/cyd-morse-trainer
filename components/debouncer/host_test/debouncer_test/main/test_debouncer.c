#include <string.h>
#include "unity.h"
#include "unity_fixture.h"
#include "debouncer.h"

TEST_GROUP(debouncer);

static debouncer_t s_db;

TEST_SETUP(debouncer)
{
    memset(&s_db, 0, sizeof(s_db));
}

TEST_TEAR_DOWN(debouncer)
{
}

TEST(debouncer, holds_initial_state_until_threshold_consistent_samples)
{
    debouncer_init(&s_db, 3, false);
    TEST_ASSERT_FALSE(debouncer_feed(&s_db, true));
    TEST_ASSERT_FALSE(debouncer_feed(&s_db, true));
    TEST_ASSERT_TRUE(debouncer_feed(&s_db, true));
}

TEST(debouncer, resets_counter_on_bounce_back_to_current_state)
{
    debouncer_init(&s_db, 3, false);
    TEST_ASSERT_FALSE(debouncer_feed(&s_db, true));
    TEST_ASSERT_FALSE(debouncer_feed(&s_db, true));
    TEST_ASSERT_FALSE(debouncer_feed(&s_db, false)); /* bounce resets the run */
    TEST_ASSERT_FALSE(debouncer_feed(&s_db, true));
    TEST_ASSERT_FALSE(debouncer_feed(&s_db, true));
    TEST_ASSERT_TRUE(debouncer_feed(&s_db, true));
}

TEST(debouncer, threshold_of_one_flips_immediately)
{
    debouncer_init(&s_db, 1, false);
    TEST_ASSERT_TRUE(debouncer_feed(&s_db, true));
    TEST_ASSERT_FALSE(debouncer_feed(&s_db, false));
}

TEST(debouncer, feeding_current_state_leaves_it_unchanged)
{
    debouncer_init(&s_db, 2, true);
    TEST_ASSERT_TRUE(debouncer_feed(&s_db, true));
    TEST_ASSERT_TRUE(debouncer_feed(&s_db, true));
}

TEST(debouncer, init_clamps_zero_threshold_to_one)
{
    debouncer_init(&s_db, 0, false);
    TEST_ASSERT_TRUE(debouncer_feed(&s_db, true));
}

TEST_GROUP_RUNNER(debouncer)
{
    RUN_TEST_CASE(debouncer, holds_initial_state_until_threshold_consistent_samples);
    RUN_TEST_CASE(debouncer, resets_counter_on_bounce_back_to_current_state);
    RUN_TEST_CASE(debouncer, threshold_of_one_flips_immediately);
    RUN_TEST_CASE(debouncer, feeding_current_state_leaves_it_unchanged);
    RUN_TEST_CASE(debouncer, init_clamps_zero_threshold_to_one);
}
