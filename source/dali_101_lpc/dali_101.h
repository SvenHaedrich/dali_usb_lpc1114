#pragma once
#define DALI_101_MAJOR_VERSION (2)
#define DALI_101_MINOR_VERSION (1)

enum dali_tx_priority {
    DALI_BACKWARD_FRAME = 0,
    DALI_PRIORITY_1 = 1,
    DALI_PRIORITY_2 = 2,
    DALI_PRIORITY_3 = 3,
    DALI_PRIORITY_4 = 4,
    DALI_PRIORITY_5 = 5,
    DALI_PRIORITY_SEND_TWICE = 6,
    DALI_PRIORITY_END = 7,
};

enum dali_error {
    DALI_OK = 0,
    DALI_ERROR_RECEIVE_START_TIMING = 1,
    DALI_ERROR_RECEIVE_DATA_TIMING = 2,
    DALI_ERROR_COLLISION_LOOPBACK_TIME = 3,
    DALI_ERROR_COLLISION_NO_CHANGE = 4,
    DALI_ERROR_COLLISION_WRONG_STATE = 5,
    DALI_ERROR_SETTLING_TIME_VIOLATION = 6,
    DALI_SYSTEM_IDLE = 11,
    DALI_SYSTEM_FAILURE = 12,
    DALI_SYSTEM_RECOVER = 13,
    DALI_ERROR_CAN_NOT_PROCESS = 20,
    DALI_ERROR_BAD_ARGUMENT = 21,
    DALI_ERROR_QUEUE_FULL = 22,
    DALI_ERROR_BAD_COMMAND = 23,
};

struct dali_rx_frame {
    bool is_status;
    uint8_t length;
    uint32_t data;
    uint32_t timestamp;
};

struct dali_tx_frame {
    uint8_t length;
    uint32_t data;
    uint8_t repeat;
    enum dali_tx_priority priority;
};

void dali_101_init(void);
void dali_101_send(const struct dali_tx_frame frame);
bool dali_101_get(struct dali_rx_frame* frame, TickType_t wait);
void dali_101_sequence_start(void);
void dali_101_sequence_next(uint32_t period_us);
void dali_101_sequence_execute(void);
void dali_101_request_status_frame(void);

void dali_tx_irq_callback(void);
void dali_rx_irq_capture_callback(void);
void dali_rx_irq_stopbit_match_callback(void);
void dali_rx_irq_period_match_callback(void);