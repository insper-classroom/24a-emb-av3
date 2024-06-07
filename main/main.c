/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

#include "hardware/adc.h"

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

typedef struct btn {
    int id;
    int status;
} btn_t;

QueueHandle_t xQueueADC;
QueueHandle_t xQueueBtn;
SemaphoreHandle_t xSemaphoreAlarmAdc;
SemaphoreHandle_t xSemaphoreAlarmEvent;

void gpio_callback(uint gpio, uint32_t events) {
    btn_t btn_data;
    btn_data.id = gpio;
    btn_data.status = events;
    xQueueSendFromISR(xQueueBtn, &btn_data, 0);
}

bool timer_0_callback(repeating_timer_t *rt) {
    adc_select_input(1); // Select ADC input 1 (GPIO27)
    int adc = adc_read();
    xQueueSendFromISR(xQueueADC, &adc, 0);
    return true; // keep repeating
}

void alarm_task(void *p) {
    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);
    gfx_clear_buffer(&disp);
    gfx_show(&disp);

    while (1) {
        uint32_t tick_s = to_ms_since_boot(get_absolute_time()) / 1000;

        if (xSemaphoreTake(xSemaphoreAlarmAdc, pdMS_TO_TICKS(10)) == pdTRUE) {
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "ADC ALARM");
            gfx_show(&disp);
            printf("[ALARM] %d s ADC\n", tick_s);
        }

        if (xSemaphoreTake(xSemaphoreAlarmEvent, pdMS_TO_TICKS(10)) == pdTRUE) {
            printf("[ALARM] %d s EVENT\n", tick_s);
            gfx_draw_string(&disp, 0, 18, 1, "EVENT ALARM");
            gfx_show(&disp);
        }
    }
}

void adc_task(void *p) {
    adc_init();
    adc_gpio_init(27);

    int timer_0_hz = 1;
    repeating_timer_t timer_0;

    if (!add_repeating_timer_us(1000000 / timer_0_hz,
                                timer_0_callback,
                                NULL,
                                &timer_0)) {
        printf("Failed to add timer\n");
    }

    int adc;
    int cnt = 0;
    while (1) {
        if (xQueueReceive(xQueueADC, &adc, pdMS_TO_TICKS(100))) {
            uint32_t tick_s = to_ms_since_boot(get_absolute_time()) / 1000;
            printf("[ADC] %d %d \n", tick_s, adc);

            if (adc > 3000)
                cnt++;
            else
                cnt = 0;

            if (cnt == 5) {
                xSemaphoreGive(xSemaphoreAlarmAdc);
            }
        }
    }
}

void event_task(void *p) {

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);
    gpio_set_irq_enabled_with_callback(BTN_1_OLED,
                                       GPIO_IRQ_EDGE_RISE |
                                           GPIO_IRQ_EDGE_FALL,
                                       true,
                                       &gpio_callback);

    gpio_init(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);
    gpio_set_irq_enabled_with_callback(BTN_2_OLED,
                                       GPIO_IRQ_EDGE_RISE |
                                           GPIO_IRQ_EDGE_FALL,
                                       true,
                                       &gpio_callback);

    gpio_init(BTN_2_OLED);

    btn_t btn_data;
    int id = 0;
    int status = 0;
    int btn_1 = 0;
    int btn_2 = 0;
    int send = 0;
    while (1) {
        if (xQueueReceive(xQueueBtn, &btn_data, portMAX_DELAY)) {
            uint32_t tick_s = to_ms_since_boot(get_absolute_time()) / 1000;

            if (btn_data.id == BTN_1_OLED) {
                if (btn_data.status == 4)
                    btn_1 = 1;
                else
                    btn_1 = 0;
                id = 1;
            } else {
                if (btn_data.status == 4)
                    btn_2 = 1;
                else
                    btn_2 = 0;
                id = 2;
            }

            if (btn_data.status == 4)
                status = 1;
            else
                status = 0;

            if (btn_data.id == BTN_1_OLED && btn_data.status == 4)
                btn_1 = 1;
            printf("%d %d \n", btn_1, btn_2);
            printf("[EVENT] %d s %d:%d\n", tick_s, id, status);
        }

        if (send == 0 && btn_1 == 1 && btn_2 == 1) {
            xSemaphoreGive(xSemaphoreAlarmEvent);
            send = 1;
        }
    }
}

int main() {
    stdio_init_all();

    // inicializa botões e leds
    xQueueADC = xQueueCreate(32, sizeof(int));
    xSemaphoreAlarmAdc = xSemaphoreCreateBinary();
    xSemaphoreAlarmEvent = xSemaphoreCreateBinary();
    xQueueBtn = xQueueCreate(32, sizeof(btn_t));

    xTaskCreate(adc_task, "Adc task", 4095, NULL, 1, NULL);
    xTaskCreate(alarm_task, "alarm task", 4095, NULL, 1, NULL);
    xTaskCreate(event_task, "event task", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
