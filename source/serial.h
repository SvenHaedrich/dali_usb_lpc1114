#pragma once
struct dali_rx_frame;

void serial_init(void);
void serial_print_head(void);
void serial_print_frame(struct dali_rx_frame frame);
