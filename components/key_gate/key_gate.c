#include "key_gate.h"

void key_gate_init(key_gate_t *g)
{
    g->output = false;
    g->prev_key_down = false;
}

bool key_gate_update(key_gate_t *g, bool key_down, bool on_air)
{
    bool key_down_edge = key_down && !g->prev_key_down;
    g->prev_key_down = key_down;

    if (!on_air || !key_down) {
        g->output = false;
    } else if (key_down_edge) {
        g->output = true;
    }
    return g->output;
}
