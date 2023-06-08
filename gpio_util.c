#include "gpio_util.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/regs/intctrl.h"
#include <stddef.h>

/**
 * Initializes and sets the direction of the GPIO pins based on the provided
 * GpioConfig array. Additionally, enables the internal pull-up resistors for
 * the pins that have `has_pullup` set to `true`.
 *
 * @param gpio Pointer to the GpioConfig array.
 * @param len Length of the GpioConfig array.
 */
void set_gpio(const GpioConfig *gpio, size_t len) {
  // Loop through the length of the GpioConfig array
  for (size_t i = 0; i < len; i++) {
    // Initialize the GPIO specified by the pin number in the current GpioConfig
    // structure
    gpio_init(gpio[i].pin_number);
    // Set the direction of the GPIO specified by the pin number and direction
    // in the current GpioConfig structure
    gpio_set_dir(gpio[i].pin_number, gpio[i].pin_dir);

    if (gpio[i].has_pullup) {
      gpio_pull_up(gpio[i].pin_number);
    }
  }
}

/**
 * Enables interrupts on the GPIO pins specified in the GpioConfig array that
 * have `has_irq` set to `true`. Also enables the GPIO IRQ bank.
 *
 * @param gpio Pointer to the GpioConfig array.
 * @param len Length of the GpioConfig array.
 */
void enable_irq(const GpioConfig *gpio, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (gpio[i].has_irq) {
      gpio_set_irq_enabled(gpio[i].pin_number, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
      irq_set_enabled(IO_IRQ_BANK0, true);
    }
  }
}

/**
 * Disables interrupts on the GPIO pins specified in the GpioConfig array that
 * have `has_irq` set to `true`. Also disables the GPIO IRQ bank.
 *
 * @param gpio Pointer to the GpioConfig array.
 * @param len Length of the GpioConfig array.
 */
void disable_irq(const GpioConfig *gpio, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (gpio[i].has_irq) {
      gpio_set_irq_enabled(gpio[i].pin_number, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
      irq_set_enabled(IO_IRQ_BANK0, false);
    }
  }
}
