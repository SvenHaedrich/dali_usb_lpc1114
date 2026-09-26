#pragma once
// clang-format off
#include "portmacro.h"  // for BaseType_t
// clang-format on

/* The ASCII command protocol of doc/commands.md.
   serial.c moves the characters, this module decides what they mean.
   command_init() has to run before serial_init() enables the UART interrupt. */

void command_init(void);
void command_receive_from_isr(char character, BaseType_t* higher_priority_woken);
void command_execute_pending(void);
