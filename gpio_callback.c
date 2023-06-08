#include <stdio.h>

#include "config.h"
#include "debounce.h"
#include "globals.h"
#include "gpio_util.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "i2c_util.h"
#include "menu_handler.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "util.h"

/**
@brief Callback function for GPIO interrupts on button presses
This function handles the GPIO interrupt for button presses and debounces the
input. It determines which button was pressed and performs the corresponding
action, such as scanning the I2C bus or showing device information. The function
keeps track of the button state and only triggers the
action when the button is stable. If the button is unstable or the input is
invalid, the function returns without taking any action.

@param gpio The GPIO pin number that triggered the interrupt
@param events The type of interrupt event (e.g., GPIO_IRQ_EDGE_RISE,
GPIO_IRQ_EDGE_FALL)
@return void
*/
void gpio_callback(uint gpio, uint32_t events) {
    volatile BtnState *target_btn;
    uint32_t btn_action;
    if (gpio == BTN0) {
        target_btn = &btns[0];
        btn_action = SCAN_I2C_BUS;
    } else if (gpio == BTN1) {
        target_btn = &btns[1];
        btn_action = SHOW_CONFIG;
    } else if (gpio == BTN2) {
        target_btn = &btns[2];
        btn_action = SHOW_DEV_ID;
    } else if (gpio == BTN3) {
        target_btn = &btns[3];
        btn_action = SHOW_ALERT_MENU;
    } else if (gpio == BTN4) {
        target_btn = &btns[4];
        btn_action = SHOW_TEMP;
    } else {
        return;
    }

    if ((events & GPIO_IRQ_EDGE_RISE) && (events & GPIO_IRQ_EDGE_FALL)) {
        reset_btn_state(target_btn);
        return;
    } else if (events & GPIO_IRQ_EDGE_RISE) {
        set_rising_edge_state(target_btn);
        return;
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        set_falling_edge_state(target_btn);
    }

    bool is_btn_stable = debounce(*target_btn);

    if (is_btn_stable) {
        if (btn_action == SHOW_TEMP) {
            enable_read_temp = true;
            return;
        } else {
            enable_read_temp = false;
            multicore_fifo_drain();
            multicore_fifo_push_timeout_us(btn_action, 1000);
        }
    }
}
