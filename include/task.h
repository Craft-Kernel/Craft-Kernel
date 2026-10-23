#pragma once
#include <stdint.h>

/*
 * this kernel only runs one thing at a time (there's no scheduler yet), so
 * when the network code has to wait, it just halts until enough timer ticks
 * have gone by. the card's interrupt still wakes the cpu up, so frames keep
 * getting handled while it waits.
 */
void task_sleep(uint32_t ticks);
