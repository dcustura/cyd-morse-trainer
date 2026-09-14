#include "debouncer.h"

void debouncer_init(debouncer_t *db, uint8_t threshold, bool initial_state)
{
    db->threshold = threshold > 0 ? threshold : 1;
    db->counter = 0;
    db->state = initial_state;
}

bool debouncer_feed(debouncer_t *db, bool sample)
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
