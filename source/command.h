#pragma once
// clang-format off
#include "portmacro.h"  // for BaseType_t
// clang-format on

void command_init(void);
void command_receive_from_isr(char character, BaseType_t* higher_priority_woken);
void command_execute_pending(void);
