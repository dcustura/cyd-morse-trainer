#include "example_component.h"

void example_component_debounce_init(example_component_debounce_t *db, uint8_t threshold, bool initial_state)
{
    db->threshold = threshold > 0 ? threshold : 1;
    db->counter = 0;
    db->state = initial_state;
}

bool example_component_debounce_feed(example_component_debounce_t *db, bool sample)
{
    if (sample == db->state) {
        db->counter = 0;
        return db->state;
    }

    db->counter++;
    if (db->counter >= db->threshold) {
        db->state = sample;
        db->counter = 0;
    }

    return db->state;
}
