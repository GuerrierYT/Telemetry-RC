#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <QMC5883LCompass.h>
#include <IBusBM.h>

// --- NOUVELLES BIBLIOTHÈQUES RÉSEAU ---
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>

#if __has_include("config.local.h")
#include "config.local.h"
#else
#include "config.example.h"
#endif

// ==========================================
// CONFIGURATION RÉSEAU (JETBOAT)
// ==========================================
const char* ssid = WIFI_AP_SSID;
const char* password = WIFI_AP_PASSWORD;

IPAddress local_IP(WIFI_AP_LOCAL_IP[0], WIFI_AP_LOCAL_IP[1], WIFI_AP_LOCAL_IP[2], WIFI_AP_LOCAL_IP[3]);
IPAddress gateway(WIFI_AP_GATEWAY[0], WIFI_AP_GATEWAY[1], WIFI_AP_GATEWAY[2], WIFI_AP_GATEWAY[3]);
IPAddress subnet(WIFI_AP_SUBNET[0], WIFI_AP_SUBNET[1], WIFI_AP_SUBNET[2], WIFI_AP_SUBNET[3]);

DNSServer dnsServer;
AsyncWebServer server(80);

// ==========================================
// STRUCTURE DES DONNÉES ET FREERTOS
// ==========================================
struct DataBateau {
  unsigned long timestamp;
  float tension;
  float courant;
  float vitesse;
  double lat;
  double lon;
  int cap;
  uint16_t ch1, ch2, ch3, ch4;
};

QueueHandle_t fileDonnees;
volatile bool demandeEnregistrement = false; // LE VERROU PRINCIPAL

// ==========================================
// CONFIGURATION DES BROCHES (PINS)
// ==========================================
const int CS_PIN = 5;             
const int pinTension = 35;        
const int pinCapteurCourant = 34; 
const int IBUS_TELEMETRY_RX_PIN = 27;
const int IBUS_TELEMETRY_TX_PIN = 26;
const int IBUS_CHANNELS_RX_PIN = 32;
const int IBUS_CHANNELS_TX_PIN = 1;
const int GPS_RX_PIN = 16;        
const int GPS_TX_PIN = 17;        
const int LED_PIN = 2;            

const uint8_t UART_RX_FIFO_THRESHOLD = 1;
const unsigned long UART_BEGIN_TIMEOUT_MS = 20000UL;
const size_t GPS_RX_BUFFER_SIZE = 1024;
const size_t GPS_TX_BUFFER_SIZE = 128;
const size_t IBUS_RX_BUFFER_SIZE = 256;
const size_t IBUS_TX_BUFFER_SIZE = 128;

// ==========================================
// PARAMÈTRES ET VARIABLES GLOBALES
// ==========================================
unsigned long chronoSD = 0;
const int intervalleSD = 100; // 10Hz
char nomFichier[32] = "/donnees_bateau.csv"; 
File fichierLog;

IBusBM IBusTelemetrie;
IBusBM IBusCanaux;
uint16_t channels[14];
unsigned long chronoTelemetrie = 0;

const int CHANNEL_INDEX = 3;  // Ch4
const int BUTTON_THRESHOLD = 1750;
bool boutonEnfonce = false;
unsigned long debutAppui = 0;
const unsigned long DUREE_COURTE = 1000;
const unsigned long DUREE_LONGUE = 3000;

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
float vitesseActuelle = 0.0;
double latActuelle = 0.0;
double lonActuelle = 0.0;
const byte config10Hz[] = {
  0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0x64, 0x00, 0x01, 0x00, 0x01, 0x00, 0x7A, 0x12
};

QMC5883LCompass compass;
int capActuel = 0;
unsigned long chronoBoussole = 0;

const int NB_MESURES = 100;
unsigned long chronoLectureTension = 0;
int historiqueTension[NB_MESURES];
int indexTension = 0;
long sommeGlissanteTension = 0;
float tensionActuelle = 0.0;
float ratioDiviseur = 11.0; 
float correctionESP32 = 1.0808;

unsigned long chronoLectureCourant = 0;
int historiqueCourant[NB_MESURES];
int indexCourant = 0;
long sommeGlissanteCourant = 0;
float courantActuel = 0.0;
float tensionZero; 
const float sensibilite = 0.0132; 

unsigned long previousMillis = 0;
bool ledState = LOW;
const long interval = 250;

// ==========================================
// DÉCLARATION DES FONCTIONS (PROTOTYPES)
// ==========================================
void creerNouveauFichier();
int obtenirProchainNumero();
void actualiserCanauxIbus();
void tareAuto();
void lireGPS();
void lireBoussole();
void actualiserBufferCourant();
void actualiserBufferTension();
void calculerMoyennesFinales();
void verifierBouton();
void verifierEnregistrement();
void envoyerFichierLittleFS(AsyncWebServerRequest *request, const char *chemin, const char *typeMime);
String echapperJson(const String &texte);

// ==========================================
// INITIALISATION DU SERVEUR WEB (PORTAIL CAPTIF)
// ==========================================
void envoyerFichierLittleFS(AsyncWebServerRequest *request, const char *chemin, const char *typeMime) {
  if (!LittleFS.exists(chemin)) {
    request->send(500, "text/plain", "Fichier LittleFS introuvable.");
    return;
  }

  AsyncWebServerResponse *response = request->beginResponse(LittleFS, chemin, typeMime);
  response->addHeader("Connection", "close");
  request->send(response);
}

String echapperJson(const String &texte) {
  String resultat;
  resultat.reserve(texte.length() + 4);

  for (size_t i = 0; i < texte.length(); i++) {
    char c = texte.charAt(i);
    switch (c) {
      case '"': resultat += "\\\""; break;
      case '\\': resultat += "\\\\"; break;
      case '\b': resultat += "\\b"; break;
      case '\f': resultat += "\\f"; break;
      case '\n': resultat += "\\n"; break;
      case '\r': resultat += "\\r"; break;
      case '\t': resultat += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buffer[7];
          snprintf(buffer, sizeof(buffer), "\\u%04x", c);
          resultat += buffer;
        } else {
          resultat += c;
        }
        break;
    }
  }

  return resultat;
}

void initServeurWeb() {
  // 1. ROUTES : ACCUEIL ET ASSETS LITTLEFS
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    envoyerFichierLittleFS(request, "/index.html", "text/html");
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    envoyerFichierLittleFS(request, "/style.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
    envoyerFichierLittleFS(request, "/script.js", "application/javascript");
  });

  server.on("/liste.js", HTTP_GET, [](AsyncWebServerRequest *request){
    envoyerFichierLittleFS(request, "/liste.js", "application/javascript");
  });


  // 2. ROUTES : LISTE DES FICHIERS ET API SD
  server.on("/liste", HTTP_GET, [](AsyncWebServerRequest *request){
    if (demandeEnregistrement) {
      request->send(503, "text/plain", "ACCES REFUSE : Arretez l'enregistrement (CH4) pour lire la carte SD.");
      return;
    }

    envoyerFichierLittleFS(request, "/liste.html", "text/html");
  });

  server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request){
    if (demandeEnregistrement) {
      request->send(503, "text/plain", "ACCES REFUSE : Arretez l'enregistrement (CH4) pour lire la carte SD.");
      return;
    }

    File root = SD.open("/");
    if (!root) {
      request->send(500, "text/plain", "Erreur lecture SD");
      return;
    }

    String json = "{\"files\":[";
    File file = root.openNextFile();
    bool premier = true;

    while (file) {
      if (!file.isDirectory()) {
        if (!premier) json += ",";
        String nom = String(file.name());
        json += "{\"name\":\"" + echapperJson(nom) + "\",\"size\":" + String(file.size()) + "}";
        premier = false;
      }

      file.close();
      file = root.openNextFile();
    }

    root.close();
    json += "]}";

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Connection", "close");
    request->send(response);
  });
  // 3. ROUTE : TÉLÉCHARGEMENT
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
    // VERROU SÉCURITÉ
    if (demandeEnregistrement) {
      request->send(503, "text/plain", "ACCES REFUSE : SD occupee.");
      return;
    }

    if (request->hasParam("file")) {
      String cheminComplet = "/" + request->getParam("file")->value();
      if (SD.exists(cheminComplet)) {
        request->send(SD, cheminComplet, "text/csv", true);
      } else {
        request->send(404, "text/plain", "Fichier introuvable.");
      }
    }
  });

  // 4. LE PIÈGE DU PORTAIL CAPTIF (Redirection Apple/Android)
  server.onNotFound([](AsyncWebServerRequest *request){
    request->redirect("http://" + local_IP.toString() + "/");
  });

  // 5. ROUTE : LES DONNÉES EN DIRECT (API JSON)
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    float puissance = tensionActuelle * courantActuel; // P = U * I
    String json = "{";
    json += "\"vitesse\":" + String(vitesseActuelle, 1) + ",";
    json += "\"tension\":" + String(tensionActuelle, 2) + ",";
    json += "\"courant\":" + String(courantActuel, 1) + ",";
    json += "\"puissance\":" + String(puissance, 0) + ",";
    json += "\"cap\":" + String(capActuel) + ",";
    json += "\"sat\":" + String(gps.satellites.isValid() ? gps.satellites.value() : 0) + ",";
    json += "\"mode\":" + String(demandeEnregistrement ? 1 : 0);
    json += "}";
    
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Connection", "close");
    request->send(response);
  });

  server.begin();
}

// ==========================================
// TÂCHE CORE 0 : L'ARCHIVISTE + RÉSEAU
// ==========================================
void tacheCarteSD(void *pvParameters) {
  DataBateau colis;
  bool enCoursEnregistrement = false;

  for (;;) { 
    // 1. LE RÉSEAU : On maintient le téléphone connecté (Portail Captif)
    dnsServer.processNextRequest();

    // 2. GESTION DU FICHIER
    if (demandeEnregistrement && !enCoursEnregistrement) {
      creerNouveauFichier();
      enCoursEnregistrement = true;
    } 
    else if (!demandeEnregistrement && enCoursEnregistrement) {
      if (fichierLog) fichierLog.close();
      enCoursEnregistrement = false;
    }

    // 3. ÉCRITURE SD (Attente max 10ms pour ne pas étouffer le DNS)
    if (xQueueReceive(fileDonnees, &colis, 10 / portTICK_PERIOD_MS) == pdPASS) {
      if (enCoursEnregistrement && fichierLog) {
        fichierLog.print(colis.timestamp); fichierLog.print(";");
        fichierLog.print(colis.tension); fichierLog.print(";");
        fichierLog.print(colis.courant); fichierLog.print(";");
        fichierLog.print(colis.vitesse); fichierLog.print(";");
        fichierLog.print(colis.lat, 6); fichierLog.print(";");
        fichierLog.print(colis.lon, 6); fichierLog.print(";");
        fichierLog.print(colis.cap); fichierLog.print(";");
        fichierLog.print(colis.ch1); fichierLog.print(";");
        fichierLog.print(colis.ch2); fichierLog.print(";");
        fichierLog.print(colis.ch3); fichierLog.print(";");
        fichierLog.println(colis.ch4); 
        
        fichierLog.flush(); 
      }
    }
  }
}

// ==========================================
// INITIALISATION (SETUP) - CORE 1
// ==========================================
void setup() {
  IBusCanaux.begin(Serial, IBUSBM_NOTIMER, IBUS_CHANNELS_RX_PIN, IBUS_CHANNELS_TX_PIN,
                   IBUS_RX_BUFFER_SIZE, 0, UART_RX_FIFO_THRESHOLD);
  delay(2000);
  Serial.println();
  Serial.println("Demarrage Telemetrie ESP32 Core 3.x");

  pinMode(LED_PIN, OUTPUT);
  analogReadResolution(12);

  gpsSerial.setRxBufferSize(GPS_RX_BUFFER_SIZE);
  gpsSerial.setTxBufferSize(GPS_TX_BUFFER_SIZE);
  gpsSerial.begin(115200, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN, false,
                  UART_BEGIN_TIMEOUT_MS, UART_RX_FIFO_THRESHOLD);
  Wire.begin(21, 22);
  compass.init();

  IBusTelemetrie.begin(Serial1, IBUSBM_NOTIMER, IBUS_TELEMETRY_RX_PIN, IBUS_TELEMETRY_TX_PIN,
                       IBUS_RX_BUFFER_SIZE, IBUS_TX_BUFFER_SIZE, UART_RX_FIFO_THRESHOLD);
  IBusTelemetrie.addSensor(IBUSS_EXTV);
  IBusTelemetrie.addSensor(IBUSS_TEMP);
  IBusTelemetrie.addSensor(IBUSS_RPM);

  Serial.println("Configuration du GPS a 10 Hz...");
  gpsSerial.write(config10Hz, sizeof(config10Hz));
  delay(500);

  // Initialisation SD [cite: 1, 5, 11]
  if (!SD.begin(CS_PIN)) {
    Serial.println("ERREUR CRITIQUE : Carte SD introuvable !");
  } else {
    Serial.println("Carte SD initialisee avec succes.");
  }

  if (!LittleFS.begin(true)) {
    Serial.println("ERREUR CRITIQUE : LittleFS introuvable !");
  } else {
    Serial.println("LittleFS initialise avec succes.");
  }
  
  tareAuto();

  for (int i = 0; i < NB_MESURES; i++) {
    historiqueTension[i] = 0;
    historiqueCourant[i] = 0;
  }

  // --- NOUVEAU : DÉMARRAGE DU WI-FI ET DU PORTAIL CAPTIF ---
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);
  WiFi.setSleep(false); 
  
  dnsServer.start(53, "*", local_IP);
  initServeurWeb();
  Serial.print("Serveur Web actif sur IP : ");
  Serial.println(WiFi.softAPIP());

  // --- CRÉATION DU BUFFER ET DE LA TÂCHE CORE 0 ---
  fileDonnees = xQueueCreate(15, sizeof(DataBateau));
  xTaskCreatePinnedToCore(
    tacheCarteSD, "TacheSD", 4096, NULL, 1, NULL, 0 
  );
  
  Serial.println(">>> Systeme TOTALEMENT operationnel (DUAL-CORE + Wi-Fi) ! <<<");
}

// ==========================================
// BOUCLE PRINCIPALE (CORE 1 - LE PILOTE)
// ==========================================
void loop() {
  // LE PILOTE RESTE TOTALEMENT LIBÉRÉ DU RÉSEAU ET DE LA CARTE SD !
  lireGPS();
  IBusTelemetrie.loop(); 
  actualiserCanauxIbus();

  verifierBouton();
  verifierEnregistrement(); 
  actualiserBufferCourant(); 
  actualiserBufferTension(); 
  calculerMoyennesFinales(); 

  if (millis() - chronoBoussole >= 100) {
    lireBoussole();
    chronoBoussole = millis();
  }

  if (millis() - chronoTelemetrie >= 500) {
    IBusTelemetrie.setSensorMeasurement(1, tensionActuelle * 100);
    IBusTelemetrie.setSensorMeasurement(2, (courantActuel + 40.0) * 10);   
    IBusTelemetrie.setSensorMeasurement(3, (int)vitesseActuelle);  
    chronoTelemetrie = millis();
  }

  // --- ACQUISITION 10 HZ ABSOLU ---
  if (demandeEnregistrement) {
    if (millis() - chronoSD >= intervalleSD) {
      chronoSD += intervalleSD;
      
      DataBateau colis;
      colis.timestamp = millis(); 
      colis.tension = tensionActuelle;
      colis.courant = courantActuel;
      colis.vitesse = vitesseActuelle;
      colis.lat = latActuelle;
      colis.lon = lonActuelle;
      colis.cap = capActuel;
      colis.ch1 = channels[0];
      colis.ch2 = channels[1];
      colis.ch3 = channels[2];
      colis.ch4 = channels[3];
      
      if (xQueueSend(fileDonnees, &colis, 0) != pdPASS) {
        Serial.println("Buffer plein ! Une trame a ete perdue.");
      }
    }
  }
}

// ==========================================
// RESTE DES FONCTIONS CAPTEURS / I-BUS...
// ==========================================
void lireGPS() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
  if (gps.speed.isValid()) vitesseActuelle = gps.speed.kmph();
  if (gps.location.isValid()) {
    latActuelle = gps.location.lat();
    lonActuelle = gps.location.lng();
  }
}

void lireBoussole() {
  compass.read();
  int x = compass.getX();
  int y = compass.getY();
  if (x == 0 && y == 0) {
    Wire.begin(21, 22);
    compass.init();
    return; 
  }
  int azimut = compass.getAzimuth();
  azimut = 180 - azimut;
  if (azimut < 0) {
    azimut += 360;
  }
  capActuel = azimut;
}

void actualiserBufferCourant() {
  unsigned long tempsActuel = millis();
  if (tempsActuel - chronoLectureCourant >= 1) { 
    chronoLectureCourant = tempsActuel;
    int nouvelleLecture = analogRead(pinCapteurCourant);
    sommeGlissanteCourant -= historiqueCourant[indexCourant];
    historiqueCourant[indexCourant] = nouvelleLecture;
    sommeGlissanteCourant += nouvelleLecture;
    indexCourant = (indexCourant + 1) % NB_MESURES;
  }
}

void actualiserBufferTension() {
  unsigned long tempsActuel = millis();
  if (tempsActuel - chronoLectureTension >= 1) {
    chronoLectureTension = tempsActuel;
    int nouvelleLecture = analogRead(pinTension);
    sommeGlissanteTension -= historiqueTension[indexTension];
    historiqueTension[indexTension] = nouvelleLecture;
    sommeGlissanteTension += nouvelleLecture;
    indexTension = (indexTension + 1) % NB_MESURES;
  }
}

void calculerMoyennesFinales() {
  float moyenneCourant = sommeGlissanteCourant / (float)NB_MESURES;
  float tensionLueCourant = (moyenneCourant / 4095.0) * 3.3;
  courantActuel = (tensionLueCourant - tensionZero) / sensibilite;
  float moyenneTension = sommeGlissanteTension / (float)NB_MESURES;
  float tensionBroche = (moyenneTension / 4095.0) * 3.3;
  tensionActuelle = tensionBroche * ratioDiviseur * correctionESP32;
}

void tareAuto() {
  Serial.println("Calibration du capteur de courant en cours...");
  long sommeCalibration = 0;
  int nombreDeMesures = 500;
  for (int i = 0; i < nombreDeMesures; i++) {
    sommeCalibration += analogRead(pinCapteurCourant);
    delay(2);
  }
  tensionZero = ((sommeCalibration / (float)nombreDeMesures) / 4095.0) * 3.3;
  Serial.print("-> Zero fixe a : ");
  Serial.print(tensionZero, 3);
  Serial.println(" V");
}

int obtenirProchainNumero() {
  int maxNum = 0;
  File root = SD.open("/");
  if (!root) return 1;
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String nom = String(file.name());
      if (nom.indexOf("Log_") >= 0) {
        int indexDebut = nom.indexOf("Log_") + 4;
        int indexFin = nom.indexOf(".csv");
        if (indexFin > indexDebut) {
          int numero = nom.substring(indexDebut, indexFin).toInt();
          if (numero > maxNum) maxNum = numero;
        }
      }
    }
    file = root.openNextFile();
  }
  return maxNum + 1;
}

void actualiserCanauxIbus() {
  for (int i = 0; i < 14; i++) {
    channels[i] = IBusCanaux.readChannel(i);
  }
}

void creerNouveauFichier() {
  int prochainNum = obtenirProchainNumero();
  snprintf(nomFichier, sizeof(nomFichier), "/Log_%03d.csv", prochainNum);
  fichierLog = SD.open(nomFichier, FILE_WRITE); 
  if (fichierLog) {
    fichierLog.println("Temps(ms);Tension(V);Courant(A);Vitesse(km/h);Lat;Lon;Cap(deg);Ch1;Ch2;Ch3;Ch4");
    fichierLog.flush(); 
    Serial.print("Nouveau fichier cree : ");
    Serial.println(nomFichier);
  } else {
    Serial.println("Erreur SD");
    demandeEnregistrement = false;
  }
}

void verifierBouton() {
  uint16_t valeurActuelle = channels[CHANNEL_INDEX];
  if (valeurActuelle > BUTTON_THRESHOLD && !boutonEnfonce) {
    boutonEnfonce = true;
    debutAppui = millis(); 
    digitalWrite(LED_PIN, HIGH);
  }
  else if (valeurActuelle < (BUTTON_THRESHOLD - 200) && boutonEnfonce) {
    boutonEnfonce = false;
    digitalWrite(LED_PIN, LOW);
    unsigned long dureeAppui = millis() - debutAppui;
    if (dureeAppui >= DUREE_COURTE && dureeAppui < DUREE_LONGUE) {
      if (demandeEnregistrement) {
        demandeEnregistrement = false;
        Serial.println(">>> ARRET demande !");
      } else {
        demandeEnregistrement = true;
        chronoSD = millis();
        Serial.println(">>> DEMARRAGE demande !");
      }
    }
  }
}

void verifierEnregistrement() {
  if (demandeEnregistrement) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  }
}
