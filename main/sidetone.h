#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIDETONE_DEFAULT_HZ 600

/** Configure the LEDC timer/channel driving the speaker GPIO. */
void sidetone_init(void);

/** Change the sidetone pitch. Takes effect on the next sidetone_key(true). */
void sidetone_set_freq(uint16_t hz);

/** Turn the sidetone on (key down) or off (key up). */
void sidetone_key(bool down);

#ifdef __cplusplus
}
#endif
