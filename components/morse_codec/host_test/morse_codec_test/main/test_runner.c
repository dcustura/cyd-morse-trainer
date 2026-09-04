#include "unity.h"
#include "unity_fixture.h"

static void run_all_tests(void)
{
    RUN_TEST_GROUP(morse_codec);
}

void app_main(void)
{
    UNITY_MAIN_FUNC(run_all_tests);
}
