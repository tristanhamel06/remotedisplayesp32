// Remote Display - ESP32-DevKitC + ecran TFT ILI9341 2.8" + bot Telegram
//
// Tu envoies une photo ou un texte a ton bot Telegram, de n'importe ou,
// et ca s'affiche sur l'ecran. Le dernier message est sauvegarde dans la
// memoire flash, donc il revient apres une panne de courant.
//
// Fichiers :
//   display.cpp   dessin sur l'ecran (texte, photos, ecrans d'info)
//   telegram.cpp  communication avec l'API de Telegram
//   secrets.h     ton token de bot et ton ID (pas sur GitHub)
//
// Branchement : voir README.md ou hardware/esp32display.kicad_sch

#include <WiFi.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "secrets.h"
#include "display.h"
#include "telegram.h"

// Nom du WiFi temporaire cree pour configurer le vrai WiFi
#define WIFI_SETUP_NAME "Ecran-Config"

// Taille max d'une photo a telecharger (doit rentrer dans la RAM)
#define MAX_JPEG_SIZE (150 * 1024)

#define LAST_IMAGE_FILE "/last.jpg"
#define LAST_TEXT_FILE  "/last.txt"

// ---------------------------------------------------------------------------
// Bouton dodo : un clic eteint l'ecran pendant 5 heures, un autre clic le
// rallume. Une petite tache FreeRTOS surveille le bouton en parallele, parce
// que loop() peut rester bloquee jusqu'a 20 s a attendre Telegram.
// ---------------------------------------------------------------------------

#define BUTTON_PIN 32                              // bouton entre GPIO 32 et GND
#define SLEEP_DURATION_MS (5UL * 60 * 60 * 1000)   // 5 heures
#define DEBOUNCE_MS 50

Preferences prefs;
volatile uint8_t brightness = 100;  // luminosite normale, en %
volatile bool sleeping = false;
volatile unsigned long sleepStart = 0;

void loadSettings() {
  prefs.begin("ecran", false);
  brightness = prefs.getUChar("brightness", 100);
}

void saveSettings() {
  prefs.putUChar("brightness", brightness);
}

// Minutes avant que l'ecran se rallume (0 s'il est allume)
unsigned long sleepMinutesLeft() {
  if (!sleeping) return 0;
  unsigned long elapsed = millis() - sleepStart;
  if (elapsed >= SLEEP_DURATION_MS) return 0;
  return (SLEEP_DURATION_MS - elapsed) / 60000;
}

String sleepStatus() {
  if (!sleeping) return "L'écran est allumé.";
  unsigned long m = sleepMinutesLeft();
  return "L'écran est éteint (bouton dodo). Il se rallume dans " +
         String(m / 60) + " h " + String(m % 60) + " min.";
}

void backlightTask(void*) {
  bool lastReading = HIGH;
  bool stableState = HIGH;
  unsigned long lastChange = 0;

  for (;;) {
    // Anti-rebond : l'etat doit rester stable 50 ms avant de compter
    bool reading = digitalRead(BUTTON_PIN);
    if (reading != lastReading) {
      lastChange = millis();
      lastReading = reading;
    }
    if (millis() - lastChange > DEBOUNCE_MS && reading != stableState) {
      stableState = reading;
      if (stableState == LOW) {  // bouton appuye
        sleeping = !sleeping;
        sleepStart = millis();
        Serial.println(sleeping ? "Bouton : écran éteint pour 5 h" : "Bouton : écran rallumé");
      }
    }

    if (sleeping && millis() - sleepStart >= SLEEP_DURATION_MS) {
      sleeping = false;
    }

    displaySetBrightness(sleeping ? 0 : brightness);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void startBacklightTask() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  xTaskCreatePinnedToCore(backlightTask, "backlight", 3072, nullptr, 1, nullptr, 0);
}

// ---------------------------------------------------------------------------
// Sauvegarde du dernier message dans la flash (LittleFS)
// ---------------------------------------------------------------------------

void saveText(const String& text) {
  LittleFS.remove(LAST_IMAGE_FILE);
  File f = LittleFS.open(LAST_TEXT_FILE, "w");
  if (f) {
    f.print(text);
    f.close();
  }
}

void saveImage(const uint8_t* jpeg, size_t len, const String& caption) {
  File f = LittleFS.open(LAST_IMAGE_FILE, "w");
  if (f) {
    f.write(jpeg, len);
    f.close();
  }
  // Pour une photo, le fichier texte contient la legende
  f = LittleFS.open(LAST_TEXT_FILE, "w");
  if (f) {
    f.print(caption);
    f.close();
  }
}

void clearSaved() {
  LittleFS.remove(LAST_IMAGE_FILE);
  LittleFS.remove(LAST_TEXT_FILE);
}

String readSavedText() {
  File f = LittleFS.open(LAST_TEXT_FILE, "r");
  if (!f) return "";
  String text = f.readString();
  f.close();
  return text;
}

// Reaffiche le dernier message sauvegarde (ou l'ecran d'attente)
void showSaved() {
  File f = LittleFS.open(LAST_IMAGE_FILE, "r");
  if (f) {
    size_t len = f.size();
    uint8_t* jpeg = (uint8_t*)malloc(len);
    bool ok = false;
    if (jpeg) {
      f.read(jpeg, len);
      ok = displayJpeg(jpeg, len, readSavedText());
      free(jpeg);
    }
    f.close();
    if (ok) return;
  }

  String text = readSavedText();
  if (text.length() > 0) {
    displayText(text);
  } else {
    displayWaiting();
  }
}

// ---------------------------------------------------------------------------
// Messages Telegram
// ---------------------------------------------------------------------------

const char* HELP_TEXT =
  "Envoie-moi une photo ou un texte et je l'affiche sur l'écran ❤️\n\n"
  "• Photo : envoie-la normalement (pas comme fichier). "
  "La légende s'affiche en bas de la photo.\n"
  "• Texte : la taille s'ajuste toute seule.\n"
  "• /effacer : vide l'écran\n"
  "• /luminosite 50 : luminosité de 5 à 100 %\n"
  "• /etat : savoir si l'écran est allumé\n\n"
  "Le bouton sur l'écran l'éteint pour 5 h. Un autre clic le rallume.";

// Reponse apres avoir affiche un message
String shownReply() {
  if (sleeping) return "✅ Affiché! " + sleepStatus();
  return "✅ Affiché!";
}

void handleBrightnessCommand(const TelegramMessage& msg) {
  int space = msg.text.indexOf(' ');
  if (space == -1) {
    telegramSendMessage(msg.chatId, "Luminosité : " + String(brightness) + " %\n\nPour changer : /luminosite 50");
    return;
  }

  int value = msg.text.substring(space + 1).toInt();
  if (value < 5 || value > 100) {
    telegramSendMessage(msg.chatId, "Choisis une valeur entre 5 et 100.");
    return;
  }

  brightness = value;
  saveSettings();
  String reply = "Luminosité réglée à " + String(value) + " %.";
  if (sleeping) reply += " " + sleepStatus();
  telegramSendMessage(msg.chatId, reply);
}

// Choisit la taille de photo qui va paraitre la plus grosse a l'ecran,
// sans depasser la memoire disponible
const PhotoSize* pickPhotoSize(const std::vector<PhotoSize>& sizes) {
  const PhotoSize* best = nullptr;
  uint32_t bestArea = 0;
  for (const PhotoSize& s : sizes) {
    if (s.fileSize > MAX_JPEG_SIZE) continue;
    uint32_t area = displayedArea(s.width, s.height);
    if (area > bestArea || (area == bestArea && best && s.fileSize < best->fileSize)) {
      best = &s;
      bestArea = area;
    }
  }
  return best;
}

void showPhoto(const TelegramMessage& msg) {
  const PhotoSize* size = pickPhotoSize(msg.photo);
  if (!size) {
    telegramSendMessage(msg.chatId, "Cette photo est trop grosse pour moi 😕");
    return;
  }
  Serial.printf("Photo %ux%u (%u octets)\n", size->width, size->height, size->fileSize);

  size_t len = 0;
  uint8_t* jpeg = telegramDownloadFile(size->fileId, MAX_JPEG_SIZE, len);
  if (!jpeg) {
    telegramSendMessage(msg.chatId, "J'ai pas réussi à télécharger la photo. Réessaie!");
    return;
  }

  if (displayJpeg(jpeg, len, msg.text)) {
    saveImage(jpeg, len, msg.text);
    telegramSendMessage(msg.chatId, shownReply());
  } else {
    telegramSendMessage(msg.chatId, "J'ai pas réussi à afficher cette image 😕");
    showSaved();  // on remet ce qu'il y avait avant
  }
  free(jpeg);
}

void handleMessage(const TelegramMessage& msg) {
  // Mode installation : on dit a la personne son ID
  if (OWNER_ID == 0) {
    String id = String(msg.fromId);
    telegramSendMessage(msg.chatId,
      "Salut! Ton ID Telegram est " + id + "\n\n"
      "Mets-le dans secrets.h (OWNER_ID) et re-téléverse le code.");
    displayStatus("Ton ID Telegram", id + "\n\nMets-le dans secrets.h (OWNER_ID) et re-téléverse.");
    return;
  }

  // N'importe qui peut trouver un bot Telegram : on ignore les inconnus
  if (msg.fromId != (int64_t)OWNER_ID) {
    Serial.printf("Message ignoré (ID %lld)\n", msg.fromId);
    return;
  }

  if (!msg.photo.empty()) {
    showPhoto(msg);
  } else if (msg.isDocument) {
    telegramSendMessage(msg.chatId, "Envoie l'image comme photo, pas comme fichier 🙂");
  } else if (msg.isOther) {
    telegramSendMessage(msg.chatId, "Je peux juste afficher des photos et du texte 🙂");
  } else if (msg.text == "/start" || msg.text == "/aide") {
    telegramSendMessage(msg.chatId, HELP_TEXT);
  } else if (msg.text.startsWith("/lumin")) {
    handleBrightnessCommand(msg);
  } else if (msg.text == "/etat") {
    telegramSendMessage(msg.chatId, sleepStatus());
  } else if (msg.text == "/effacer") {
    clearSaved();
    displayWaiting();
    telegramSendMessage(msg.chatId, "Écran effacé.");
  } else {
    displayText(msg.text);
    saveText(msg.text);
    telegramSendMessage(msg.chatId, shownReply());
  }
}

// ---------------------------------------------------------------------------
// WiFi et heure
// ---------------------------------------------------------------------------

void connectWifi() {
  displayStatus("Connexion WiFi...", "");

  WiFiManager wm;
  // Appele si le WiFi sauvegarde marche pas (ou au premier demarrage)
  wm.setAPCallback([](WiFiManager*) {
    displayStatus("Configuration WiFi",
      "1. Sur ton téléphone, connecte-toi au WiFi \"" WIFI_SETUP_NAME "\"\n"
      "2. Dans la page qui s'ouvre, choisis ton WiFi et entre le mot de passe");
  });
  wm.setConfigPortalTimeout(300);  // apres 5 min sans config, on redemarre

  if (!wm.autoConnect(WIFI_SETUP_NAME)) {
    ESP.restart();
  }
  Serial.println("WiFi connecté : " + WiFi.localIP().toString());
}

// L'heure est necessaire pour verifier le certificat HTTPS de Telegram
void syncClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  unsigned long start = millis();
  while (time(nullptr) < 1700000000 && millis() - start < 15000) {
    delay(200);
  }
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  loadSettings();
  displayBegin();
  startBacklightTask();

  if (!LittleFS.begin(true)) {
    Serial.println("Erreur LittleFS");
  }

  connectWifi();
  syncClock();
  telegramBegin(BOT_TOKEN);

  if (OWNER_ID == 0) {
    displayStatus("Installation", "Envoie un message à ton bot sur Telegram pour avoir ton ID.");
  } else {
    showSaved();
  }
}

void loop() {
  TelegramMessage msg;
  if (telegramPoll(msg)) {
    handleMessage(msg);
  } else if (telegramErrorCount() > 0) {
    // Internet coupe? On attend un peu. Apres ~3 minutes d'erreurs,
    // on redemarre pour repartir a neuf.
    if (telegramErrorCount() >= 30) ESP.restart();
    delay(5000);
  }
}
