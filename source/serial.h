#pragma once
#include <stdbool.h>    // for bool
#include "portmacro.h"  // for TickType_t
struct dali_rx_frame;
struct dali_tx_frame;

void serial_print_head(void);
void serial_print_frame(struct dali_rx_frame frame);
bool serial_get(struct dali_tx_frame* frame, TickType_t wait);
void serial_init (void);
