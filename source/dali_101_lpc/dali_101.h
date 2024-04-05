#pragma once
#include <stdbool.h>
#include <stdint.h>

#define DALI_101_MAJOR_VERSION (4U)
#define DALI_101_MINOR_VERSION (0U)
#define DALI_MAX_DATA_LENGTH (32U)

/**
 * @brief Type of DALI frame to transmit. Priorities are defined by
 * IEC 62386-101:2022 Table 22 - Multi-master transmitter settling time values
 */
enum dali_frame_type {
    DALI_FRAME_NONE = 0,     /**< no frame to send */
    DALI_FRAME_BACKWARD,     /**< send a backward frame*/
    DALI_FRAME_BACK_TO_BACK, /**< send next frame immediately after minimal stop condition*/
    DALI_FRAME_FORWARD_1,    /**< send a forward frame, inter frame priority 1 */
    DALI_FRAME_FORWARD_2,    /**< send a forward frame, inter frame priority 2 */
    DALI_FRAME_FORWARD_3,    /**< send a forward frame, inter frame priority 3 */
    DALI_FRAME_FORWARD_4,    /**< send a forward frame, inter frame priority 4 */
    DALI_FRAME_FORWARD_5,    /**< send a forward frame, inter frame priority 5 */
    DALI_FRAME_QUERY_1,      /**< send a forward query frame, inter frame priority 1, expect backward frame */
    DALI_FRAME_QUERY_2,      /**< send a forward query frame, inter frame priority 2, expect backward frame */
    DALI_FRAME_QUERY_3,      /**< send a forward query frame, inter frame priority 3, expect backward frame */
    DALI_FRAME_QUERY_4,      /**< send a forward query frame, inter frame priority 4, expect backward frame */
    DALI_FRAME_QUERY_5,      /**< send a forward query frame, inter frame priority 5, expect backward frame */
    DALI_FRAME_CORRUPT,      /**< send a corrupt frame */
};

/**
 * @brief Status codes for received frames
 *
 */
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
    bool twice;              /**< frame was received twice */
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
    uint8_t length;            /**< number of data bits to transmit */
    uint32_t data;             /**< data payload */
    uint8_t repeat;            /**< repeat entire frame */
    enum dali_frame_type type; /**< frame type */
};

/**
 * @brief Initialize the DALI low level driver.
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
 * @brief Get the next frame from the input queue.
 *
 * @param frame received frame
 * @param wait_ms number of milliseconds to wait for a frame from queue
 * @param forever if `true` the get function will not timeout. `wait_ms` is disregarded.
 * @return `true` - a frame was received and is availabe
 * @return `false`-  no frame availabe, discard buffer
 */
bool dali_101_get(struct dali_rx_frame* frame, uint32_t wait_ms, bool forever);

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

/* callback functions defined by the low level driver
 *  to be called by the board interface module
 */
void dali_tx_irq_callback(void);
void dali_rx_irq_capture_callback(void);
void dali_rx_irq_stopbit_match_callback(void);
void dali_rx_irq_period_match_callback(void);
void dali_rx_irq_query_match_callback(void);