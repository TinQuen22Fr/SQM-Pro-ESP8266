// WiFi.ino
// WiFi + cloud upload to SQM Nightwatch (magnitude-tracker).
//
// Copyright (c) 2025-2026 Quentin Dumont
//
// Depuis la v2.3.0 :
//   - Les credentials WiFi ne sont PLUS dans secrets.h. Ils sont saisis par
//     l'utilisateur via le portail captif `SQM-Setup-XXXXXX` au premier boot
//     (cf. WiFiPortal.ino). Ils sont persistés en flash par ESP8266WiFi et
//     restaurés automatiquement via WiFi.begin() sans arguments.
//   - L'identifiant de la sonde (`ID=...` envoyé au backend) provient
//     également du portail captif et est stocké en EEPROM (`gStationName`).
//     Cela permet un projet coopératif où chaque utilisateur identifie sa
//     propre station sans recompiler le firmware.
//
// La SENSOR_KEY (clé API du backend) reste hardcodée car partagée par tous
// les utilisateurs du projet coopératif (= "passe-partout vers le serveur de
// Quentin"). Cette valeur est injectée dans secrets.h au moment de la
// compilation CI depuis le GitHub Secret `SENSOR_KEY`.
//
#ifdef WIFI_ON

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// Variables exposées par WiFiPortal.ino
extern char gStationName[33];
extern bool gWifiPortalOk;

void wifi_setup() {
  // Avant la v2.3.0, cette fonction faisait WiFi.begin(ssid, password).
  // Désormais c'est wifiPortal_setup() (appelé une seule fois au démarrage)
  // qui établit la connexion via WiFiManager. Cette fonction-ci se contente
  // de surveiller l'état du WiFi et de tenter une reconnexion si on a perdu
  // la connexion (sans pour autant relancer le portail captif).
  if (WiFi.status() == WL_CONNECTED) {
    WiFiConnected = true;
    return;
  }

#ifdef DEBUG_WIFI_ON
  Serial.print(F("[WiFi] disconnected, attempting reconnect..."));
#endif

  // ESP8266WiFi stocke les derniers credentials en flash → un simple
  // WiFi.begin() sans args suffit pour retenter la même connexion.
  WiFi.begin();
  for (uint8_t t = 30; t > 0; t--) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(500);
#ifdef DEBUG_WIFI_ON
    Serial.print(".");
#endif
  }

  if (WiFi.status() != WL_CONNECTED) {
#ifdef DEBUG_WIFI_ON
    Serial.println(F("\n[WiFi] reconnect failed"));
#endif
    WiFiConnected = false;
  } else {
    WiFiConnected = true;
#ifdef DEBUG_WIFI_ON
    Serial.print(F("\n[WiFi] reconnected, IP: "));
    Serial.println(WiFi.localIP());
#endif
  }
}

// Called periodically from loop() to push a fresh measurement to the backend.
// The signature is unchanged; SQM_pro.ino keeps calling wifi_main(...) as before.
void wifi_main(double mpsas, double dmpsas, int temp, byte hum, int pres) {
  if (!WiFiConnected) return;

#ifdef NIGHT_ONLY_PUSH_ON
  // Skip push during daytime: the TSL2591 is saturated and the magnitude is
  // meaningless. The OLED keeps showing the live reading.
  if (mpsas < NIGHT_THRESHOLD_MPSAS) {
#ifdef DEBUG_WIFI_ON
    Serial.print("Daytime detected (mpsas=");
    Serial.print(mpsas);
    Serial.print(" < ");
    Serial.print(NIGHT_THRESHOLD_MPSAS);
    Serial.println("): skipping push.");
#endif
    return;
  }
#endif

  String url;
  // Calibrated battery readout (helper defined in MyLib.ino)
  float battery = readBatteryVoltage();
  byte  battPct = getBatteryPercentSmoothed(battery);

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
  Serial.print("Battery: "); Serial.print(battery, 3); Serial.print(" V ("); Serial.print(battPct); Serial.println(" %)");
  Serial.print("Station ID: "); Serial.println(gStationName);
#endif

  url  = "?ID=";  url += gStationName;  // <-- nom dynamique depuis EEPROM
  url += "&KEY="; url += sensor_key;
  url += "&T=";   url += temp;
  url += "&H=";   url += hum;
  url += "&P=";   url += pres / 100;
  url += "&S=";   url += mpsas;
  url += "&D=";   url += dmpsas;
  url += "&V=";   url += String(battery, 3);
  url += "&Vpct=";url += battPct;
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
