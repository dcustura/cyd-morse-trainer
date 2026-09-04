#include <stddef.h>
#include "esp_log.h"
#include "example_component.h"

static const char *TAG = "template_project";

void app_main(void)
{
    example_component_debounce_t button;
    example_component_debounce_init(&button, 3, false);

    const bool samples[] = { true, true, true, false, false };
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        bool debounced = example_component_debounce_feed(&button, samples[i]);
        ESP_LOGI(TAG, "sample=%d debounced_state=%d", samples[i], debounced);
    }
}
