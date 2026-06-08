#include "driver_motor.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "DRIVER_MOTOR";

/* Định nghĩa chân GPIO cho motor driver (L298N hoặc tương tự) */
#define MOTOR_IN1  GPIO_NUM_4
#define MOTOR_IN2  GPIO_NUM_16
#define MOTOR_IN3  GPIO_NUM_17
#define MOTOR_IN4  GPIO_NUM_5

esp_err_t driver_motor_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MOTOR_IN1) | (1ULL << MOTOR_IN2) |
                        (1ULL << MOTOR_IN3) | (1ULL << MOTOR_IN4),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Dừng motor khi khởi động */
    gpio_set_level(MOTOR_IN1, 0);
    gpio_set_level(MOTOR_IN2, 0);
    gpio_set_level(MOTOR_IN3, 0);
    gpio_set_level(MOTOR_IN4, 0);

    ESP_LOGI(TAG, "Motor driver initialized");
    return ESP_OK;
}

void driver_motor_handle_cmd(motor_cmd_t cmd)
{
    switch (cmd) {
        case MOTOR_CMD_FORWARD:
            ESP_LOGI(TAG, "Forward");
            gpio_set_level(MOTOR_IN1, 0);
            gpio_set_level(MOTOR_IN2, 1);
            gpio_set_level(MOTOR_IN3, 0);
            gpio_set_level(MOTOR_IN4, 1);
            break;

        case MOTOR_CMD_BACKWARD:
            ESP_LOGI(TAG, "Backward");
            gpio_set_level(MOTOR_IN1, 1);
            gpio_set_level(MOTOR_IN2, 0);
            gpio_set_level(MOTOR_IN3, 1);
            gpio_set_level(MOTOR_IN4, 0);
            break;

        case MOTOR_CMD_LEFT:
            ESP_LOGI(TAG, "Turn left");
            gpio_set_level(MOTOR_IN1, 0);
            gpio_set_level(MOTOR_IN2, 1);
            gpio_set_level(MOTOR_IN3, 1);
            gpio_set_level(MOTOR_IN4, 0);
            break;

        case MOTOR_CMD_RIGHT:
            ESP_LOGI(TAG, "Turn right");
            gpio_set_level(MOTOR_IN1, 1);
            gpio_set_level(MOTOR_IN2, 0);
            gpio_set_level(MOTOR_IN3, 0);
            gpio_set_level(MOTOR_IN4, 1);
            break;

        case MOTOR_CMD_RUN:
            ESP_LOGI(TAG, "Run");
            gpio_set_level(MOTOR_IN1, 0);
            gpio_set_level(MOTOR_IN2, 1);
            gpio_set_level(MOTOR_IN3, 0);
            gpio_set_level(MOTOR_IN4, 1);
            break;

        case MOTOR_CMD_STOP:
        default:
            ESP_LOGI(TAG, "Stop");
            gpio_set_level(MOTOR_IN1, 0);
            gpio_set_level(MOTOR_IN2, 0);
            gpio_set_level(MOTOR_IN3, 0);
            gpio_set_level(MOTOR_IN4, 0);
            break;
    }
}
