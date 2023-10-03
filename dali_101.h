#pragma once
#define DALI_101_MAJOR_VERSION (3)
#define DALI_101_MINOR_VERSION (6)
#define DALI_MAX_DATA_LENGTH (32U)

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

enum dali_status {
    DALI_OK = 0x0,
    DALI_TIMEOUT = 0x81,
    DALI_ERROR_RECEIVE_START_TIMING = 0x82,
    DALI_ERROR_RECEIVE_DATA_TIMING = 0x83,
    DALI_ERROR_COLLISION_LOOPBACK_TIME = 0x84,
    DALI_ERROR_COLLISION_NO_CHANGE = 0x85,
    DALI_ERROR_COLLISION_WRONG_STATE = 0x86,
    DALI_ERROR_SETTLING_TIME_VIOLATION = 0x87,
    DALI_SYSTEM_IDLE = 0x90,
    DALI_SYSTEM_FAILURE = 0x91,
    DALI_SYSTEM_RECOVER = 0x92,
    DALI_ERROR_CAN_NOT_PROCESS = 0xA0,
    DALI_ERROR_BAD_ARGUMENT = 0xA1,
    DALI_ERROR_QUEUE_FULL = 0xA2,
    DALI_ERROR_BAD_COMMAND = 0xA3,
};

struct dali_rx_frame {
    bool loopback;
    enum dali_status status;
    uint8_t length;
    uint32_t data;
    uint32_t timestamp;
};

struct dali_tx_frame {
    bool is_query;
    uint8_t length;
    uint32_t data;
    uint8_t repeat;
    enum dali_tx_priority priority;
};

void dali_101_init(void);
void dali_101_send(const struct dali_tx_frame frame);
bool dali_101_get(struct dali_rx_frame* frame, TickType_t wait);
bool dali_101_tx_is_idle(void);
void dali_101_sequence_start(void);
void dali_101_sequence_next(uint32_t period_us);
void dali_101_sequence_execute(void);
void dali_101_request_status_frame(void);

void dali_tx_irq_callback(void);
void dali_rx_irq_capture_callback(void);
void dali_rx_irq_stopbit_match_callback(void);
void dali_rx_irq_period_match_callback(void);
void dali_rx_irq_query_match_callback(void);