#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "Audio.h"
#include "es8311.h"
#include "Wire.h"
#include "FT6336U.h"
#include <Preferences.h>

// Pins SD Card
#define SD_SCK 38
#define SD_CMD 40
#define SD_D0 39
#define SD_D1 41
#define SD_D2 48
#define SD_D3 47

// Pins Audio & I2S
#define I2S_MCK 4
#define I2S_BCK 5
#define I2S_DOUT 8
#define I2S_WS 7
#define AP_ENABLE 1
#define I2C_SPEED 400000

// Configuration matérielle I2C Freenove pour le tactile 2.8 pouces
#define I2C_SDA 16
#define I2C_SCL 15
#define RST_N_PIN 18
#define INT_N_PIN 17
#define BACKLIGHT_PIN 1  // Broche rétroéclairage
#define BAT_ADC_PIN 9    // Broche ADC batterie

#define FNK0104AB_2P8_240x320_ILI9341

// Règle heure France (Paris)
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3"; 

// Paramètres pour la batterie et l'horloge
unsigned long lastBatteryUpdate = 0;
unsigned long lastTimeUpdate = 0;

TFT_eSPI tft = TFT_eSPI();

// Instanciation du tactile
FT6336U ft6336u(I2C_SDA, I2C_SCL, RST_N_PIN, INT_N_PIN);
FT6336U_TouchPointType tp;

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

// Structure pour associer un nom à une URL
struct RadioStation {
  const char* name;
  const char* url;
};

// Liste de stations radio Internet
const RadioStation STATIONS_LIST[] = {
  "RTL", "https://streaming-ice.audiomeans.fr/rtl/feed-rtl-mp3-128?aw_0_1st.broadcast_type=streaming&aw_0_1st.source_type=external&aw_0_1st.platform=external&aw_0_1st.media_category=fm&aw_0_1st.station=rtl&aw_0_1st.program=rtl&listen=webCwsBCggNCQgLDQUGBAcGBg",
  "France Info", "https://icecast.radiofrance.fr/franceinfo-midfi.mp3",
  "France Inter", "http://direct.franceinter.fr/live/franceinter-midfi.mp3",
  "Europe 1", "http://europe1.lmn.fm/europe1.mp3",
  "France Bleue Isere", "http://direct.francebleu.fr/live/fbisere-midfi.mp3",
  "BBC", "https://stream.live.vc.bbcmedia.co.uk/bbc_world_service"
};
const int TOTAL_STATIONS = sizeof(STATIONS_LIST) / sizeof(STATIONS_LIST[0]);
int currentStationIndex = 0;

// Volume initial (0 à 21 max pour audioI2S)
int currentVolume = 2;
bool isMuted = false;
int savedVolume = 2;

Audio RadioAudio;
Preferences preferences;

// Structure pour stocker vos profils Wi-Fi 
struct WifiProfile {
  const char* ssid;
  const char* password;
};

const WifiProfile myNetworks[] = {
  {"SSID1", "password"},
  {"SSID2", "password"},
  {"SSID3", "password"},
  {"SSID4", "password"}
};
const int NETWORKS_COUNT = sizeof(myNetworks) / sizeof(myNetworks[0]);

void driver_es8311_init(void) {
  pinMode(AP_ENABLE, OUTPUT);
  digitalWrite(AP_ENABLE, LOW);  
  Wire.begin(I2C_SDA, I2C_SCL, I2C_SPEED);
  if (es8311_codec_init() != ESP_OK) {
    Serial.println("ES8311 Codec en échec.");
  }
}

// Fonction mise en veille
void enterSleepMode() {
  Serial.println("Entrée en mode veille...");
  preferences.begin("radio_settings", false); 
  int volumeToSave = isMuted ? savedVolume : currentVolume;
  preferences.putInt("volume", volumeToSave);
  preferences.end(); 
  
  RadioAudio.stopSong(); // Couper le son et l'amplificateur audio (AP_ENABLE = 1)
  digitalWrite(AP_ENABLE, HIGH); // Met l'ampli en veille
  digitalWrite(TFT_BL, LOW); // Éteindre le rétroéclairage de l'écran (TFT_BL = 45)
  tft.writecommand(0x10); // Mettre l'écran ILI9341 lui-même en mode veille (via SPI)
  delay(120);             
  esp_sleep_enable_ext0_wakeup((gpio_num_t)INT_N_PIN, 0);   // Configurer la broche d'interruption tactile (GPIO 17) comme source de réveil unique L'argument '0' indique qu'on souhaite se réveiller sur un état BAS (LOW) quand on touche l'écran
  esp_deep_sleep_start(); // Extinction de l'ESP32-S3
}

// Dessiner boutons du dessous
void drawButtons() {
  tft.setTextSize(1);
  tft.fillRoundRect(10, 190, 65, 40, 4, TFT_DARKCYAN);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("<", 42, 203, 2);

  tft.fillRoundRect(85, 190, 65, 40, 4, TFT_MAROON);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("-", 117, 203, 2);

  tft.fillRoundRect(160, 190, 65, 40, 4, TFT_MAROON);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("+", 192, 203, 2);

  tft.fillRoundRect(235, 190, 65, 40, 4, TFT_DARKCYAN);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(">", 267, 203, 2);
}

// Dessiner bandeau supérieur
// Affichage de la Batterie (Tout à gauche : X = 10)
void updateBatteryDisplay() {
  int adcValue = analogRead(BAT_ADC_PIN);
  float vBat = (adcValue * 3.3 / 4095.0) * 2.0; 
  int percent = (vBat - 3.2) * 100.0 / (4.2 - 3.2);
  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;
  // Efface la zone de gauche et écrit le pourcentage
  tft.fillRect(5, 5, 85, 35, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString("Bat " + String(percent) + "%", 10, 14, 2);
}
// Affichage de l'Heure (Au centre : X = 160)
void updateTimeDisplay() {
  struct tm timeinfo;
  // Si le serveur NTP n'a pas encore répondu, on ne fait rien et on garde l'affichage "--:--"
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Heure NTP non disponible...");
    return; 
  }
  char timeStr[6];
  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  // Dès que l'heure est reçue, elle s'affiche proprement au centre
  tft.fillRect(110, 3, 100, 30, TFT_DARKGREY); // Nettoyage zone affichage heure 
  tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);  // Heure en jaune
  tft.setTextSize(1); 
  tft.drawCentreString(timeStr, 160, 10, 4); 
}  
// Affichage du Volume (Tout à droite : X = 310)
void updateVolumeDisplay() {
  // Efface uniquement la zone de droite
  tft.fillRect(220, 5, 95, 35, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  if (isMuted) {
    tft.drawRightString("MUTED", 310, 14, 2);
  } else {
    tft.drawRightString("Vol " + String(currentVolume * 5) + "%", 310, 14, 2);
  }
}

// Affichage bouton SLEEP
void drawSleepButton() {
  tft.fillRoundRect(10, 52, 70, 38, 4, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawCentreString("DODO", 45, 63, 2);
}
// Affichage bouton 20% au centre (X: 90 à 230 | Largeur: 140 | Hauteur: 38)
void drawVol20Button() {
  tft.fillRoundRect(90, 52, 140, 38, 4, TFT_DARKCYAN);
  tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
  tft.drawCentreString("VOL 20%", 160, 63, 2); 
}
// Affichage bouton MUTE
void drawMuteButton() {
  // Si le son est coupé : Orange. Si le son joue : Cyan
  uint16_t buttonColor = isMuted ? TFT_ORANGE : TFT_DARKCYAN;
  tft.fillRoundRect(240, 52, 70, 38, 4, buttonColor);
  tft.setTextColor(TFT_WHITE, buttonColor);
  tft.drawCentreString("FTG", 275, 63, 2);
}

// Nettoyage zone centrale et affichage du nom de la station
void audio_showstation(const char *infrastructure) {
// Efface tout l'intérieur du cadre central (Y de 96 à 298)
tft.fillRect(11, 96, 298, 83, TFT_BLACK);
tft.setTextColor(TFT_ORANGE, TFT_BLACK);
tft.drawCentreString(STATIONS_LIST[currentStationIndex].name, 160, 120, 4); // Configuration du texte : Couleur Orange, police 4
}

void setup() { //-------------------------------------------------------------
  Serial.begin(115200);

  currentVolume = 1; // Volume au démarrage à 1
  savedVolume = 1; // Volume sauvegardé à 1
  isMuted = false; // S'assure que le son n'est pas coupé par le bouton Mute

  tft.init(); // Initialisation de l'Écran TFT et Tactile
  tft.setRotation(3);  // Mode paysage
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);  // Allume le rétroéclairage
  // CONFIGURATION DES BROCHES DE VEILLE
  pinMode(INT_N_PIN, INPUT_PULLUP);
  pinMode(AP_ENABLE, OUTPUT);

  // Si l'ESP32-S3 vient de se réveiller d'une veille profonde (Deep Sleep)
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
    Serial.println("Réveil provoqué par l'écran tactile");
  // L'écran sort automatiquement de veille à l'initialisation complète
  }
  // Audio Init
  driver_es8311_init();
  RadioAudio.setPinout(I2S_BCK, I2S_WS, I2S_DOUT, I2S_MCK);
  RadioAudio.setVolume(currentVolume);

  // Ecran initial de connection au WiFi
  tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("Recherche WiFi ...", 160, 110, 4);

  WiFi.mode(WIFI_STA); // Configuration du mode Station
  // L'ESP32 gère lui-même la reconnexion en tâche de fond sans code dans le loop
  WiFi.setAutoReconnect(true); 
  bool connected = false;
  int tentativeGlobale = 1;
  const int MAX_TENTATIVES = 3; // 3 boucles de tentatives de connection au Wifi

  while (!connected && tentativeGlobale <= MAX_TENTATIVES) {
    tft.fillRect(10, 140, 300, 30, TFT_NAVY); // Message temporaire sur l'écran
    tft.drawCentreString("Essai Wi-Fi " + String(tentativeGlobale) + "/" + String(MAX_TENTATIVES), 160, 145, 2);

    for (int i = 0; i < NETWORKS_COUNT; i++) { // On parcourt la liste des réseaux enregistrés
      WiFi.begin(myNetworks[i].ssid, myNetworks[i].password);
      int timeout = 0; 
      while (WiFi.status() != WL_CONNECTED && timeout < 10) { // On attend max ~4 secondes (10 * 400ms) par réseau
        delay(400);
        timeout++;
      }
      if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        break; // Un réseau fonctionne, on sort du 'for'
      }
    }
    if (!connected) { // Pas de connection, on reboucle
      tentativeGlobale++;
      delay(1000); // Petite pause d'une seconde avant de relancer le cycle suivant
    }
  }
  
  // Initialisation Audio & Codec
  driver_es8311_init();
  RadioAudio.setPinout(I2S_BCK, I2S_WS, I2S_DOUT, I2S_MCK);
  RadioAudio.setVolume(currentVolume);

  
  // Configuration de la broche batterie
  pinMode(BAT_ADC_PIN, INPUT);
  analogReadResolution(12); 

  // Initialisation de la puce tactile FT6336U
  ft6336u.begin();

  // --- Affichage écran
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 45, TFT_DARKGREY); // Barre du haut
  
  // Nettoyage de l'arrière-plan d'abord
  tft.fillRect(0, 46, 320, 143, TFT_BLACK);
  
  // Dessin du cadre blanc par-dessus
  tft.drawRoundRect(10, 95, 300, 85, 6, TFT_WHITE);
  
  // Dessin des éléments d'interface
  updateBatteryDisplay();
  updateTimeDisplay();
  updateVolumeDisplay();
  
  drawSleepButton();
  drawVol20Button();
  drawMuteButton();
  drawButtons();

  if (connected) {
    Serial.println("\nWi-Fi Connecté avec succès !");
    RadioAudio.connecttohost(STATIONS_LIST[currentStationIndex].url);
    configTzTime(TZ_INFO, "pool.ntp.org", "europe.pool.ntp.org");
    
    tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
    tft.setTextSize(1); 
    tft.drawCentreString("--:--", 160, 10, 4); 
  } else {  // On a pas réussi à se connecter, on pleure
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("Hors ligne (Pas de Wi-Fi)", 160, 130, 4);
  }
}

void loop() { //----------------------------------------------------------------
  RadioAudio.loop();  

  unsigned long currentMillis = millis();

  // Rafraîchir l'horloge toutes les 15 secondes
  if (currentMillis - lastTimeUpdate >= 15000) {
    lastTimeUpdate = currentMillis;
    updateTimeDisplay();
  }

  // Rafraîchir l'icône de batterie toutes les 120 secondes
  if (currentMillis - lastBatteryUpdate >= 120000) {
    lastBatteryUpdate = currentMillis;
    updateBatteryDisplay();
  }

  // Lecture état de la puce tactile FT6336U
  tp = ft6336u.scan();
  if (tp.touch_count != 0) {
    // Calcul des coordonnées exactes (Mode Paysage)
    int t_x = 320 - tp.tp[0].y;
    int t_y = tp.tp[0].x;
    // Anti rebond
    delay(250);

    // Boutons SLEEP, Vol 20% et MUTE (Y: 52 à 90)
    if (t_y >= 52 && t_y <= 90) {

      // Bouton SLEEP (X: 10 à 80)
      if (t_x >= 10 && t_x <= 80) {
        enterSleepMode(); 
      }
      // Bouton Vol 20% (X: 240 à 310)
      else if (t_x >= 81 && t_x <= 230) {
        isMuted = false;      
        currentVolume = 4;    
        RadioAudio.setVolume(currentVolume);
        updateVolumeDisplay(); 
        drawMuteButton();      
      }
      // Bouton MUTE (X: 240 à 310)
      else if (t_x >= 240 && t_x <= 310) {
        if (!isMuted) {
          savedVolume = currentVolume;
          currentVolume = 0;
          isMuted = true;
        } else {
          currentVolume = savedVolume;
          isMuted = false;
        }
        RadioAudio.setVolume(currentVolume);
        updateVolumeDisplay(); // Met à jour le texte Vol: XX% en haut
        drawMuteButton();    // Change la couleur du bouton (Orange/Cyan)  
      }
    }

    // --- Zone des boutons tout en bas (Y entre 190 et 230)
    if (t_y >= 190 && t_y <= 230) {
      // Bouton < (X entre 10 et 75)
      if (t_x >= 10 && t_x <= 75) {
        currentStationIndex--;
        if (currentStationIndex < 0) currentStationIndex = TOTAL_STATIONS - 1;
        tft.fillRect(11, 96, 298, 83, TFT_BLACK); 
        tft.setTextColor(TFT_GOLD, TFT_BLACK);  // Affichage en zone centrale et en jaune de la station recherchée
        tft.drawCentreString(STATIONS_LIST[currentStationIndex].name, 160, 120, 4); // Police 4 à Y=120
        RadioAudio.connecttohost(STATIONS_LIST[currentStationIndex].url);
      }
      // Bouton - (X: 85 à 150)
      else if (t_x >= 85 && t_x <= 150) {
        if (isMuted) { 
          isMuted = false; 
          currentVolume = savedVolume; 
        }
        if (currentVolume > 0) currentVolume--;
        RadioAudio.setVolume(currentVolume);
        updateVolumeDisplay(); 
        drawMuteButton();      
      }
      // Bouton + (X: 160 à 225)
      else if (t_x >= 160 && t_x <= 225) {
        if (isMuted) { 
          isMuted = false; 
          currentVolume = savedVolume; 
        }
        if (currentVolume < 21) currentVolume++;
        RadioAudio.setVolume(currentVolume);
        updateVolumeDisplay();
        drawMuteButton();
        }
        // Bouton > (X entre 235 et 300)
        else if (t_x >= 235 && t_x <= 300) {
        currentStationIndex++;
        if (currentStationIndex >= TOTAL_STATIONS) currentStationIndex = 0;
        tft.fillRect(11, 96, 298, 83, TFT_BLACK);
        tft.setTextColor(TFT_GOLD, TFT_BLACK);  // Affichage en zone centrale et en jaune de la station recherchée
        tft.drawCentreString(STATIONS_LIST[currentStationIndex].name, 160, 120, 4);
        RadioAudio.connecttohost(STATIONS_LIST[currentStationIndex].url);
        }
      }
  }
}
