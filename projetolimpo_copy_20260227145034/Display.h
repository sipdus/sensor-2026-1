#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// ===== Variáveis globais usadas para BLE =====
extern int latest_bpm;
extern float latest_spo2;
extern float latest_glucose;

// Display / touch pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 400
#define FIX_COLOR(c) lv_color_hex(__builtin_bswap32(c) >> 8)

static SPIClass touchscreenSPI = SPIClass(VSPI);
static XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
static TFT_eSPI tft = TFT_eSPI();

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * 10];

LV_IMG_DECLARE(sipdus_logo);  // se tiver gerado com o conversor LVGL

// Objetos LVGL
static lv_obj_t* scr_splash;
static lv_obj_t* scr_main;
static lv_obj_t* spinner;
static lv_obj_t* label_loading;
static lv_obj_t* img_logo_obj;

static lv_obj_t* label_bpm;
static lv_obj_t* label_spo2;
static lv_obj_t* label_glucose;
static lv_obj_t* label_avg;
static lv_obj_t* chart;
static lv_chart_series_t* ser_glucose;

// Splash timing
static unsigned long splash_start_ms = 0;
static const unsigned long SPLASH_MS = 10000; // 10 s

// Histórico glicose (ring buffer)
static const int GLU_COUNT = 20;
static float glucose_history[GLU_COUNT];
static int glucose_idx = 0;
static int glucose_size = 0; // quantas válidas

// ===== Forward declarations =====
void lv_create_splash();
void lv_create_main();
void switch_to_main();
void add_glucose_measurement(float g);
float glucose_average();
void update_sensor_values(int bpm, float spo2);
int get_glucose_count();

// ---------- Display flush ----------
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1,
                    area->x2 - area->x1 + 1,
                    area->y2 - area->y1 + 1);
  uint32_t size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
  tft.pushColors((uint16_t*)color_p, size, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

// ---------- Touchscreen ----------
void touchscreen_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    int x = map(p.x, 200, 3700, 0, SCREEN_WIDTH);
    int y = map(p.y, 3800, 240, 0, SCREEN_HEIGHT);
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ---------- Setup LVGL / TFT / Touch ----------
void lvgl_setup() {
  Serial.println("Iniciando LVGL...");
  tft.begin();
  tft.setRotation(0);
  lv_init();

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);

  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCREEN_WIDTH * 10);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_WIDTH;
  disp_drv.ver_res = SCREEN_HEIGHT;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchscreen_read;
  lv_indev_drv_register(&indev_drv);

  // Cria telas
  lv_create_splash();
  lv_create_main();

  lv_scr_load(scr_splash);
  splash_start_ms = millis();

  for (int i = 0; i < GLU_COUNT; i++) glucose_history[i] = 0.0;
  glucose_idx = 0;
  glucose_size = 0;
}

// ---------- Loop LVGL ----------
void display_loop() {
  static bool splash_done = false;

  if (lv_scr_act() == scr_splash && !splash_done) {
    if (millis() - splash_start_ms >= SPLASH_MS) {
      switch_to_main();
      splash_done = true;
    }
  }

  lv_timer_handler();
  delay(5); // pequeno delay para reduzir flicker
}

// ---------- Criação de telas ----------
void lv_create_splash() {
  scr_splash = lv_obj_create(NULL);
  lv_obj_set_size(scr_splash, SCREEN_WIDTH, SCREEN_HEIGHT);

  lv_obj_set_style_bg_color(scr_splash, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr_splash, LV_OPA_COVER, LV_PART_MAIN);

  spinner = lv_spinner_create(scr_splash, 2000, 60);
  lv_obj_set_size(spinner, 64, 64);
  lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -100);
  lv_obj_set_style_arc_color(spinner, lv_color_hex(0xE3C82B), LV_PART_MAIN);
  lv_obj_set_style_arc_color(spinner, lv_color_hex(0xBFA624), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(spinner, 6, 0);

  label_loading = lv_label_create(scr_splash);
  lv_label_set_text(label_loading, "CALIBRANDO...");
  lv_obj_set_style_text_font(label_loading, &lv_font_montserrat_28, 0);
  lv_obj_align(label_loading, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_text_color(label_loading, lv_color_hex(0xE3C82B), 0);
}

void lv_create_main() {
  scr_main = lv_obj_create(NULL);
  lv_obj_set_size(scr_main, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_color(scr_main, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr_main, LV_OPA_COVER, LV_PART_MAIN);

  label_bpm = lv_label_create(scr_main);
  lv_label_set_text(label_bpm, "BPM: --");
  lv_obj_set_style_text_font(label_bpm, &lv_font_montserrat_28, 0);
  lv_obj_align(label_bpm, LV_ALIGN_LEFT_MID, 90, -110);
  lv_obj_set_style_text_color(label_bpm, lv_color_hex(0xE3C82B), 0);

  label_spo2 = lv_label_create(scr_main);
  lv_label_set_text(label_spo2, "SpO2: --");
  lv_obj_set_style_text_font(label_spo2, &lv_font_montserrat_28, 0);
  lv_obj_align(label_spo2, LV_ALIGN_LEFT_MID, 90, -70);
  lv_obj_set_style_text_color(label_spo2, lv_color_hex(0xE3C82B), 0);

  label_glucose = lv_label_create(scr_main);
  lv_label_set_text(label_glucose, "GLICOSE: --");
  lv_obj_set_style_text_font(label_glucose, &lv_font_montserrat_28, 0);
  lv_obj_align(label_glucose, LV_ALIGN_LEFT_MID, 90, -30);
  lv_obj_set_style_text_color(label_glucose, lv_color_hex(0xE3C82B), 0);

  chart = lv_chart_create(scr_main);
  lv_obj_set_size(chart, 260, 170);
  lv_obj_align(chart, LV_ALIGN_BOTTOM_LEFT, 30, -200);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, GLU_COUNT);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 40, 300);
  ser_glucose = lv_chart_add_series(chart, lv_color_hex(0xE3C82B), LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(chart, LV_OBJ_FLAG_HIDDEN);

  label_avg = lv_label_create(scr_main);
  lv_label_set_text(label_avg, "--");
  lv_obj_set_style_text_font(label_avg, &lv_font_montserrat_18, 0);
  lv_obj_align(label_avg, LV_ALIGN_RIGHT_MID, 0, 10);
  lv_obj_set_style_text_color(label_avg, lv_color_hex(0xE3C82B), 0);
  lv_obj_add_flag(label_avg, LV_OBJ_FLAG_HIDDEN);
}

// ---------- Trocar tela ----------
void switch_to_main() {
  lv_chart_set_all_value(chart, ser_glucose, 0);
  int start = (glucose_idx - glucose_size + GLU_COUNT) % GLU_COUNT;
  for (int i = 0; i < glucose_size; i++) {
    float v = glucose_history[(start + i) % GLU_COUNT];
    lv_chart_set_next_value(chart, ser_glucose, (lv_coord_t)v);
  }
  lv_scr_load(scr_main);
}

// ---------- Histórico / gráfico ----------
void add_glucose_measurement(float g) {
  glucose_history[glucose_idx] = g;
  glucose_idx = (glucose_idx + 1) % GLU_COUNT;
  if (glucose_size < GLU_COUNT) glucose_size++;

  latest_glucose = g;

  char buf[64];
  if (glucose_size >= GLU_COUNT) {
    lv_chart_set_all_value(chart, ser_glucose, LV_CHART_POINT_NONE);
    int start = (glucose_idx - glucose_size + GLU_COUNT) % GLU_COUNT;
    for (int i = 0; i < glucose_size; i++) {
      float v = glucose_history[(start + i) % GLU_COUNT];
      lv_chart_set_next_value(chart, ser_glucose, (lv_coord_t)v);
    }

    lv_obj_clear_flag(chart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_avg, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(label_bpm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(label_spo2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(label_glucose, LV_OBJ_FLAG_HIDDEN);

    float avg = glucose_average();
    String status;
    if (avg < 70) status = "baixa";
    else if (avg > 140) status = "alta";
    else status = "normal";

    snprintf(buf, sizeof(buf), "Sua glicose media %.0f esta %s", avg, status.c_str());
    lv_label_set_text(label_avg, buf);
  } else {
    lv_obj_clear_flag(label_bpm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_spo2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_glucose, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(chart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(label_avg, LV_OBJ_FLAG_HIDDEN);

    snprintf(buf, sizeof(buf), "GL: %.0f", g);
    lv_label_set_text(label_glucose, buf);
  }
}

float glucose_average() {
  if (glucose_size == 0) return 0.0;
  float s = 0;
  for (int i = 0; i < glucose_size; i++) s += glucose_history[i];
  return s / glucose_size;
}

// ---------- Atualização dos valores ----------
void update_sensor_values(int bpm, float spo2) {
  char buf[64];
  snprintf(buf, sizeof(buf), "BPM: %d", bpm);
  lv_label_set_text(label_bpm, buf);
  snprintf(buf, sizeof(buf), "SpO2: %.0f%%", spo2);
  lv_label_set_text(label_spo2, buf);

  latest_bpm = bpm;
  latest_spo2 = spo2;
}

// ---------- Número de leituras ----------
int get_glucose_count() {
  return glucose_size;
}

#endif // DISPLAY_H
