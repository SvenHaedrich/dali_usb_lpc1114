#pragma once

enum dali_tx_priority {
    DALI_BACKWARD_FRAME = 0,
    DALI_PRIORITY_1 = 1,
    DALI_PRIORITY_2 = 2,
    DALI_PRIORITY_3 = 3,
    DALI_PRIORITY_4 = 4,
    DALI_PRIORITY_5 = 5,
    DALI_PRIORITY_SEND_TWICE = 6,
};

enum dali_tx_return {
    RETURN_OK = 0,
    RETURN_CAN_NOT_CONVERT = 1,
    RETURN_BAD_ARGUMENT = 2,
};

struct dali_tx_frame {
    uint8_t length;
    uint32_t data;
    bool send_twice;
    uint8_t repeat;
    enum dali_tx_priority priority;
};

bool dali_is_tx_active(void);
void dali_destroy (void);
enum dali_tx_return dali_transmit (const struct dali_tx_frame frame);
void dali_tx_init(void);
