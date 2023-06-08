#include "core1.h"

#include <stdio.h>

#include "config.h"
#include "debounce.h"
#include "globals.h"
#include "gpio_util.h"
#include "menu_handler.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

/**
 * @brief Entry point for core1.
 *
 * This function is the entry point for the second core. It continuously
 * monitors an input GPIO pin for any alerts, and updates two output GPIO pins
 * to indicate the alert status. It also checks the multicore FIFO for any
 * pending requests, and calls the `handle_request()` function to handle them.
 *
 * @return void
 */
void core1_entry() {
    uint32_t request = 0xFFFFFFFF;

    while (1) {
        bool has_alert_N = !gpio_get(ALERT_GP);
        gpio_put(LED1, has_alert_N);
        gpio_put(LED0, !has_alert_N);

        if (multicore_fifo_rvalid()) {
            uint32_t request = multicore_fifo_pop_blocking();
            handle_request(request);
        }
    }
}

/**
 * @brief Handle the specified request.
 *
 * This function takes a request code as a parameter and performs the
 * appropriate action based on the request. The supported requests and their
 * corresponding actions are as follows:
 * - SCAN_I2C_BUS: Shows the landing page and scans the I2C bus for devices.
 * - SHOW_CONFIG: Displays the configuration settings.
 * - SHOW_DEV_ID: Displays the device ID.
 * - SHOW_ALERT_MENU: Displays the alert menu.
 *
 * If the request is not one of the above, the function does nothing.
 *
 * @param request The request code to handle.
 * @return void
 */
void handle_request(uint32_t request) {
    switch (request) {
        case SCAN_I2C_BUS:
            show_landing_page();
            scan_i2c_bus(i2c, I2C_READ_TIMEOUT_MICRO_SEC);
            multicore_fifo_drain();
            break;
        case SHOW_CONFIG:
            handle_show_config();
            break;
        case SHOW_DEV_ID:
            handle_show_dev_id();
            break;
        case SHOW_ALERT_MENU:
            handle_show_alert_menu();
            break;
        default:
            break;
    }
}

/**
 * @brief Displays the configuration menu and handles the user's configuration
 * choice.
 *
 * This function displays the configuration menu and waits for the user to make
 * a configuration choice. It then applies the corresponding configuration
 * change to the device, and displays the updated configuration status. The
 * supported configuration options and their corresponding bit masks are as
 * follows:
 * - Shutdown mode: SHUTDOWN_MODE_REQ_MASK
 * - Comparator/Interrupt mode: COMP_INT_MODE_REQ_MASK
 * - Alert polarity: ALERT_POLARITY_REQ_MASK
 * - Fault queue mode: FAULT_QUEUE_MODE_REQ_MASK
 * - ADC resolution: ADC_RESOLUTION_REQ_MASK
 * - One-shot mode: ONE_SHOT_MODE_REQ_MASK
 *
 * If the user does not make a valid configuration choice, or chooses to make no
 * change, the function does nothing. Additionally, the function disables
 * interrupts while the configuration menu is displayed and changes are applied,
 * to ensure data consistency.
 *
 * @return void
 */
void handle_show_config() {
    multicore_fifo_push_blocking(DISABLE_IRQ);
    uint32_t user_config_result = show_config_menu();
    uint8_t curr_config_result = read_config(i2c, dev_addr);
    uint8_t new_config_result = 0x0;
    bool has_new_change = true;
    if (user_config_result & NO_CHANGE_SHIFT) {
        has_new_change = false;
    } else if (user_config_result & SHUTDOWN_MODE_SHIFT) {
        new_config_result = (curr_config_result & ~SHUTDOWN_MODE_REQ_MASK) |
                            (user_config_result & SHUTDOWN_MODE_REQ_MASK);
    } else if (user_config_result & COMP_INT_MODE_SHIFT) {
        new_config_result = (curr_config_result & ~COMP_INT_MODE_REQ_MASK) |
                            (user_config_result & COMP_INT_MODE_REQ_MASK);
    } else if (user_config_result & ALERT_POLARITY_SHIFT) {
        new_config_result = (curr_config_result & ~ALERT_POLARITY_REQ_MASK) |
                            (user_config_result & ALERT_POLARITY_REQ_MASK);
    } else if (user_config_result & FAULT_QUEUE_MODE_SHIFT) {
        new_config_result = (curr_config_result & ~FAULT_QUEUE_MODE_REQ_MASK) |
                            (user_config_result & FAULT_QUEUE_MODE_REQ_MASK);
    } else if (user_config_result & ADC_RESOLUTION_SHIFT) {
        new_config_result = (curr_config_result & ~ADC_RESOLUTION_REQ_MASK) |
                            (user_config_result & ADC_RESOLUTION_REQ_MASK);
    } else if (user_config_result & ONE_SHOT_MODE_SHIFT) {
        new_config_result = (curr_config_result & ~ONE_SHOT_MODE_REQ_MASK) |
                            (user_config_result & ONE_SHOT_MODE_REQ_MASK);
    } else {
        has_new_change = false;
    }

    if (has_new_change) {
        write_config(i2c, dev_addr, new_config_result);
    }

    show_landing_page();
    uint8_t config_result = read_config(i2c, dev_addr);
    printf("Sensor Config Status\n");
    parse_config(config_result);
    multicore_fifo_push_blocking(ENABLE_IRQ);
}

/**
 * @brief Displays the device ID change menu and handles the user's device ID
 * choice.
 *
 * This function displays the device ID change menu and waits for the user to
 * enter a new device ID. It then checks if the device is accessible using the
 * new ID, and if successful, changes the device ID and displays a success
 * message. If the device is not accessible, the function displays a warning
 * message. Additionally, the function disables interrupts while the device ID
 * change menu is displayed, to ensure data consistency.
 *
 * @return void
 */
void handle_show_dev_id() {
    multicore_fifo_push_blocking(DISABLE_IRQ);
    uint8_t addr = show_dev_change_menu(TCN75A_DEFAULT_ADDR);
    show_landing_page();
    multicore_fifo_push_blocking(ENABLE_IRQ);

    if (addr != 0) {
        uint8_t rxdata;
        int ret = check_addr(i2c, addr, &rxdata, I2C_READ_TIMEOUT_MICRO_SEC);

        if (ret > 0) {
            dev_addr = addr;
            printf("[SUCCCESS] Dev ID changed to 0x%x\n", addr);
        } else {
            printf("[WARNING] Could not communicate with Dev ID 0x%x\n", addr);
        }
    }

    multicore_fifo_drain();
}

/**
 * @brief Displays the alert menu and handles the user's alert choice.
 *
 * This function displays the alert menu and waits for the user to make an alert
 * choice. It then performs the corresponding action based on the choice made by
 * the user. The supported alert choices and their corresponding actions are as
 * follows:
 * - READ_TEMP_HYST_LIMIT: Reads the temperature hysteresis limit.
 * - READ_TEMP_SET_LIMIT: Reads the temperature set limit.
 * - WRITE_TEMP_HYST_LIMIT: Writes the temperature hysteresis limit using the
 * two-byte buffer passed by the user.
 * - WRITE_TEMP_SET_LIMIT: Writes the temperature set limit using the two-byte
 * buffer passed by the user.
 *
 * If the user does not make a valid alert choice, or chooses to perform no
 * action, the function does nothing. Additionally, the function disables
 * interrupts while the alert menu is displayed and actions are performed, to
 * ensure data consistency.
 *
 * @return void
 */
void handle_show_alert_menu() {
    multicore_fifo_push_blocking(DISABLE_IRQ);
    uint8_t buf[2];
    uint32_t result = show_alert_menu(buf);
    show_landing_page();

    if ((result & READ_TEMP_HYST_LIMIT) && READ_TEMP_HYST_LIMIT) {
        read_temp_hyst_limit(i2c, dev_addr);
    } else if ((result & READ_TEMP_SET_LIMIT) && READ_TEMP_SET_LIMIT) {
        read_temp_set_limit(i2c, dev_addr);
    } else if ((result & WRITE_TEMP_HYST_LIMIT) && WRITE_TEMP_HYST_LIMIT) {
        write_temp_hyst_limit(i2c, dev_addr, buf[0], buf[1]);
    } else if ((result & WRITE_TEMP_SET_LIMIT) && WRITE_TEMP_SET_LIMIT) {
        write_temp_set_limit(i2c, dev_addr, buf[0], buf[1]);
    }

    multicore_fifo_push_blocking(ENABLE_IRQ);
}
