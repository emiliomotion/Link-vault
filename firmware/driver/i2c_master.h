#pragma once
/*
 * driver/i2c_master.h  —  ESP-IDF 5.x I2C master API shim for ESP32 2.0.x (ESP-IDF 4.x)
 *
 * Waveshare's i2c_bsp files use the ESP-IDF 5.x i2c_master driver API
 * (<driver/i2c_master.h>).  ESP32 Arduino 2.0.x ships ESP-IDF 4.x which only
 * has the legacy <driver/i2c.h> API.  Place this file in the sketch's driver/
 * subdirectory; the sketch directory is on the compiler include path, so
 * #include <driver/i2c_master.h> resolves here instead of the missing system
 * header.  All ESP-IDF 5.x types and functions are mapped to their 4.x
 * equivalents inline — no extra .cpp file is needed.
 */

#include <driver/i2c.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Enumerations ─────────────────────────────────────────────────────────── */

typedef enum {
    I2C_CLK_SRC_DEFAULT = 0,
    I2C_CLK_SRC_XTAL    = 0,
} i2c_clock_source_t;

typedef enum {
    I2C_ADDR_BIT_LEN_7  = 0,
    I2C_ADDR_BIT_LEN_10 = 1,
} i2c_addr_bit_len_t;

/* ── Config structs ───────────────────────────────────────────────────────── */

typedef struct {
    i2c_clock_source_t clk_source;
    i2c_port_t         i2c_port;
    gpio_num_t         sda_io_num;
    gpio_num_t         scl_io_num;
    uint8_t            glitch_ignore_cnt;   /* ignored in shim */
    struct {
        uint32_t enable_internal_pullup : 1;
    } flags;
} i2c_master_bus_config_t;

typedef struct {
    i2c_addr_bit_len_t dev_addr_length;
    uint16_t           device_address;
    uint32_t           scl_speed_hz;
    uint32_t           scl_wait_us;         /* ignored in shim */
    struct {
        uint32_t disable_ack_check : 1;     /* ignored in shim */
    } flags;
} i2c_device_config_t;

/* ── Handle types ─────────────────────────────────────────────────────────── */

typedef struct {
    i2c_port_t port;
} i2c_master_bus_t;

typedef struct {
    i2c_port_t port;
    uint16_t   addr;
    uint32_t   speed_hz;
} i2c_master_dev_t;

typedef i2c_master_bus_t * i2c_master_bus_handle_t;
typedef i2c_master_dev_t * i2c_master_dev_handle_t;

/* ── API (inline) ─────────────────────────────────────────────────────────── */

static inline esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *cfg,
                                            i2c_master_bus_handle_t *out)
{
    i2c_config_t conf;
    conf.mode            = I2C_MODE_MASTER;
    conf.sda_io_num      = cfg->sda_io_num;
    conf.scl_io_num      = cfg->scl_io_num;
    conf.sda_pullup_en   = cfg->flags.enable_internal_pullup
                               ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    conf.scl_pullup_en   = cfg->flags.enable_internal_pullup
                               ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = 400000;   /* overridden per-device where possible */
    conf.clk_flags        = 0;

    esp_err_t ret = i2c_param_config(cfg->i2c_port, &conf);
    if (ret != ESP_OK) return ret;

    ret = i2c_driver_install(cfg->i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    /* ESP_ERR_INVALID_STATE means the driver is already installed — that's OK */
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;

    i2c_master_bus_t *bus = (i2c_master_bus_t *)malloc(sizeof(i2c_master_bus_t));
    if (!bus) return ESP_ERR_NO_MEM;
    bus->port = cfg->i2c_port;
    *out = bus;
    return ESP_OK;
}

static inline esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,
                                                    const i2c_device_config_t *dev_cfg,
                                                    i2c_master_dev_handle_t *out)
{
    i2c_master_dev_t *dev = (i2c_master_dev_t *)malloc(sizeof(i2c_master_dev_t));
    if (!dev) return ESP_ERR_NO_MEM;
    dev->port     = bus->port;
    dev->addr     = dev_cfg->device_address;
    dev->speed_hz = dev_cfg->scl_speed_hz;
    *out = dev;
    return ESP_OK;
}

static inline esp_err_t i2c_master_transmit(i2c_master_dev_handle_t dev,
                                              const uint8_t *write_buf,
                                              size_t write_size,
                                              int xfer_timeout_ms)
{
    TickType_t ticks = (xfer_timeout_ms < 0)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS((uint32_t)xfer_timeout_ms);
    return i2c_master_write_to_device(dev->port, (uint8_t)dev->addr,
                                       write_buf, write_size, ticks);
}

static inline esp_err_t i2c_master_receive(i2c_master_dev_handle_t dev,
                                             uint8_t *read_buf,
                                             size_t read_size,
                                             int xfer_timeout_ms)
{
    TickType_t ticks = (xfer_timeout_ms < 0)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS((uint32_t)xfer_timeout_ms);
    return i2c_master_read_from_device(dev->port, (uint8_t)dev->addr,
                                        read_buf, read_size, ticks);
}

static inline esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev,
                                                      const uint8_t *write_buf,
                                                      size_t write_size,
                                                      uint8_t *read_buf,
                                                      size_t read_size,
                                                      int xfer_timeout_ms)
{
    TickType_t ticks = (xfer_timeout_ms < 0)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS((uint32_t)xfer_timeout_ms);
    return i2c_master_write_read_device(dev->port, (uint8_t)dev->addr,
                                         write_buf, write_size,
                                         read_buf, read_size, ticks);
}

static inline esp_err_t i2c_master_bus_reset(i2c_master_bus_handle_t bus)
{
    (void)bus;
    return ESP_OK;
}

static inline esp_err_t i2c_master_bus_wait_all_done(i2c_master_bus_handle_t bus,
                                                       int timeout_ms)
{
    (void)bus; (void)timeout_ms;
    return ESP_OK;
}

static inline esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus)
{
    if (bus) {
        i2c_driver_delete(bus->port);
        free(bus);
    }
    return ESP_OK;
}

static inline esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t dev)
{
    if (dev) free(dev);
    return ESP_OK;
}

static inline esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus,
                                          uint16_t addr,
                                          int xfer_timeout_ms)
{
    (void)xfer_timeout_ms;
    uint8_t dummy;
    return i2c_master_read_from_device(bus->port, (uint8_t)addr, &dummy, 0,
                                        pdMS_TO_TICKS(50));
}

#ifdef __cplusplus
}
#endif
