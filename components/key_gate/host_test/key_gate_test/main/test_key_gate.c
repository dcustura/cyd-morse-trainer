#include "unity.h"
#include "unity_fixture.h"
#include "key_gate.h"

static key_gate_t s_g;

TEST_GROUP(key_gate);

TEST_SETUP(key_gate)
{
    key_gate_init(&s_g);
}

TEST_TEAR_DOWN(key_gate)
{
}

TEST(key_gate, on_air_output_follows_the_key)
{
    TEST_ASSERT_FALSE(key_gate_update(&s_g, false, true));
    TEST_ASSERT_TRUE(key_gate_update(&s_g, true, true));
    TEST_ASSERT_TRUE(key_gate_update(&s_g, true, true));
    TEST_ASSERT_FALSE(key_gate_update(&s_g, false, true));
    TEST_ASSERT_TRUE(key_gate_update(&s_g, true, true));
}

TEST(key_gate, off_air_output_is_never_driven)
{
    TEST_ASSERT_FALSE(key_gate_update(&s_g, true, false));
    TEST_ASSERT_FALSE(key_gate_update(&s_g, false, false));
    TEST_ASSERT_FALSE(key_gate_update(&s_g, true, false));
}

TEST(key_gate, going_off_air_mid_element_releases_at_once)
{
    TEST_ASSERT_TRUE(key_gate_update(&s_g, true, true));
    TEST_ASSERT_FALSE(key_gate_update(&s_g, true, false));
    /* Back on-air while that element is still down: stays released. */
    TEST_ASSERT_FALSE(key_gate_update(&s_g, true, true));
}

TEST(key_gate, going_on_air_mid_element_waits_for_the_next_key_down)
{
    TEST_ASSERT_FALSE(key_gate_update(&s_g, true, false));
    TEST_ASSERT_FALSE(key_gate_update(&s_g, true, true));
    TEST_ASSERT_FALSE(key_gate_update(&s_g, false, true));
    TEST_ASSERT_TRUE(key_gate_update(&s_g, true, true));
}

TEST(key_gate, going_on_air_between_elements_keys_the_next_one)
{
    TEST_ASSERT_FALSE(key_gate_update(&s_g, false, false));
    TEST_ASSERT_FALSE(key_gate_update(&s_g, false, true));
    TEST_ASSERT_TRUE(key_gate_update(&s_g, true, true));
}

TEST_GROUP_RUNNER(key_gate)
{
    RUN_TEST_CASE(key_gate, on_air_output_follows_the_key);
    RUN_TEST_CASE(key_gate, off_air_output_is_never_driven);
    RUN_TEST_CASE(key_gate, going_off_air_mid_element_releases_at_once);
    RUN_TEST_CASE(key_gate, going_on_air_mid_element_waits_for_the_next_key_down);
    RUN_TEST_CASE(key_gate, going_on_air_between_elements_keys_the_next_one);
}
