#include "touch.h"
#include "esp_log.h"
#include "lvgl.h"

#define I2C_MASTER_SCL_IO 0 /*!< GPIO per SCL */
#define I2C_MASTER_SDA_IO 1 /*!< GPIO per SDA */
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

#define TSC2007_ADDR 0x48

typedef struct
{
    uint16_t x_min;
    uint16_t x_max;
    uint16_t y_min;
    uint16_t y_max;
} touch_calib_t;

static touch_calib_t touch_calib;

esp_err_t tsc2007_read(uint8_t command, uint16_t *value)
{
    uint8_t buf[2];
    esp_err_t ret;

    // invia il command byte
    ret = i2c_master_write_to_device(
        I2C_MASTER_NUM,
        TSC2007_ADDR,
        &command, 1,
        pdMS_TO_TICKS(100));
    if (ret != ESP_OK)
        return ret;

    // attesa per conversione ADC
    ets_delay_us(200); // ~200 µs per ADC 12 bit

    // legge 2 byte (MSB first)
    ret = i2c_master_read_from_device(
        I2C_MASTER_NUM,
        TSC2007_ADDR,
        buf, 2,
        pdMS_TO_TICKS(100));
    if (ret != ESP_OK)
        return ret;

    // converte in 12 bit
    *value = ((buf[0] << 8) | buf[1]) >> 4;
    ESP_LOGI("TOUCH", "Raw value %d", *value);
    return ESP_OK;
}

#define LCD_WIDTH 240
#define LCD_HEIGHT 320
#define TOUCH_SAMPLES 5

static lv_point_t calib_points[3] = {
    {20, 20},                       // angolo superiore sinistro
    {LCD_WIDTH - 20, 20},           // angolo superiore destro
    {LCD_WIDTH / 2, LCD_HEIGHT / 2} // centro
};

void touch_calibrate_interactive(void)
{
    lv_obj_t *label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(label, "Tocca il punto rosso");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, -40);

    uint16_t x_raw[3], y_raw[3];

    for (int i = 0; i < 3; i++)
    {
        // disegna il punto rosso
        lv_obj_t *point = lv_obj_create(lv_scr_act(), NULL);
        lv_obj_set_size(point, 10, 10);
        lv_obj_set_style_local_bg_color(point, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_RED);
        lv_obj_align(point, NULL, LV_ALIGN_IN_TOP_LEFT, calib_points[i].x, calib_points[i].y);

        bool touched = false;
        while (!touched)
        {
            lv_task_handler(); // aggiorna LVGL
            vTaskDelay(pdMS_TO_TICKS(10));

            int stable_count = 0;
            uint32_t x_sum = 0, y_sum = 0;
            for (int s = 0; s < TOUCH_SAMPLES; s++)
            {
                uint16_t x, y;
                if (tsc2007_read(0x90, &x) == ESP_OK &&
                    tsc2007_read(0xD0, &y) == ESP_OK)
                {
                    // considera solo valori plausibili
                    if (x > 200 && x < 2700 && y > 200 && y < 2700)
                    {
                        x_sum += x;
                        y_sum += y;
                        stable_count++;
                    }
                }
                ets_delay_us(200); // breve delay tra letture
            }

            if (stable_count == TOUCH_SAMPLES)
            {
                x_raw[i] = x_sum / TOUCH_SAMPLES;
                y_raw[i] = y_sum / TOUCH_SAMPLES;
                touched = true;
            }
        }

        lv_obj_del(point); // rimuove il punto
    }

    lv_obj_del(label); // rimuove il label

    // calcola min/max dai punti estremi
    touch_calib.x_min = x_raw[0];
    touch_calib.x_max = x_raw[1];
    touch_calib.y_min = y_raw[0];
    touch_calib.y_max = y_raw[2];

    ESP_LOGI("TOUCH", "Calibrazione completata: X[%u,%u] Y[%u,%u]",
             touch_calib.x_min, touch_calib.x_max,
             touch_calib.y_min, touch_calib.y_max);
}

// Funzione touch_read() LVGL 7
bool touch_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint16_t x, y;

    if (tsc2007_read(0x90, &x) == ESP_OK &&
        tsc2007_read(0xD0, &y) == ESP_OK)
    {
        // soglia minima plausibile
        if (x > 200 && x < 3800 && y > 200 && y < 3800)
        {
            data->point.x = (x - touch_calib.x_min) * LCD_WIDTH /
                            (touch_calib.x_max - touch_calib.x_min);
            data->point.y = (y - touch_calib.y_min) * LCD_HEIGHT /
                            (touch_calib.y_max - touch_calib.y_min);
            data->state = LV_INDEV_STATE_PR;
            return false;
        }
    }

    data->state = LV_INDEV_STATE_REL;
    return false;
}

void touch_init()
{
    ESP_LOGI("SCAN", "TOUCH DRIVER TEST");

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TSC2007_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    ESP_LOGI("SCAN", "TSC2007 check: %s", esp_err_to_name(ret));

    ESP_LOGI(__FILE__, "TOUCH INIT - CALIBRATION");
    touch_calibrate_interactive();
    ESP_LOGI(__FILE__, "TOUCH INIT - CALIBRATION DONE");

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(__FILE__, "TOUCH DRIVER INIT DONE");
}
