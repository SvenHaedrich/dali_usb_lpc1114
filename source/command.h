#pragma once
// clang-format off
#include "portmacro.h"  // for TickType_t, BaseType_t
// clang-format on
struct dali_tx_frame;

/* The ASCII command protocol of doc/commands.md.
   serial.c moves the characters, this module decides what they mean.
   command_init() has to run before serial_init() enables the UART interrupt. */

void command_init(void);
void command_receive_from_isr(char character, BaseType_t* higher_priority_woken);
int command_get(struct dali_tx_frame* frame, TickType_t wait);
void command_report_cannot_process(void);
