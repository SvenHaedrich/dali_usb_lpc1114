#pragma once

enum board_led { LED_HEARTBEAT = 0, LED_DALI = 1, LED_SERIAL = 2, LED_MAX = 3 };

void board_flash(enum board_led id);
void board_indicate_error(void);
void board_led_init(void);
