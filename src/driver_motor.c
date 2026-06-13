#include "driver_motor.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "DRIVER_MOTOR";

/* Định nghĩa chân GPIO cho motor driver (L298N hoặc tương tự) */
#define MOTOR_IN1  GPIO_NUM_4
#define MOTOR_IN2  GPIO_NUM_16
#define MOTOR_IN3  GPIO_NUM_17
#define MOTOR_IN4  GPIO_NUM_5

/* ENA/ENB của L298N nối cố định lên enable -> điều khiển tốc độ
 * bằng PWM ngay trên chân IN (chân nào HIGH sẽ chạy PWM, chân kia giữ LOW) */
#define PWM_FREQ_HZ     5000
#define PWM_RESOLUTION  LEDC_TIMER_8_BIT  /* duty 0-255 */
#define PWM_MAX_DUTY    255

/* Tăng/giảm tốc dần để tránh giật: mỗi RAMP_INTERVAL_MS thay đổi RAMP_STEP đơn vị duty */
#define RAMP_INTERVAL_MS  20
#define RAMP_STEP         8

/* Duty khởi động tối thiểu: khi bắt đầu chạy từ 0, nhảy ngay lên mức này
 * trước khi ramp tiếp, để vượt qua lực ma sát tĩnh của động cơ -> nhấn
 * nhanh vẫn làm xe chạy được, không bị "không chạy" như khi ramp từ 0 */
#define MIN_START_DUTY    100

static const ledc_channel_t s_channels[4] = {
    LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3
};
static const gpio_num_t s_pins[4] = { MOTOR_IN1, MOTOR_IN2, MOTOR_IN3, MOTOR_IN4 };

static int s_current_duty[4] = { 0, 0, 0, 0 };
static int s_target_duty[4]  = { 0, 0, 0, 0 };

static esp_timer_handle_t s_ramp_timer;
static motor_cmd_t s_last_cmd = MOTOR_CMD_STOP;

static void ramp_timer_cb(void *arg)
{
    for (int i = 0; i < 4; i++) {
        if (s_current_duty[i] < s_target_duty[i]) {
            s_current_duty[i] += RAMP_STEP;
            if (s_current_duty[i] > s_target_duty[i]) s_current_duty[i] = s_target_duty[i];
        } else if (s_current_duty[i] > s_target_duty[i]) {
            s_current_duty[i] -= RAMP_STEP;
            if (s_current_duty[i] < s_target_duty[i]) s_current_duty[i] = s_target_duty[i];
        } else {
            continue;
        }
        ledc_set_duty(LEDC_LOW_SPEED_MODE, s_channels[i], s_current_duty[i]);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, s_channels[i]);
    }
}

esp_err_t driver_motor_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    for (int i = 0; i < 4; i++) {
        ledc_channel_config_t ch_conf = {
            .gpio_num   = s_pins[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = s_channels[i],
            .timer_sel  = LEDC_TIMER_0,
            .duty       = 0,
            .hpoint     = 0,
        };
        ret = ledc_channel_config(&ch_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LEDC channel %d config failed: %s", i, esp_err_to_name(ret));
            return ret;
        }
    }

    const esp_timer_create_args_t timer_args = {
        .callback = ramp_timer_cb,
        .name     = "motor_ramp",
    };
    ret = esp_timer_create(&timer_args, &s_ramp_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ramp timer create failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_timer_start_periodic(s_ramp_timer, RAMP_INTERVAL_MS * 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ramp timer start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Motor driver initialized (PWM ramp)");
    return ESP_OK;
}

motor_cmd_t driver_motor_get_state(void)
{
    return s_last_cmd;
}

static void set_targets(int in1, int in2, int in3, int in4)
{
    int targets[4] = { in1, in2, in3, in4 };

    for (int i = 0; i < 4; i++) {
        s_target_duty[i] = targets[i];

        /* Nhảy ngay lên MIN_START_DUTY khi chuyển từ đứng yên sang chạy,
         * để xe phản hồi ngay cả khi nhấn rất nhanh */
        if (s_current_duty[i] == 0 && targets[i] > MIN_START_DUTY) {
            s_current_duty[i] = MIN_START_DUTY;
            ledc_set_duty(LEDC_LOW_SPEED_MODE, s_channels[i], s_current_duty[i]);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, s_channels[i]);
        }
    }
}

void driver_motor_handle_cmd(motor_cmd_t cmd)
{
    s_last_cmd = cmd;

    switch (cmd) {
        case MOTOR_CMD_FORWARD:
        case MOTOR_CMD_RUN:
            ESP_LOGI(TAG, "Forward");
            set_targets(0, PWM_MAX_DUTY, 0, PWM_MAX_DUTY);
            break;

        case MOTOR_CMD_BACKWARD:
            ESP_LOGI(TAG, "Backward");
            set_targets(PWM_MAX_DUTY, 0, PWM_MAX_DUTY, 0);
            break;

        case MOTOR_CMD_LEFT:
            ESP_LOGI(TAG, "Turn left");
            set_targets(0, PWM_MAX_DUTY, PWM_MAX_DUTY, 0);
            break;

        case MOTOR_CMD_RIGHT:
            ESP_LOGI(TAG, "Turn right");
            set_targets(PWM_MAX_DUTY, 0, 0, PWM_MAX_DUTY);
            break;

        case MOTOR_CMD_STOP:
        default:
            ESP_LOGI(TAG, "Stop");
            set_targets(0, 0, 0, 0);
            break;
    }
}
