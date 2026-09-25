#pragma once
// clang-format off
#include "portmacro.h"  // for TickType_t
// clang-format on
struct dali_rx_frame;
struct dali_tx_frame;

void serial_print_head(void);
void serial_print_frame(struct dali_rx_frame frame);
void serial_print_cannot_process(void);
int serial_get(struct dali_tx_frame* frame, TickType_t wait);
void serial_init(void);
