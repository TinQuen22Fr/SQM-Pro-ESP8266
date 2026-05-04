// OTA.ino
// Over-The-Air firmware update support (ArduinoOTA).
//
// Copyright (c) 2025 Quentin Dumont
//
// When OTA_ON is defined in Config.h, the ESP8266 advertises itself on the
// local network with the hostname `ota_hostname` and accepts firmware uploads
// from Arduino IDE (Tools -> Port -> network port) protected by `ota_password`.
//
// During an OTA upload the OLED shows progress in % and the buzzer beeps once
// when the new firmware has been fully received.
//
#ifdef OTA_ON
#include <ArduinoOTA.h>

void ota_setup() {
  if (!WiFiConnected) return;

  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
#ifdef DEBUG_WIFI_ON
    Serial.println("OTA Start: " + type);
#endif
    OledDisp.clear();
    OledDisp.setCursor(0, 0);
    OledDisp.print("OTA Update");
    OledDisp.setCursor(0, 2);
    OledDisp.print("Type: ");
    OledDisp.print(type);
    OledDisp.setCursor(0, 4);
    OledDisp.print("Do NOT unplug!");
  });

  ArduinoOTA.onEnd([]() {
#ifdef DEBUG_WIFI_ON
    Serial.println("\nOTA End");
#endif
    OledDisp.setCursor(0, 6);
    OledDisp.print("Done. Reboot.");
    buzzer(100);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char buf[24];
    sprintf(buf, "%3u%%      ", (progress / (total / 100)));
    OledDisp.setCursor(0, 6);
    OledDisp.print(buf);
  });

  ArduinoOTA.onError([](ota_error_t error) {
#ifdef DEBUG_WIFI_ON
    Serial.printf("OTA Error[%u]\n", error);
#endif
    const char *msg = "OTA Err";
    if      (error == OTA_AUTH_ERROR)    msg = "OTA AuthErr";
    else if (error == OTA_BEGIN_ERROR)   msg = "OTA BegErr";
    else if (error == OTA_CONNECT_ERROR) msg = "OTA ConErr";
    else if (error == OTA_RECEIVE_ERROR) msg = "OTA RecErr";
    else if (error == OTA_END_ERROR)     msg = "OTA EndErr";
    OledDisp.setCursor(0, 6);
    OledDisp.print(msg);
    buzzer(500);
  });

  ArduinoOTA.begin();
#ifdef DEBUG_WIFI_ON
  Serial.print("OTA ready, hostname: ");
  Serial.println(ota_hostname);
#endif
}

void ota_loop() {
  ArduinoOTA.handle();
}

#endif // OTA_ON
