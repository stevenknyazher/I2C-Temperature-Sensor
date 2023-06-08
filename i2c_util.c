#include "i2c_util.h"

#include <stdint.h>

#include "config.h"
#include "util.h"

/**
 * @brief Sets the I2C configuration for the specified pins.
 *
 * This function sets the I2C configuration for the specified pins by setting
 * the GPIO function to I2C and optionally pulling up the pins if the I2C
 * configuration has pull-up resistors.
 *
 * @param i2c A pointer to the I2CConfig struct containing the SDA and SCL pin
 * numbers, and the pull-up configuration.
 * @param len The number of I2C configurations in the i2c array.
 *
 * @return None.
 */
void set_i2c(const I2CConfig *i2c, size_t len) {
  for (size_t i = 0; i < len; i++) {
    gpio_set_function(i2c->sda_pin_number, GPIO_FUNC_I2C);
    gpio_set_function(i2c->scl_pin_number, GPIO_FUNC_I2C);
    if (i2c->has_pullup) {
      gpio_pull_up(i2c->sda_pin_number);
      gpio_pull_up(i2c->scl_pin_number);
    }
  }
}

/**
 * @brief Writes data to a register over I2C.
 *
 * This function writes data to a register over I2C. The register address is
 * appended to the front of the data packet, and the resulting message is sent
 * to the specified I2C address. The function returns the number of bytes
 * written.
 *
 * @param i2c_inst A pointer to the I2C instance to use for the write.
 * @param addr The I2C address to write to.
 * @param reg The register address to write to.
 * @param buf A pointer to the buffer containing the data to write.
 * @param nbytes The number of bytes to write to the register.
 *
 * @return The number of bytes written to the register.
 */
int reg_write(i2c_inst_t *i2c_inst, const uint8_t addr, const uint8_t reg,
              uint8_t *buf, const uint8_t nbytes) {
  int num_bytes_read = 0;
  uint8_t msg[nbytes + 1];

  msg[0] = reg;
  for (int i = 0; i < nbytes; i++) {
    msg[i + 1] = buf[i];
  }

  num_bytes_read = i2c_write_blocking(i2c_inst, addr, msg, (nbytes + 1), false);
  
  return num_bytes_read;
}

/**
 * @brief Reads data from a register over I2C.
 *
 * This function reads data from a register over I2C. The register address is
 * sent to the specified I2C address, and the resulting data is read into the
 * provided buffer. The function returns the number of bytes read.
 *
 * @param i2c_inst A pointer to the I2C instance to use for the read.
 * @param addr The I2C address to read from.
 * @param reg The register address to read from.
 * @param buf A pointer to the buffer to store the read data.
 * @param nbytes The number of bytes to read from the register.
 *
 * @return The number of bytes read from the register.
 */
int reg_read(i2c_inst_t *i2c_inst, const uint8_t addr, const uint8_t reg,
             uint8_t *buf, const uint8_t nbytes) {
  int num_bytes_read = 0;

  if (nbytes < 1) {
    return 0;
  }

  i2c_write_blocking(i2c_inst, addr, &reg, 1, true);
  num_bytes_read = i2c_read_blocking(i2c_inst, addr, buf, nbytes, false);

  return num_bytes_read;
}

/**
 * @brief Checks if an I2C device is present at the specified address.
 *
 * This function checks if an I2C device is present at the specified address by
 * attempting to read one byte of data from the device. The function returns the
 * number of bytes read (should be 1 if the device is present), or an error code
 * if the read operation timed out or encountered an error.
 *
 * @param i2c A pointer to the I2C instance to use for the read.
 * @param addr The I2C address to check for the presence of a device.
 * @param rxdata A pointer to the buffer to store the read data.
 * @param timeout The timeout (in microseconds) for the read operation.
 *
 * @return The number of bytes read (should be 1 if the device is present), or
 * an error code if the read operation timed out or encountered an error.
 */
int check_addr(i2c_inst_t *i2c, uint8_t addr, uint8_t *rxdata, uint timeout) {
  int ret = i2c_read_timeout_us(i2c, addr, rxdata, 1, false, timeout);
  return ret;
}

/**
 * @brief Scans the I2C bus for devices.
 *
 * This function scans the I2C bus for devices by attempting to read one byte of
 * data from each possible address on the bus. For each address, the function
 * prints a character to the console indicating whether a device was detected at
 * that address or not. The function skips over any reserved addresses.
 *
 * @param i2c A pointer to the I2C instance to use for the scan.
 * @param timeout The timeout (in microseconds) for the check_addr function.
 *
 * @return None.
 */
void scan_i2c_bus(i2c_inst_t *i2c, uint timeout) {
  printf("\nI2C Bus Scan\n");
  printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

  for (uint8_t addr = 0; addr < (1 << 7); ++addr) {
    if (addr % 16 == 0) {
      printf("%02x ", addr);
    }

    int ret;
    uint8_t rxdata;
    if (reserved_addr(addr)) {
      printf("X");
    } else {
      ret = check_addr(i2c, addr, &rxdata, timeout);
      printf(ret <= 0 ? "." : "@");
    }

    printf(addr % 16 == 15 ? "\n" : "  ");
  }
  printf("Done.\n");
}

/**
 * @brief Checks if an I2C address is a reserved address.
 *
 * This function checks if an I2C address is a reserved address. A reserved
 * address is an address that has the form 0000xxx or 1111xxx, where xxx can be
 * any value. Reserved addresses are not used by any I2C device, and attempting
 * to communicate with a device at a reserved address may cause conflicts on the
 * I2C bus.
 *
 * @param addr The I2C address to check.
 *
 * @return true if the address is reserved, false otherwise.
 */
bool reserved_addr(uint8_t addr) {
  if ((addr & 0x78) == 0 || (addr & 0x78) == 0x78) {
    return 1;
  } else {
    return 0;
  }
}

/**
 * @brief Reads data from a register on a temperature sensor over I2C.
 *
 * This function reads data from a register on a temperature sensor over I2C.
 * The function calls the reg_read function with the specified I2C instance,
 * device address, register address, buffer, and number of bytes to read.
 *
 * @param i2c A pointer to the I2C instance to use for the read.
 * @param dev_addr The I2C address of the temperature sensor.
 * @param reg_addr The register address to read from.
 * @param buf A pointer to the buffer to store the read data.
 * @param nbytes The number of bytes to read from the register.
 *
 * @return None.
 */
void read_temp_reg(i2c_inst_t *i2c, uint8_t dev_addr, uint8_t reg_addr,
                   uint8_t *buf, uint8_t nbytes) {
  reg_read(i2c, dev_addr, reg_addr, buf, nbytes);
}

/**
 * @brief Writes data to a register on a temperature sensor over I2C.
 *
 * This function writes data to a register on a temperature sensor over I2C. The
 * function calls the reg_write function with the specified I2C instance, device
 * address, register address, buffer, and number of bytes to write.
 *
 * @param i2c A pointer to the I2C instance to use for the write.
 * @param dev_addr The I2C address of the temperature sensor.
 * @param reg_addr The register address to write to.
 * @param buf A pointer to the buffer containing the data to write.
 * @param nbytes The number of bytes to write to the register.
 *
 * @return None.
 */
void write_temp_reg(i2c_inst_t *i2c, uint8_t dev_addr, uint8_t reg_addr,
                    uint8_t *buf, uint8_t nbytes) {
  reg_write(i2c, dev_addr, reg_addr, buf, nbytes);
}

/**
 * @brief Reads temperature data from a pair of registers on a temperature
 * sensor over I2C.
 *
 * This function reads temperature data from a pair of registers on a
 * temperature sensor over I2C. The function calls the read_temp_reg function
 * with the specified I2C instance, device address, register address, buffer,
 * and number of bytes to read. The function then prints a message to the
 * console, and calls the print_temp_table function to display the temperature
 * data in a formatted table.
 *
 * @param i2c A pointer to the I2C instance to use for the read.
 * @param dev_addr The I2C address of the temperature sensor.
 * @param reg_addr The register address to read from.
 * @param buf A pointer to the buffer to store the read data.
 * @param nbytes The number of bytes to read from the register.
 * @param message The message to display before the temperature table.
 *
 * @return None.
 */
void read_temperature_registers(i2c_inst_t *i2c, uint8_t dev_addr,
                                uint8_t reg_addr, uint8_t *buf, uint8_t nbytes,
                                const char *message) {
  read_temp_reg(i2c, dev_addr, reg_addr, buf, nbytes);
  printf("%s\n", message);
  print_temp_table(buf[0], buf[1]);
}

/**
 * @brief Reads and displays the ambient temperature from a temperature sensor
 * over I2C.
 *
 * This function reads the ambient temperature from a temperature sensor over
 * I2C by calling the read_temperature_registers function with the specified I2C
 * instance, device address, register address, buffer, number of bytes to read,
 * and message. The function then clears the console screen and displays the
 * temperature data in a formatted table.
 *
 * @param i2c A pointer to the I2C instance to use for the read.
 * @param dev_addr The I2C address of the temperature sensor.
 *
 * @return None.
 */
void print_ambient_temperature(i2c_inst_t *i2c, uint8_t dev_addr) {
  uint8_t nbytes = 2;
  uint8_t tmp[2] = {0, 0};
  clear_screen();
  read_temperature_registers(i2c, dev_addr, AMBIENT_TEMP_REG, tmp, nbytes,
                             "Ambient Temperature");
}

/**
 * @brief Reads and displays the temperature hysteresis limit from a temperature
 * sensor over I2C.
 *
 * This function reads the temperature hysteresis limit from a temperature
 * sensor over I2C by calling the read_temperature_registers function with the
 * specified I2C instance, device address, register address, buffer, number of
 * bytes to read, and message. The function then displays the temperature data
 * in a formatted table.
 *
 * @param i2c A pointer to the I2C instance to use for the read.
 * @param dev_addr The I2C address of the temperature sensor.
 *
 * @return None.
 */
void read_temp_hyst_limit(i2c_inst_t *i2c, uint8_t dev_addr) {
  uint8_t nbytes = 2;
  uint8_t tmp[2] = {0, 0};
  read_temperature_registers(i2c, dev_addr, TEMP_HYST_MIN_REG, tmp, nbytes,
                       "Temperature Hyst Limit");
}

/**
 * @brief Sets the temperature hysteresis limit on a temperature sensor over
 * I2C.
 *
 * This function sets the temperature hysteresis limit on a temperature sensor
 * over I2C by writing the specified integer and decimal parts to the
 * appropriate registers using the write_temp_reg function. The function then
 * reads and displays the updated temperature hysteresis limit using the
 * read_temp_hyst_limit function.
 *
 * @param i2c A pointer to the I2C instance to use for the write.
 * @param dev_addr The I2C address of the temperature sensor.
 * @param integer_part The integer part of the temperature hysteresis limit.
 * @param decimal_part The decimal part of the temperature hysteresis limit.
 *
 * @return None.
 */
void write_temp_hyst_limit(i2c_inst_t *i2c, uint8_t dev_addr,
                           uint8_t integer_part, uint8_t decimal_part) {
  uint8_t nbytes = 2;
  uint8_t tmp[2] = {[0]=integer_part, [1]=decimal_part};
  write_temp_reg(i2c, dev_addr, TEMP_HYST_MIN_REG, tmp, nbytes);
  read_temp_hyst_limit(i2c, dev_addr);
}

/**
 * @brief Reads and displays the temperature set limit from a temperature sensor
 * over I2C.
 *
 * This function reads the temperature set limit from a temperature sensor over
 * I2C by calling the read_temperature_registers function with the specified I2C
 * instance, device address, register address, buffer, number of bytes to read,
 * and message. The function then displays the temperature data in a formatted
 * table.
 *
 * @param i2c A pointer to the I2C instance to use for the read.
 * @param dev_addr The I2C address of the temperature sensor.
 *
 * @return None.
 */
void read_temp_set_limit(i2c_inst_t *i2c, uint8_t dev_addr) {
  uint8_t nbytes = 2;
  uint8_t tmp[2] = {0, 0};
  read_temperature_registers(i2c, dev_addr, TEMP_SET_MAX_REG, tmp, nbytes,
                             "Temperature Set Limit");
}

/**
 * @brief Sets the temperature set limit on a temperature sensor over I2C.
 *
 * This function sets the temperature set limit on a temperature sensor over I2C
 * by writing the specified integer and decimal parts to the appropriate
 * registers using the write_temp_reg function. The function then reads and
 * displays the updated temperature set limit using the read_temp_set_limit
 * function.
 *
 * @param i2c A pointer to the I2C instance to use for the write.
 * @param dev_addr The I2C address of the temperature sensor.
 * @param integer_part The integer part of the temperature set limit.
 * @param decimal_part The decimal part of the temperature set limit.
 *
 * @return None.
 */
void write_temp_set_limit(i2c_inst_t *i2c, uint8_t dev_addr,
                          uint8_t integer_part, uint8_t decimal_part) {
  uint8_t nbytes = 2;
  uint8_t tmp[2] = {[0]=integer_part, [1]=decimal_part};
  write_temp_reg(i2c, dev_addr, TEMP_SET_MAX_REG, tmp, nbytes);
  read_temp_set_limit(i2c, dev_addr);
}

/**
 * @brief Reads and returns the configuration register value from a temperature
 * sensor over I2C.
 *
 * This function reads the configuration register value from a temperature
 * sensor over I2C by calling the read_temp_reg function with the specified I2C
 * instance, device address, register address, buffer, and number of bytes to
 * read.
 *
 * @param i2c A pointer to the I2C instance to use for the read.
 * @param dev_addr The I2C address of the temperature sensor.
 *
 * @return The value of the configuration register.
 */
uint8_t read_config(i2c_inst_t *i2c, uint8_t dev_addr) {
  uint8_t nbytes = 1;
  uint8_t tmp[1] = {0};
  read_temp_reg(i2c, dev_addr, SENSOR_CONFIG_REG, tmp, nbytes);
  return tmp[0];
}

/**
 * @brief Writes the configuration register value to a temperature sensor over
 * I2C.
 *
 * This function writes the specified configuration register value to a
 * temperature sensor over I2C by calling the write_temp_reg function with the
 * specified I2C instance, device address, register address, buffer, and number
 * of bytes to write.
 *
 * @param i2c A pointer to the I2C instance to use for the write.
 * @param dev_addr The I2C address of the temperature sensor.
 * @param conf The value to write to the configuration register.
 *
 * @return Always returns 0.
 */
uint8_t write_config(i2c_inst_t *i2c, uint8_t dev_addr, uint8_t conf) {
  uint8_t nbytes = 1;
  uint8_t tmp[1] = {conf};
  write_temp_reg(i2c, dev_addr, SENSOR_CONFIG_REG, tmp, nbytes);
  return 0;
}

/**
 * @brief Prints a temperature data table to the console.
 *
 * This function takes the integer and decimal parts of a temperature value,
 * converts it to Celsius and Fahrenheit, and then prints a formatted
 * temperature data table to the console with the Celsius and Fahrenheit values.
 *
 * @param integer_part The integer part of the temperature value.
 * @param decimal_part The decimal part of the temperature value.
 *
 * @return None.
 */
void print_temp_table(uint8_t integer_part, uint8_t decimal_part) {
  printf("%-8s--%-8s\n", "-------", "-------");
  printf("%-8s| %-8s\n", "Temp C", "Temp F");
  printf("%-8s+ %-8s\n", "-------", "-------");
  float celsius = fixedToFloat(integer_part, decimal_part);
  printf("%-8.4f| %-8.4f\n", celsius, c2f(celsius));
}
