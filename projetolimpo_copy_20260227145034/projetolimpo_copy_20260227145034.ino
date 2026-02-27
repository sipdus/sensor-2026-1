#include <lvgl.h>

#include <Ticker.h>
#include "Display.h"
#include "Sensor.h"
#include "BLEHandler.h"
extern "C" void lv_tick_inc(uint32_t ms);

//Ticker lvgl_tick;
//void lv_tick_handler() {
 // lv_tick_inc(1);  // agora deve compilar
//}bool finger_detected = false;
int latest_bpm = 0;
float latest_spo2 = 0.0;
float latest_glucose = 0.0;



void setup() {
  Serial.begin(115200);


  lvgl_setup();
  sensor_setup();
    splash_start_ms = millis(); // marca início do splash


Serial.println("LVGL version:");
Serial.print(LVGL_VERSION_MAJOR);
Serial.print(".");
Serial.print(LVGL_VERSION_MINOR);
Serial.print(".");
Serial.println(LVGL_VERSION_PATCH);
  // Aumenta o tamanho máximo do pacote BLE
// Solicita aumento de MTU (para permitir payloads maiores que 20 bytes)



Serial.println("📡 BLE configurado com MTU = 200");


  BLE_setup();

 // lvgl_tick.attach_ms(1, lv_tick_handler);
}

void loop() {
  lv_timer_handler();
  display_loop();
  sensor_loop();

    BLE_send(latest_bpm, latest_spo2, latest_glucose, true);


}
