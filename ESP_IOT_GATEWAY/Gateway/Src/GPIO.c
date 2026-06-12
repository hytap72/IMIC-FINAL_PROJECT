#include "global.h"

void init_hardware(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE, // Disable interrupt
        .mode = GPIO_MODE_OUTPUT,       // Set as output mode
        .pin_bit_mask = (1ULL << LED1_PIN) | (1ULL << RELAY_PIN) | (1ULL << LED2_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);

    // Set initial state to OFF (Logic level 0)
    gpio_set_level(LED1_PIN, 0);
    gpio_set_level(RELAY_PIN, 0);
    gpio_set_level(LED2_PIN, 0);
}
