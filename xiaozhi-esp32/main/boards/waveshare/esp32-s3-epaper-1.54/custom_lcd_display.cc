#include <stdio.h>
#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <vector>
#include <string>
#include <esp_log.h>
#include "custom_lcd_display.h"
#include "board.h"
#include "config.h"
#include "esp_lvgl_port.h"
#include "settings.h"

#define TAG "CustomLcdDisplay"

#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define BUFF_SIZE (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT * BYTES_PER_PIXEL)

const uint8_t WF_Full_1IN54[159] =
{											
    0x80,0x48,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x40,0x48,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x80,0x48,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x40,0x48,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0xA,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x8,0x1,0x0,0x8,0x1,0x0,0x2,				
    0xA,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x22,0x22,0x22,0x22,0x22,0x22,0x0,0x0,0x0,			
    0x22,0x17,0x41,0x0,0x32,0x20
};

const uint8_t WF_PARTIAL_1IN54_0[159] =
{
    0x0,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x40,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0xF,0x0,0x0,0x0,0x0,0x0,0x0,
    0x1,0x1,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x22,0x22,0x22,0x22,0x22,0x22,0x0,0x0,0x0,
    0x02,0x17,0x41,0xB0,0x32,0x28,
};

void CustomLcdDisplay::lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p) {
    assert(disp != NULL);
    CustomLcdDisplay *driver = (CustomLcdDisplay *) lv_display_get_user_data(disp);
    uint16_t         *buffer = (uint16_t *) color_p;
    driver->EPD_Clear();
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint8_t color = (*buffer < 0x7fff) ? DRIVER_COLOR_BLACK : DRIVER_COLOR_WHITE;
            driver->EPD_DrawColorPixel(x, y, color);
            buffer++;
        }
    }
    driver->EPD_DisplayPart();
    lv_disp_flush_ready(disp);
}

CustomLcdDisplay::CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, 
    int width, int height, int offset_x, int offset_y, 
    bool mirror_x, bool mirror_y, bool swap_xy, custom_lcd_spi_t _lcd_spi_data) : 
    LcdDisplay(panel_io, panel, width, height), 
    lcd_spi_data(_lcd_spi_data), 
    Width(width), Height(height) {

    ESP_LOGI(TAG, "Initialize SPI");
    spi_port_init();
    spi_gpio_init();

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority   = 2;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);
    lvgl_port_lock(0);

    buffer = (uint8_t *) heap_caps_malloc(lcd_spi_data.buffer_len, MALLOC_CAP_SPIRAM);
    assert(buffer);
    display_ = lv_display_create(width, height); /* 以水平和垂直分辨率（像素）进行基本初始化 */
    lv_display_set_flush_cb(display_, lvgl_flush_cb);
    lv_display_set_user_data(display_, this);

    uint8_t *buffer_1 = NULL;
    buffer_1          = (uint8_t *) heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
    assert(buffer_1);
    lv_display_set_buffers(display_, buffer_1, NULL, BUFF_SIZE, LV_DISPLAY_RENDER_MODE_FULL);

    ESP_LOGI(TAG, "EPD init");
    EPD_Init();
    EPD_Clear();
    EPD_Display();
    EPD_DisplayPartBaseImage();
    EPD_Init_Partial(); // 局部刷新初始化

    lvgl_port_unlock();
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    // Note: SetupUI() should be called by Application::Initialize(), not in constructor
    // to ensure lvgl objects are created after the display is fully initialized.
}

CustomLcdDisplay::~CustomLcdDisplay() {
    
}

void CustomLcdDisplay::spi_gpio_init() {
    int rst  = lcd_spi_data.rst;
    int cs   = lcd_spi_data.cs;
    int dc   = lcd_spi_data.dc;
    int busy = lcd_spi_data.busy;

    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type     = GPIO_INTR_DISABLE;
    gpio_conf.mode          = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask  = (0x1ULL << rst) | (0x1ULL << dc) | (0x1ULL << cs);
    gpio_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    gpio_conf.mode         = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (0x1ULL << busy);
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    set_rst_1();
}

void CustomLcdDisplay::spi_port_init() {
    int              mosi     = lcd_spi_data.mosi;
    int              scl      = lcd_spi_data.scl;
    int              spi_host = lcd_spi_data.spi_host;
    esp_err_t        ret;
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num      = -1;
    buscfg.mosi_io_num      = mosi;
    buscfg.sclk_io_num      = scl;
    buscfg.quadwp_io_num    = -1;
    buscfg.quadhd_io_num    = -1;
    buscfg.max_transfer_sz  = Width * Height;

    spi_device_interface_config_t devcfg = {};
    devcfg.spics_io_num                  = -1;
    devcfg.clock_speed_hz                = 40 * 1000 * 1000; // Clock out at 10 MHz
    devcfg.mode                          = 0;                // SPI mode 0
    devcfg.queue_size                    = 7;                // We want to be able to queue 7 transactions at a time

    ret = spi_bus_initialize((spi_host_device_t) spi_host, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device((spi_host_device_t) spi_host, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
}

void CustomLcdDisplay::read_busy() {
    int busy = lcd_spi_data.busy;
    while (gpio_get_level((gpio_num_t) busy) == 1) {
        vTaskDelay(pdMS_TO_TICKS(5)); // LOW: idle, HIGH: busy
    }
}

void CustomLcdDisplay::SPI_SendByte(uint8_t data) {
    esp_err_t         ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8;
    t.tx_buffer = &data;
    ret         = spi_device_polling_transmit(spi, &t); // Transmit!
    assert(ret == ESP_OK);                              // Should have had no issues.
}

void CustomLcdDisplay::EPD_SendData(uint8_t data) {
    set_dc_1();
    set_cs_0();
    SPI_SendByte(data);
    set_cs_1();
}

void CustomLcdDisplay::EPD_SendCommand(uint8_t command) {
    set_dc_0();
    set_cs_0();
    SPI_SendByte(command);
    set_cs_1();
}

void CustomLcdDisplay::writeBytes(uint8_t *buffer, int len) {
    set_dc_1();
    set_cs_0();
    esp_err_t         ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8 * len;
    t.tx_buffer = buffer;
    ret         = spi_device_polling_transmit(spi, &t); // Transmit!
    assert(ret == ESP_OK);
    set_cs_1();
}

void CustomLcdDisplay::writeBytes(const uint8_t *buffer, int len) {
    set_dc_1();
    set_cs_0();
    esp_err_t         ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8 * len;
    t.tx_buffer = buffer;
    ret         = spi_device_polling_transmit(spi, &t); // Transmit!
    assert(ret == ESP_OK);
    set_cs_1();
}

void CustomLcdDisplay::EPD_SetWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend) {
    EPD_SendCommand(0x44); // SET_RAM_X_ADDRESS_START_END_POSITION
    EPD_SendData((Xstart >> 3) & 0xFF);
    EPD_SendData((Xend >> 3) & 0xFF);

    EPD_SendCommand(0x45); // SET_RAM_Y_ADDRESS_START_END_POSITION
    EPD_SendData(Ystart & 0xFF);
    EPD_SendData((Ystart >> 8) & 0xFF);
    EPD_SendData(Yend & 0xFF);
    EPD_SendData((Yend >> 8) & 0xFF);
}

void CustomLcdDisplay::EPD_SetCursor(uint16_t Xstart, uint16_t Ystart) {
    EPD_SendCommand(0x4E); // SET_RAM_X_ADDRESS_COUNTER
    EPD_SendData(Xstart & 0xFF);

    EPD_SendCommand(0x4F); // SET_RAM_Y_ADDRESS_COUNTER
    EPD_SendData(Ystart & 0xFF);
    EPD_SendData((Ystart >> 8) & 0xFF);
}

void CustomLcdDisplay::EPD_SetLut(const uint8_t *lut) {
    EPD_SendCommand(0x32);
    writeBytes(lut, 153);
    read_busy();

    EPD_SendCommand(0x3f);
    EPD_SendData(lut[153]);

    EPD_SendCommand(0x03);
    EPD_SendData(lut[154]);

    EPD_SendCommand(0x04);
    EPD_SendData(lut[155]);
    EPD_SendData(lut[156]);
    EPD_SendData(lut[157]);

    EPD_SendCommand(0x2c);
    EPD_SendData(lut[158]);
}

void CustomLcdDisplay::EPD_TurnOnDisplay() {
    EPD_SendCommand(0x22);
    EPD_SendData(0xc7);
    EPD_SendCommand(0x20);
    read_busy();
}

void CustomLcdDisplay::EPD_TurnOnDisplayPart() {
    EPD_SendCommand(0x22);
    EPD_SendData(0xcf);
    EPD_SendCommand(0x20);
    read_busy();
}

void CustomLcdDisplay::EPD_Init() {
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));
    set_rst_0();
    vTaskDelay(pdMS_TO_TICKS(20));
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));

    read_busy();
    EPD_SendCommand(0x12); // SWRESET
    read_busy();

    EPD_SendCommand(0x01); // Driver output control
    EPD_SendData(0xC7);
    EPD_SendData(0x00);
    EPD_SendData(0x01);

    EPD_SendCommand(0x11); // data entry mode
    EPD_SendData(0x01);

    EPD_SetWindows(0, Width - 1, Height - 1, 0);

    EPD_SendCommand(0x3C); // BorderWavefrom
    EPD_SendData(0x01);

    EPD_SendCommand(0x18);
    EPD_SendData(0x80);

    EPD_SendCommand(0x22); // Load Temperature and waveform setting.
    EPD_SendData(0XB1);
    EPD_SendCommand(0x20);

    EPD_SetCursor(0, Height - 1);
    read_busy();

    EPD_SetLut(WF_Full_1IN54);
}

void CustomLcdDisplay::EPD_Clear() {
    int buffer_len = lcd_spi_data.buffer_len;
    memset(buffer, 0xff, buffer_len);
}

void CustomLcdDisplay::EPD_Display() {
    int buffer_len = lcd_spi_data.buffer_len;
    EPD_SendCommand(0x24);
    assert(buffer);
    writeBytes(buffer, buffer_len);
    EPD_TurnOnDisplay();
}

void CustomLcdDisplay::EPD_DisplayPartBaseImage() {
    int buffer_len = lcd_spi_data.buffer_len;
    EPD_SendCommand(0x24);
    assert(buffer);
    writeBytes(buffer, buffer_len);
    EPD_SendCommand(0x26);
    writeBytes(buffer, buffer_len);
    EPD_TurnOnDisplay();
}

void CustomLcdDisplay::EPD_Init_Partial() {
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));
    set_rst_0();
    vTaskDelay(pdMS_TO_TICKS(20));
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));

    read_busy();

    EPD_SetLut(WF_PARTIAL_1IN54_0);

    EPD_SendCommand(0x37);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x40);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);

    EPD_SendCommand(0x3C); // BorderWavefrom
    EPD_SendData(0x80);

    EPD_SendCommand(0x22);
    EPD_SendData(0xc0);
    EPD_SendCommand(0x20);
    read_busy();
}

void CustomLcdDisplay::EPD_DisplayPart() {
    EPD_SendCommand(0x24);
    assert(buffer);
    writeBytes(buffer, 5000);
    EPD_TurnOnDisplayPart();
}

void CustomLcdDisplay::EPD_DrawColorPixel(uint16_t x, uint16_t y, uint8_t color) {
    if (x >= Width || y >= Height) {
        ESP_LOGE("EPD", "Out of bounds pixel: (%d,%d)", x, y);
        return;
    }

    uint16_t index = y * 25 + (x >> 3);
    uint8_t  bit   = 7 - (x & 0x07);
    if (color == DRIVER_COLOR_WHITE) {
        buffer[index] |= (0x01 << bit);
    } else {
        buffer[index] &= ~(0x01 << bit);
    }
}

/* ============================================================
 *  大表情脸（Big Face）
 *  用 LVGL 基础图形绘制占据屏幕上半区的大脸，黑白分明适配墨水屏。
 *  覆盖基类 LcdDisplay::SetEmotion 的小图标方案。
 * ============================================================ */

// 脸部布局常量（屏幕200x200，脸区180x120，居中偏上）
static constexpr int FACE_W = 180;
static constexpr int FACE_H = 120;
static constexpr int EYE_CX_L = 48;    // 左眼中心x
static constexpr int EYE_CX_R = 132;   // 右眼中心x
static constexpr int EYE_CY  = 42;     // 眼睛中心y
static constexpr int MOUTH_CX = 90;    // 嘴中心x

void CustomLcdDisplay::CreateFace() {
    if (face_ != nullptr) {
        return;
    }
    auto screen = lv_screen_active();

    face_ = lv_obj_create(screen);
    lv_obj_remove_style_all(face_);
    lv_obj_set_size(face_, FACE_W, FACE_H);
    lv_obj_align(face_, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_remove_flag(face_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    auto make_solid = [this](lv_obj_t*& obj, bool circle) {
        obj = lv_obj_create(face_);
        lv_obj_remove_style_all(obj);
        lv_obj_set_style_bg_color(obj, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(obj, circle ? LV_RADIUS_CIRCLE : 4, 0);
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    };
    make_solid(left_eye_, true);
    make_solid(right_eye_, true);
    make_solid(left_brow_, false);
    make_solid(right_brow_, false);
    make_solid(tear_, true);
    make_solid(mouth_bar_, false);

    // 弧线嘴：隐藏背景弧和旋钮，只留指示弧
    mouth_arc_ = lv_arc_create(face_);
    lv_obj_remove_style(mouth_arc_, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_opa(mouth_arc_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(mouth_arc_, lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(mouth_arc_, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(mouth_arc_, true, LV_PART_INDICATOR);
    lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(mouth_arc_, 0);
    lv_arc_set_bg_angles(mouth_arc_, 0, 360);

    // 睡觉的 zZ
    zzz_label_ = lv_label_create(face_);
    lv_label_set_text(zzz_label_, "z Z");
    lv_obj_set_style_text_color(zzz_label_, lv_color_black(), 0);
    lv_obj_set_pos(zzz_label_, FACE_W - 46, 2);
}

// 小工具：以中心点摆放一个矩形部件
static void place_center(lv_obj_t* obj, int cx, int cy, int w, int h) {
    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, cx - w / 2, cy - h / 2);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void CustomLcdDisplay::ApplyFace(const char* e) {
    // 先全部隐藏，再按表情逐个摆放
    lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(left_brow_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(right_brow_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(tear_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_bar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(zzz_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_transform_rotation(left_brow_, 0, 0);
    lv_obj_set_style_transform_rotation(right_brow_, 0, 0);

    auto eyes_open = [this](int size) {
        place_center(left_eye_, EYE_CX_L, EYE_CY, size, size);
        place_center(right_eye_, EYE_CX_R, EYE_CY, size, size);
    };
    auto eyes_closed = [this]() {
        place_center(left_eye_, EYE_CX_L, EYE_CY, 40, 9);
        place_center(right_eye_, EYE_CX_R, EYE_CY, 40, 9);
    };
    // 弧线嘴：smile=下半弧, frown=上半弧(下移), circle=整圆
    auto mouth_smile = [this](int size, int width) {
        lv_obj_set_style_arc_width(mouth_arc_, width, LV_PART_INDICATOR);
        lv_obj_set_size(mouth_arc_, size, size);
        lv_obj_set_pos(mouth_arc_, MOUTH_CX - size / 2, 96 - size / 2);
        lv_arc_set_angles(mouth_arc_, 25, 155);
        lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
    };
    auto mouth_frown = [this](int size) {
        lv_obj_set_style_arc_width(mouth_arc_, 8, LV_PART_INDICATOR);
        lv_obj_set_size(mouth_arc_, size, size);
        lv_obj_set_pos(mouth_arc_, MOUTH_CX - size / 2, 118 - size / 2);
        lv_arc_set_angles(mouth_arc_, 205, 335);
        lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
    };
    auto mouth_o = [this](int size) {
        lv_obj_set_style_arc_width(mouth_arc_, 8, LV_PART_INDICATOR);
        lv_obj_set_size(mouth_arc_, size, size);
        lv_obj_set_pos(mouth_arc_, MOUTH_CX - size / 2, 96 - size / 2);
        lv_arc_set_angles(mouth_arc_, 0, 360);
        lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
    };
    auto mouth_flat = [this](int w) {
        place_center(mouth_bar_, MOUTH_CX, 96, w, 8);
    };

    std::string em = (e != nullptr) ? e : "neutral";

    if (em == "happy" || em == "relaxed" || em == "confident" ||
        em == "loving" || em == "kissy" || em == "delicious") {
        eyes_open(36);
        mouth_smile(72, 8);
    } else if (em == "laughing" || em == "funny" || em == "silly") {
        eyes_closed();                      // 眯眼大笑
        mouth_smile(92, 10);
    } else if (em == "sad") {
        eyes_open(30);
        mouth_frown(64);
    } else if (em == "crying") {
        eyes_open(30);
        mouth_frown(64);
        place_center(tear_, EYE_CX_R + 26, EYE_CY + 26, 12, 16);   // 眼角泪滴
    } else if (em == "angry") {
        eyes_open(32);
        // 斜眉：内低外高
        place_center(left_brow_, EYE_CX_L, EYE_CY - 30, 44, 9);
        place_center(right_brow_, EYE_CX_R, EYE_CY - 30, 44, 9);
        lv_obj_set_style_transform_rotation(left_brow_, 250, 0);    // +25°
        lv_obj_set_style_transform_rotation(right_brow_, -250, 0);  // -25°
        mouth_frown(64);
    } else if (em == "surprised" || em == "shocked") {
        eyes_open(46);
        mouth_o(44);
    } else if (em == "winking") {
        place_center(left_eye_, EYE_CX_L, EYE_CY, 40, 9);           // 左眼闭
        place_center(right_eye_, EYE_CX_R, EYE_CY, 36, 36);         // 右眼睁
        mouth_smile(72, 8);
    } else if (em == "cool") {
        // “墨镜”：两条粗横杠盖住眼睛
        place_center(left_brow_, EYE_CX_L, EYE_CY, 52, 20);
        place_center(right_brow_, EYE_CX_R, EYE_CY, 52, 20);
        mouth_smile(64, 8);
    } else if (em == "sleepy") {
        eyes_closed();
        mouth_flat(36);
        lv_obj_remove_flag(zzz_label_, LV_OBJ_FLAG_HIDDEN);
    } else if (em == "thinking" || em == "confused" || em == "embarrassed") {
        place_center(left_eye_, EYE_CX_L, EYE_CY, 32, 32);
        place_center(right_eye_, EYE_CX_R, EYE_CY - 6, 24, 24);     // 一大一小
        place_center(right_brow_, EYE_CX_R, EYE_CY - 34, 40, 8);    // 挑眉
        mouth_flat(30);
    } else {  // neutral 及未知情绪
        eyes_open(32);
        mouth_flat(44);
    }
}

void CustomLcdDisplay::SetEmotion(const char* emotion) {
    DisplayLockGuard lock(this);
    CreateFace();
    // 隐藏基类的小图标表情
    if (emoji_label_ != nullptr) {
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (emoji_image_ != nullptr) {
        lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_remove_flag(face_, LV_OBJ_FLAG_HIDDEN);
    ApplyFace(emotion);
}
