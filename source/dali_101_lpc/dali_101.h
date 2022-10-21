#pragma once
#define DALI_101_MAJOR_VERSION (2)
#define DALI_101_MINOR_VERSION (0)

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

enum dali_errors {  // TODO rename to dali_error
    DALI_OK = 0,
    DALI_ERROR_RECEIVE_START_TIMING = 1,
    DALI_ERROR_RECEIVE_DATA_TIMING = 2,
    DALI_ERROR_COLLISION_LOOPBACK_TIME = 3,
    DALI_ERROR_COLLISION_NO_CHANGE = 4,
    DALI_ERROR_COLLISION_WRONG_STATE = 5,
    DALI_ERROR_SETTLING_TIME_VIOLATION = 6,
    DALI_ERROR_SYSTEM_FAILURE = 7,
    DALI_ERROR_SYSTEM_RECOVER = 8
};

struct dali_rx_frame {
    enum dali_errors error_code;
    uint8_t length;
    uint32_t data;
    uint32_t timestamp;
    uint32_t error_timer_usec;
};

struct dali_tx_frame {
    uint8_t length;
    uint32_t data;
    bool send_twice;
    uint8_t repeat;
    enum dali_tx_priority priority;
};

void dali_101_init(void);
enum dali_tx_return dali_101_send (const struct dali_tx_frame frame);
bool dali_101_get(struct dali_rx_frame* frame, TickType_t wait);
enum dali_errors dali_101_get_error(void);
void dali_101_sequence_start(void);
bool dali_101_sequence_next(uint32_t delay_us);
void dali_101_sequence_execute(void);