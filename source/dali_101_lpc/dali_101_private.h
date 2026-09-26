#pragma once
// clang-format off
#include <stdbool.h>   // for bool
#include "dali_101.h"  // for dali_frame_type
// clang-format on

/* Functions the transmitter and the receiver share with each other.
   Not part of the driver interface, see dali_101.h for that. */

void rx_schedule_transmission(enum dali_frame_type type);
void rx_schedule_query(void);

void dali_tx_init(void);
void dali_tx_start_send(void);
void tx_reset(void);
bool dali_tx_repeat(void);
