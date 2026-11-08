#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_rom_sys.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "LCD";

#define SDA_PIN GPIO_NUM_13
#define SCL_PIN GPIO_NUM_14

// Bitovi na PCF8574 pločici
#define LCD_RS 0x01   
#define LCD_EN 0x04   
#define LCD_BL 0x08   

static i2c_master_dev_handle_t lcd;

static void i2c_send(uint8_t data)
{
    i2c_master_transmit(lcd, &data, 1, 100);
}

static void lcd_write_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = (nibble & 0xF0) | rs | LCD_BL;
    i2c_send(data | LCD_EN);
    esp_rom_delay_us(1);
    i2c_send(data);
    esp_rom_delay_us(50);
}

static void lcd_write(uint8_t value, uint8_t rs)
{
    lcd_write_nibble(value & 0xF0, rs);   // gornja 4 bita
    lcd_write_nibble(value << 4, rs);     // donja 4 bita
}

static void lcd_cmd(uint8_t cmd)
{
    lcd_write(cmd, 0);
    if (cmd == 0x01 || cmd == 0x02) {
        esp_rom_delay_us(2000);           
    }
}

static void lcd_print(const char *s)
{
    while (*s) {
        lcd_write(*s++, LCD_RS);
    }
}

static void lcd_set_cursor(uint8_t col, uint8_t row)
{
    static const uint8_t row_offset[2] = {0x00, 0x40};
    lcd_cmd(0x80 | (col + row_offset[row]));
}

static void lcd_init(void)
{
    esp_rom_delay_us(50000);
    lcd_write_nibble(0x30, 0); esp_rom_delay_us(4500);
    lcd_write_nibble(0x30, 0); esp_rom_delay_us(4500);
    lcd_write_nibble(0x30, 0); esp_rom_delay_us(150);
    lcd_write_nibble(0x20, 0);            // prebaci u 4-bitni mod

    lcd_cmd(0x28);   
    lcd_cmd(0x0C);   
    lcd_cmd(0x01);   
    lcd_cmd(0x06);     
}

static esp_err_t i2c_setup(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    uint8_t addr;
    if (i2c_master_probe(bus, 0x27, 100) == ESP_OK) {
        addr = 0x27;
    } else if (i2c_master_probe(bus, 0x3F, 100) == ESP_OK) {
        addr = 0x3F;
    } else {
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "LCD na adresi 0x%02X", addr);

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };
    return i2c_master_bus_add_device(bus, &dev_cfg, &lcd);
}

static void lcd_task(void *arg)
{
    if (i2c_setup() != ESP_OK) {
        vTaskDelete(NULL);
    }
    lcd_init();

    lcd_set_cursor(0, 0);
    lcd_print("Hello World!");

    char buf[17];
    uint32_t sec = 0;
    while (1) {
        snprintf(buf, sizeof(buf), "Vrijeme: %4lus", (unsigned long)(sec % 10000));
        sec++;
        lcd_set_cursor(0, 1);
        lcd_print(buf);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    xTaskCreate(lcd_task, "lcd_task", 4096, NULL, 5, NULL);
}
