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
#define BAT_ADC_PIN 9
#define BACKLIGHT_PIN 1  // Broche rétroéclairage
#define BAT_ADC_PIN 9    // Broche ADC batterie

#define FNK0104AB_2P8_240x320_ILI9341

// Règle heure France (Paris)
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3"; // Règle heure France (Paris)

// Paramètre pour la batterie
unsigned long lastBatteryUpdate = 0;

// Paramètre pour l'horloge
unsigned long lastTimeUpdate = 0;

TFT_eSPI tft = TFT_eSPI();

// Instanciation du tactile officiel Freenove
FT6336U ft6336u(I2C_SDA, I2C_SCL, RST_N_PIN, INT_N_PIN);
FT6336U_TouchPointType tp;

FNK0104AB_2P8_240x320_ILI9341
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

// Définition de la structure pour associer un nom à une URL
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

// Volume actuel (0 à 21 max pour audioI2S)
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
// Ajoutez ici vos réseaux (le code testera la liste au démarrage)
const WifiProfile myNetworks[] = {
  {"SSID1", "password"},
  {"SSID2", "password"},
  {"SSID3", "password"},
  {"SSID4", "password"}
};
const int NETWORKS_COUNT = sizeof(myNetworks) / sizeof(myNetworks[0]);

void driver_es8311_init(void) {
  pinMode(AP_ENABLE, OUTPUT);
  digitalWrite(AP_ENABLE, LOW);  // Réveille l'ampli
  Wire.begin(I2C_SDA, I2C_SCL, I2C_SPEED);
  if (es8311_codec_init() != ESP_OK) {
    Serial.println("ES8311 Codec en échec.");
  }
}

// Fonction mise en veille
void enterSleepMode() {
  Serial.println("Entrée en mode veille...");
  // Sauvegarde du volume en mémoire Flash (NVS)
  preferences.begin("radio_settings", false); // Ouvre l'espace "radio_settings" en mode écriture
  // Si on est en Mute, on sauvegarde le volume réel restaurable, sinon le volume actuel
  int volumeToSave = isMuted ? savedVolume : currentVolume;
  preferences.putInt("volume", volumeToSave);
  preferences.end(); // Ferme proprement la mémoire
  Serial.printf("Volume %d sauvegarde en NVS !\n", volumeToSave);
  // Couper le son et l'amplificateur audio (AP_ENABLE = 1)
  RadioAudio.stopSong();
  digitalWrite(AP_ENABLE, HIGH); // Met l'ampli en veille
  // Éteindre le rétroéclairage de l'écran (TFT_BL = 45)
  digitalWrite(TFT_BL, LOW); 
  // Mettre l'écran ILI9341 lui-même en mode veille (via SPI)
  tft.writecommand(0x10); 
  delay(120);             
  // Configurer la broche d'interruption tactile (GPIO 17) comme source de réveil unique
  // L'argument '0' indique qu'on souhaite se réveiller sur un état BAS (LOW) quand on touche l'écran
  esp_sleep_enable_ext0_wakeup((gpio_num_t)INT_N_PIN, 0);

  // 5. Extinction de l'ESP32-S3
  esp_deep_sleep_start();
}

// Dessiner boutons du dessous
void drawButtons() {
  tft.setTextSize(1);

  // Bouton Station Précédente
  tft.fillRoundRect(10, 190, 65, 40, 4, TFT_DARKCYAN);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("<", 42, 203, 2);

  // Bouton Volume Moins
  tft.fillRoundRect(85, 190, 65, 40, 4, TFT_MAROON);
    tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("-", 117, 203, 2);

  // Bouton Volume Plus
  tft.fillRoundRect(160, 190, 65, 40, 4, TFT_MAROON);
    tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("+", 192, 203, 2);

  // Bouton Station Suivante
  tft.fillRoundRect(235, 190, 65, 40, 4, TFT_DARKCYAN);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(">", 267, 203, 2);
}

// Dessiner bandeau supérieur
// 1. Affichage de la Batterie (Tout à gauche : X = 10)
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

// 2. Affichage de l'Heure (Au centre : X = 160)
void updateTimeDisplay() {
  struct tm timeinfo;
  // Si le serveur NTP n'a pas encore répondu, on ne fait rien et on garde l'affichage "--:--"
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Heure NTP non disponible pour le moment...");
    return; 
  }
  char timeStr[6];
  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  // Dès que l'heure est reçue, elle s'affiche proprement au centre
    tft.fillRect(105, 3, 110, 38, TFT_DARKGREY);  // Nettoyage zone affichage heure
  tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);   // Heure en jaune
  // On applique un multiplicateur de taille de 2 sur la police 4
  tft.setTextSize(2); 
  tft.drawCentreString(timeStr, 160, 6, 4);  //Ecriture de l'heure en police 4 Position Y calée à 6 pour un centrage vertical parfait avec le multiplicateur
  tft.setTextSize(1);
  // IMPORTANT : Toujours remettre la taille à 1 après pour ne pas 
  // agrandir accidentellement les autres textes du programme !
  }  

// 3. Affichage du Volume (Tout à droite : X = 310)
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
  tft.fillRoundRect(10, 52, 70, 26, 4, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawCentreString("SLEEP", 45, 57, 2);
  }

  // Affichage bouton MUTE
void drawMuteButton() {
  // Si le son est coupé : Orange. Si le son joue : Cyan.
  uint16_t buttonColor = isMuted ? TFT_ORANGE : TFT_DARKCYAN;
  uint16_t textColor   = isMuted ? TFT_WHITE : TFT_WHITE;
  
  tft.fillRoundRect(240, 52, 70, 26, 4, buttonColor);
  tft.setTextColor(textColor, buttonColor);
  tft.drawCentreString("MUTE", 275, 57, 2);
}


void setup() { //-------------------------------------------------------------------------
  Serial.begin(115200);

  // Volume au démarrage à 1
  currentVolume = 1; 
  savedVolume = 1; // Aligne la variable sauvegardée pour le bouton Mute
  isMuted = false; // S'assure que le son n'est pas coupé graphiquement

  // Initialisation de l'Écran TFT et Tactile
  tft.init();
  tft.setRotation(3);  // Mode paysage (320x240)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);  // Allume le rétroéclairage

  // CONFIGURATION DES BROCHES DE VEILLE
  pinMode(INT_N_PIN, INPUT_PULLUP);
  pinMode(AP_ENABLE, OUTPUT);

  // Si l'ESP32-S3 vient de se réveiller d'une veille profonde (Deep Sleep) :
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
  Serial.println("Réveil provoqué par l'écran tactile !");
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
  // Connexion Wi-Fi ------------------------------------------------------------------------------------------
  WiFi.mode(WIFI_STA); // Configuration du mode Station
  // L'ESP32 gère lui-même la reconnexion en tâche de fond sans code dans le loop
  WiFi.setAutoReconnect(true); 
  bool connected = false;
    int tentativeGlobale = 1;
  const int MAX_TENTATIVES = 3; // 3 boucles de tentatives de connection au Wifi

  while (!connected && tentativeGlobale <= MAX_TENTATIVES) {
    Serial.printf("\n--- Cycle de recherche Wi-Fi %d sur %d ---\n", tentativeGlobale, MAX_TENTATIVES);
    
    // Message temporaire sur l'écran
    tft.fillRect(10, 140, 300, 30, TFT_NAVY);
    tft.drawCentreString("Essai Wi-Fi " + String(tentativeGlobale) + "/" + String(MAX_TENTATIVES), 160, 145, 2);

    // On parcourt la liste de vos réseaux enregistrés
    for (int i = 0; i < NETWORKS_COUNT; i++) {
      Serial.printf("Tentative sur : %s\n", myNetworks[i].ssid);
      WiFi.begin(myNetworks[i].ssid, myNetworks[i].password);

      int timeout = 0;
      // On attend max ~4 secondes (10 * 400ms) par réseau
      while (WiFi.status() != WL_CONNECTED && timeout < 10) {
        delay(400);
        Serial.print(".");
        timeout++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        break; // Un réseau fonctionne, on sort du 'for'
      }
    }

    if (!connected) {
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
  analogReadResolution(12); // Résolution 12 bits (0-4095)

  // Initialisation indispensable de la puce tactile FT6336U
  ft6336u.begin();

  // Dessin du lecteur
  // Écran principal : fond noir et barre supérieure gris foncé
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 45, TFT_DARKGREY); // Barre en haut
  // Cadre du lecteur tactile
  tft.drawRoundRect(10, 85, 300, 95, 6, TFT_WHITE);
  // Dessin des trois indicateurs de la barre supérieure
  updateBatteryDisplay();
  updateTimeDisplay();
  updateVolumeDisplay();
  // Bouton SLEEP à gauche
  drawSleepButton();
  // Bouton MUTE à droite
  drawMuteButton();
  // Dessin des boutons du bas
  drawButtons();


  if (connected) {
    Serial.println("\nWi-Fi Connecté avec succès !");
    // Lancement immédiat du flux audio
    RadioAudio.connecttohost(STATIONS_LIST[currentStationIndex].url);
    
    // Et ensuite initialisation du NTP en tâche de fond (Non-bloquant)
    // Cette fonction ne bloque pas le code. L'ESP32 va demander l'heure au serveur
    // pendant que la musique commence à jouer.
    configTzTime(TZ_INFO, "pool.ntp.org", "europe.pool.ntp.org");
    tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);

    tft.setTextSize(2); // Double la taille des caractères
    tft.drawCentreString("--:--", 160, 6, 4); // Affiche des tirets en attendant l'heure
    tft.setTextSize(1); // Reset de la taille des caractères

  } else {
    Serial.println("\nÉchec WiFi définitif : Aucun réseau trouvé après 3 essais.");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("Hors ligne (Pas de Wi-Fi)", 160, 110, 4);
  }
}



void loop() { //--------------------------------------------------------------------------------------

  RadioAudio.loop();  // Doit tourner en continu

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
  // Vérifier si un doigt touche l'écran
  if (tp.touch_count != 0) {
    // Calcul des coordonnées exactes (Mode Paysage)
    int t_x = 320-tp.tp[0].y;
    int t_y = tp.tp[0].x;
    // Anti-rebond
    delay(250);

    // Boutons SLEEP et MUTE (Y: 42 à 68) ---
    if (t_y >= 42 && t_y <= 78) {
      
      // Bouton SLEEP (X: 10 à 80)
      if (t_x >= 10 && t_x <= 80) {
        enterSleepMode(); 
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
        drawMuteButton();      // Change la couleur du bouton (Orange/Cyan)
      }
    }

    // Affichage dans le moniteur série (pour info)
    Serial.printf("Toucher sur écran détecté en X: %d, Y: %d\n", t_x, t_y);

    // --- ZONE DES BOUTONS tout en BAS (Y entre 190 et 230) ---
    if (t_y >= 190 && t_y <= 230) {

      // Bouton PREC (X entre 10 et 75)
      if (t_x >= 10 && t_x <= 75) {
        currentStationIndex--;
        if (currentStationIndex < 0) currentStationIndex = TOTAL_STATIONS - 1;
        tft.fillRect(11, 86, 298, 92, TFT_BLACK);
        tft.setTextColor(TFT_GOLD, TFT_BLACK);
        tft.drawCentreString(STATIONS_LIST[currentStationIndex].name, 160, 115, 4); // Police 4 à Y=115
        RadioAudio.connecttohost(STATIONS_LIST[currentStationIndex].url);
      }

      // --- Bouton VOL - (X: 85 à 150) ---
      else if (t_x >= 85 && t_x <= 150) {
        if (isMuted) { 
          isMuted = false; 
          currentVolume = savedVolume; 
        }
        if (currentVolume > 0) currentVolume--;
        
        RadioAudio.setVolume(currentVolume);
        updateVolumeDisplay(); 
        drawMuteButton();      // REPASSE LE BOUTON MUTE EN CYAN !
      }
      
      // --- Bouton VOL + (X: 160 à 225) ---
      else if (t_x >= 160 && t_x <= 225) {
        if (isMuted) { 
          isMuted = false; 
          currentVolume = savedVolume; 
        }
        if (currentVolume < 21) currentVolume++;
        
        RadioAudio.setVolume(currentVolume);
        updateVolumeDisplay(); 
        drawMuteButton();      // REPASSE LE BOUTON MUTE EN CYAN !
      }

      // Bouton SUIV (X entre 235 et 300)
      else if (t_x >= 235 && t_x <= 300) {
        currentStationIndex++;
        if (currentStationIndex >= TOTAL_STATIONS) currentStationIndex = 0;
        tft.fillRect(11, 86, 298, 92, TFT_BLACK);
        tft.setTextColor(TFT_GOLD, TFT_BLACK);
        tft.drawCentreString(STATIONS_LIST[currentStationIndex].name, 160, 115, 4); // Police 4 à Y=115
        RadioAudio.connecttohost(STATIONS_LIST[currentStationIndex].url);
      }
    }
  }
}


// Nettoyage et affichage de la station
void audio_showstation(const char *infrastructure) {
  // Efface tout l'intérieur du cadre central pour éviter les résidus (Y de 86 à 178)
  tft.fillRect(11, 86, 298, 92, TFT_BLACK);
  // Configuration du texte : Couleur Orange, Police 4 (Grande et propre)
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  // Centrage parfait au milieu du cadre (X = 160, Y = 115)
  tft.drawCentreString(STATIONS_LIST[currentStationIndex].name, 160, 115, 4);
}


// Nettoyage zone du titre
void audio_showstreamtitle(const char *title) {
// Affiche toujours l'information dans le moniteur série pour le diagnostic
  Serial.print("Flux info (non affiche) : ");
  Serial.println(title);
  // Le code graphique a été retiré pour laisser le nom de la station seul et centré
}

