#pragma once
#define DALI_101_MAJOR_VERSION (3)
#define DALI_101_MINOR_VERSION (9)
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

/**
 * @brief DALI received frame
 *
 */
struct dali_rx_frame {
    bool loopback;           /**< frame was received while transmission was active*/
    enum dali_status status; /**< status at the end of frame receiption */
    uint8_t length;          /**< number of data bits received */
    uint32_t data;           /**< data payload */
    uint32_t timestamp;      /**< timetstamp when start bit was deteceted */
};

/**
 * @brief DALI transmission frame
 *
 */
struct dali_tx_frame {
    bool is_query;                  /**< check for a backward frame */
    bool is_corrupt;                /**< tranmsit a corrupt frame (disregard `length` and `data`)*/
    uint8_t length;                 /**< number of data bits to transmit */
    uint32_t data;                  /**< data payload */
    uint8_t repeat;                 /**< repeat entire frame */
    enum dali_tx_priority priority; /**< inter frame timing priority */
};

/**
 * @brief Initialize the DALI low level driver
 *
 */
void dali_101_init(void);

/**
 * @brief Send DALI frame
 *
 * @param frame frame to send
 */
void dali_101_send(const struct dali_tx_frame frame);

/**
 * @brief Get the next frame from the input queue
 *
 * @param frame received frame
 * @param wait number of ticks to wait until a frame is available in queue
 * @return `true` - a frame was received and is availabe
 * @return `false`-  no frame availabe, discard buffer
 */
bool dali_101_get(struct dali_rx_frame* frame, TickType_t wait);

/**
 * @brief Check if a transmission is active or pending
 *
 * @return `true` - no transmission
 * @return `false` - transmission is active
 */
bool dali_101_tx_is_idle(void);

/**
 * @brief Start a new bit sequence, discard old sequence information
 *
 */
void dali_101_sequence_start(void);

/**
 * @brief Define next period for sequence
 *
 * @param period_us duration for the next period, given in micro seconds
 */
void dali_101_sequence_next(uint32_t period_us);

/**
 * @brief Send the sequence
 *
 */
void dali_101_sequence_execute(void);

/**
 * @brief request a status frame
 *
 * Request a status frame, even when there is no bus activity.
 * This is a soon to be obsolete function.
 */
void dali_101_request_status_frame(void);

/* callback functions defined by the low level driver
 *  to be called by the board interface module
 */
void dali_tx_irq_callback(void);
void dali_rx_irq_capture_callback(void);
void dali_rx_irq_stopbit_match_callback(void);
void dali_rx_irq_period_match_callback(void);
void dali_rx_irq_query_match_callback(void);