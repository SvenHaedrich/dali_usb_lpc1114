#pragma once

void frame_pump_enqueue_tx_frame(struct dali_tx_frame frame);
void frame_pump_enqueue_rx_frame(struct dali_rx_frame frame);
void frame_pump_set_tx_callback(void (*callback)(const struct dali_tx_frame));
void frame_pump_set_rx_callback(void (*callback)(const struct dali_rx_frame));
void frame_pump_init(void);