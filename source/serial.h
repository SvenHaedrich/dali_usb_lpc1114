#pragma once
struct dali_rx_frame;

/* The UART: line assembly on the way in, whole messages on the way out.
   What the characters mean is command.c's business. */

void serial_init(void);
void serial_print_head(void);
void serial_print_frame(struct dali_rx_frame frame);
