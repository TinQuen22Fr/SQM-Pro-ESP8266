// WiFi.ino
// WiFi + cloud upload to SQM Nightwatch (magnitude-tracker).
//
// Copyright (c) 2025 Quentin Dumont
//
// The backend endpoint is HTTPS-only (port 80 redirects 301 -> 443),
// so we use WiFiClientSecure. The ingestion route accepts an HTTP GET
// with query-string parameters; the key names (ID, KEY, T, H, P, S, D,
// V, L, Alt, Lat, Lon) match what this firmware has always emitted, so
// only the transport changed (HTTP -> HTTPS).
//
#ifdef WIFI_ON

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

void wifi_setup() {
  if (!WiFiConnected) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
#ifdef DEBUG_WIFI_ON
    Serial.printf("Wait for WiFi.");
#endif
    for (uint8_t t = 30; t > 0; t--) {
      if (WiFi.status() == WL_CONNECTED) break;
      delay(500);
#ifdef DEBUG_WIFI_ON
      Serial.print(".");
#endif
    }
#ifdef ALT_SSID_ON
#ifdef DEBUG_WIFI_ON
    Serial.printf("\nALT_SSID");
#endif
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid2, password2);
      for (uint8_t t = 30; t > 0; t--) {
        if (WiFi.status() == WL_CONNECTED) break;
        delay(500);
#ifdef DEBUG_WIFI_ON
        Serial.print(":");
#endif
      }
    }
#endif
  }
  if (WiFi.status() != WL_CONNECTED) {
#ifdef DEBUG_WIFI_ON
    Serial.printf("\nWIFI not connect.");
#endif
    WiFiConnected = false;
  } else {
    WiFiConnected = true;
#ifdef DEBUG_WIFI_ON
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
#endif
  }
}

// Called periodically from loop() to push a fresh measurement to the backend.
// The signature is unchanged; SQM_pro.ino keeps calling wifi_main(...) as before.
void wifi_main(double mpsas, double dmpsas, int temp, byte hum, int pres) {
  if (!WiFiConnected) return;

  String url;
  float battery = int(analogRead(A0) / 1023.0 * 11 * 100 + 0.5) / 100.;

  // Compute lux from the TSL2591 raw channels (optional extra field "L").
  float lux = 0.0f;
  if (sqm.full > 0 || sqm.ir > 0) {
    lux = sqm.calculateLux(sqm.full, sqm.ir);
    if (lux < 0) lux = 0;
  }

#ifdef DEBUG_WIFI_ON
  Serial.println("");
  Serial.print("SQM: ");     Serial.print(mpsas);  Serial.print("+-"); Serial.print(dmpsas); Serial.println(" mas^2");
  Serial.print("Temp: ");    Serial.print(temp);   Serial.println(" C");
  Serial.print("Humidity: ");Serial.print(hum);    Serial.println(" %");
  Serial.print("Pressure: ");Serial.print(pres/100); Serial.println(" hPa");
  Serial.print("Lux: ");     Serial.println(lux, 4);
  Serial.print("Battery: "); Serial.println(battery);
#endif

  url  = "?ID=";  url += SensorID;
  url += "&KEY="; url += sensor_key;
  url += "&T=";   url += temp;
  url += "&H=";   url += hum;
  url += "&P=";   url += pres / 100;
  url += "&S=";   url += mpsas;
  url += "&D=";   url += dmpsas;
  url += "&V=";   url += battery;
  url += "&L=";   url += String(lux, 4);
#ifdef GPS_ON
  if (g_sat > 2) {
    url += "&Alt="; url += g_alt;
    url += "&Lat="; url += String(g_lat, 6);
    url += "&Lon="; url += String(g_lng, 6);
  }
#endif
  send_cloud(url);
}

// HTTPS GET to sqm.quentin-astro.fr/api/sqm_push?...
// ESP8266 does not have enough RAM to fully validate a CA chain with a
// dynamic certificate (Let's Encrypt), so we use setInsecure(): TLS is
// still used to encrypt the channel, but without certificate pinning.
void send_cloud(String url) {
  WiFiClientSecure wifi_client;
  wifi_client.setInsecure();     // encrypted but no cert validation
  wifi_client.setTimeout(5000);

  const int httpPort = HTTP_PORT; // 443

  if (!wifi_client.connect(host, httpPort)) {
#ifdef DEBUG_WIFI_ON
    Serial.println("connection failed");
#endif
    WiFiConnected = false;
    return;
  }

  wifi_client.print(String("GET ") + app + url + " HTTP/1.1\r\n"
                  + "Host: " + host + "\r\n"
                  + "User-Agent: SQMPro-ESP8266/" + Version + "\r\n"
                  + "Connection: close\r\n\r\n");

  unsigned long timeout = millis();
  while (wifi_client.available() == 0) {
    if (millis() - timeout > 5000) {
#ifdef DEBUG_WIFI_ON
      Serial.println("HTTPS Timeout !");
#endif
      wifi_client.stop();
      return;
    }
  }

#ifdef DEBUG_WIFI_ON
  while (wifi_client.available()) {
    String line = wifi_client.readStringUntil('\r');
    Serial.print(line);
  }
  Serial.println();
  Serial.println("closing connection");
#else
  // Drain the response even without debug, to avoid leaving bytes pending
  while (wifi_client.available()) wifi_client.read();
#endif
}

#endif // WIFI_ON
