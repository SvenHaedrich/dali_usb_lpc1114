#pragma once
// clang-format off
#include <stdbool.h> // for bool
// clang-format on

struct dali_rx_frame;

struct serial_line {
    char* text; // NULL when there is only a dropped line to report
    bool too_long;
    bool dropped; // at least one line was lost before this one
};

void serial_init(void);
void serial_print_head(void);
void serial_print_frame(struct dali_rx_frame frame);
struct serial_line serial_take_line(void);
void serial_release_line(void);
