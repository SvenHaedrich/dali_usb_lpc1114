#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "log/log.h"
#include "dali_101_lpc/dali_101.h"
#include "serial.h"
#include "frame_pump.h"

enum queue_element_type {
    TX_FRAME = 0,
    RX_FRAME
};

enum pump_status {
    OK = 0,
    FULL,
};

struct queue_element
{
    enum queue_element_type type;
    union { 
        struct dali_tx_frame tx_frame;
        struct dali_rx_frame rx_frame;
    } frame;
};

#define QUEUE_SIZE (10U)
#define QUEUE_ELEMENT_SIZE (sizeof(struct queue_element))

#define FRAME_PUMP_STACKSIZE (configMINIMAL_STACK_SIZE)
#define FRAME_PUMP_PRIORITY (1U)

struct frame_pump {
    uint8_t queue_storage [QUEUE_SIZE * QUEUE_ELEMENT_SIZE];
    QueueHandle_t queue_handle;
    StaticQueue_t queue_buffer;
    StackType_t task_stack[FRAME_PUMP_STACKSIZE];
    StaticTask_t task_buffer;
    TaskHandle_t task_handle;
    void (*process_tx)(const struct dali_tx_frame);
    void (*process_rx)(const struct dali_rx_frame);
    int status;
} pump = {0};

void frame_pump_enqueue_tx_frame(struct dali_tx_frame frame)
{
    const struct queue_element element = {
        .type = TX_FRAME,
        .frame.tx_frame = frame
    };
    const BaseType_t result = xQueueSendToBack(pump.queue_handle, (void*) &element, 0);
    if (result == errQUEUE_FULL) {
        pump.status = FULL;
    }
}

void frame_pump_enqueue_rx_frame(struct dali_rx_frame frame)
{
    const struct queue_element element = {
        .type = RX_FRAME,
        .frame.rx_frame = frame
    };
    const BaseType_t result = xQueueSendToBack(pump.queue_handle, (void*) &element, 0);
    if (result == errQUEUE_FULL) {
        pump.status = FULL;
    }
}

static void frame_pump_process_tx_frame(const struct dali_tx_frame frame) 
{
    if (pump.process_tx)  {
        pump.process_tx(frame);
    }
}

static void frame_pump_process_rx_frame(const struct dali_rx_frame frame) 
{
    if (pump.process_rx)  {
        pump.process_rx(frame);
    }
}

void frame_pump_set_tx_callback(void (*callback)(const struct dali_tx_frame))
{
    pump.process_tx = callback;
}

void frame_pump_set_rx_callback(void (*callback)(const struct dali_rx_frame))
{
    pump.process_rx = callback;
}

__attribute__((noreturn)) static void frame_pump_task (__attribute__((unused)) void *dummy)
{
    LOG_THIS_INVOCATION(LOG_FORCE);   
    while(true) {
        struct queue_element element;
        const BaseType_t result = xQueueReceive(pump.queue_handle, &element, portMAX_DELAY);
        if (result == pdPASS) {
            switch (element.type) {
                case TX_FRAME:
                    frame_pump_process_tx_frame(element.frame.tx_frame);
                    break;
                case RX_FRAME:
                    frame_pump_process_rx_frame(element.frame.rx_frame);
                    break;
                default:
                    LOG_ASSERT(false);
                    break;
            }
        }
    }
}

void frame_pump_init(void)
{
    LOG_THIS_INVOCATION(LOG_FORCE);
    LOG_TEST(pump.task_handle = xTaskCreateStatic(frame_pump_task, "PUMP", FRAME_PUMP_STACKSIZE,
    NULL, FRAME_PUMP_PRIORITY, pump.task_stack, &pump.task_buffer));

    pump.queue_handle = xQueueCreateStatic(
        QUEUE_SIZE, QUEUE_ELEMENT_SIZE, pump.queue_storage, &pump.queue_buffer);
    LOG_ASSERT(pump.queue_handle);

    // connect rx pipeline
    dali_rx_set_callback(frame_pump_enqueue_rx_frame);
    frame_pump_set_rx_callback(serial_print_frame);

}