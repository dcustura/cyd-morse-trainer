#include <string.h>
#include "unity.h"
#include "unity_fixture.h"
#include "example_component.h"

TEST_GROUP(example_component);

static example_component_debounce_t s_db;

TEST_SETUP(example_component)
{
    memset(&s_db, 0, sizeof(s_db));
}

TEST_TEAR_DOWN(example_component)
{
}

TEST(example_component, holds_initial_state_until_threshold_consistent_samples)
{
    example_component_debounce_init(&s_db, 3, false);
    TEST_ASSERT_FALSE(example_component_debounce_feed(&s_db, true));
    TEST_ASSERT_FALSE(example_component_debounce_feed(&s_db, true));
    TEST_ASSERT_TRUE(example_component_debounce_feed(&s_db, true));
}

TEST(example_component, resets_counter_on_bounce_back_to_current_state)
{
    example_component_debounce_init(&s_db, 3, false);
    TEST_ASSERT_FALSE(example_component_debounce_feed(&s_db, true));
    TEST_ASSERT_FALSE(example_component_debounce_feed(&s_db, true));
    TEST_ASSERT_FALSE(example_component_debounce_feed(&s_db, false)); /* bounce resets the run */
    TEST_ASSERT_FALSE(example_component_debounce_feed(&s_db, true));
    TEST_ASSERT_FALSE(example_component_debounce_feed(&s_db, true));
    TEST_ASSERT_TRUE(example_component_debounce_feed(&s_db, true));
}

TEST(example_component, threshold_of_one_flips_immediately)
{
    example_component_debounce_init(&s_db, 1, false);
    TEST_ASSERT_TRUE(example_component_debounce_feed(&s_db, true));
    TEST_ASSERT_FALSE(example_component_debounce_feed(&s_db, false));
}

TEST(example_component, feeding_current_state_leaves_it_unchanged)
{
    example_component_debounce_init(&s_db, 2, true);
    TEST_ASSERT_TRUE(example_component_debounce_feed(&s_db, true));
    TEST_ASSERT_TRUE(example_component_debounce_feed(&s_db, true));
}

TEST(example_component, init_clamps_zero_threshold_to_one)
{
    example_component_debounce_init(&s_db, 0, false);
    TEST_ASSERT_TRUE(example_component_debounce_feed(&s_db, true));
}

TEST_GROUP_RUNNER(example_component)
{
    RUN_TEST_CASE(example_component, holds_initial_state_until_threshold_consistent_samples);
    RUN_TEST_CASE(example_component, resets_counter_on_bounce_back_to_current_state);
    RUN_TEST_CASE(example_component, threshold_of_one_flips_immediately);
    RUN_TEST_CASE(example_component, feeding_current_state_leaves_it_unchanged);
    RUN_TEST_CASE(example_component, init_clamps_zero_threshold_to_one);
}
