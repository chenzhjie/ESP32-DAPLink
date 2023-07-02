#include "cdc_uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

typedef struct
{
    uart_port_t uart;
    cdc_uart_rx_callback_t callback;
    void *usr_data;
} cdc_uart_t;

static cdc_uart_t s_cdc_uart = {0};
static const char *TAG = "cdc_uart";
static void cdc_uart_rx_task(void *param);

bool cdc_uart_init(uart_port_t uart, gpio_num_t tx_pin, gpio_num_t rx_pin, int baudrate)
{
    bool ret = false;
    uart_config_t uart_config = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    s_cdc_uart.uart = uart;
    ret = (ESP_OK == uart_driver_install(s_cdc_uart.uart, 2 * 1024, 0, 0, NULL, 0));
    ret = ret && (ESP_OK == uart_param_config(s_cdc_uart.uart, &uart_config));
    ret = ret && (ESP_OK == uart_set_pin(s_cdc_uart.uart, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xTaskCreate(cdc_uart_rx_task, "cdc_uart_rx_task", 4096, (void *)&s_cdc_uart, 10, NULL);

    return ret;
}

bool cdc_uart_set_baudrate(uint32_t baudrate)
{
    return (ESP_OK == uart_set_baudrate(s_cdc_uart.uart, baudrate));
}

bool cdc_uart_get_baudrate(uint32_t *baudrate)
{
    return (ESP_OK == uart_get_baudrate(s_cdc_uart.uart, baudrate));
}

bool cdc_uart_write(const void *src, size_t size)
{
    return (0 <= uart_write_bytes(s_cdc_uart.uart, src, size));
}

void cdc_uart_register_rx_callback(cdc_uart_rx_callback_t callback, void *usr_data)
{
    if (callback)
    {
        s_cdc_uart.usr_data = usr_data;
        s_cdc_uart.callback = callback;
    }
}

static void cdc_uart_rx_task(void *param)
{
#define RX_BUF_SIZE 64

    int offset = 0;
    int need = 0;
    int read = 0;
    uint8_t data[RX_BUF_SIZE] = {0};
    cdc_uart_t *cdc_uart = (cdc_uart_t *)param;

    for (;;)
    {
        need = RX_BUF_SIZE - offset;
        read = uart_read_bytes(cdc_uart->uart, data + offset, need, pdMS_TO_TICKS(5));

        if (read < 0)
        {
            ESP_LOGE(TAG, "uart_read_bytes() failed: %d", read);
        }
        else
        {
            offset += read;

            if ((offset == RX_BUF_SIZE) || ((read == 0) && (offset > 0)))
            {
                if (cdc_uart->callback)
                {
                    cdc_uart->callback(cdc_uart->uart, cdc_uart->usr_data, data, offset);
                }

                offset = 0;
            }
        }
    }
}