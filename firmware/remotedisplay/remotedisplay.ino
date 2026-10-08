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

// Fuseau horaire du Quebec (heure normale de l'Est, avec l'heure avancee)
#define TIMEZONE "EST5EDT,M3.2.0,M11.1.0"

// ---------------------------------------------------------------------------
// Mode nuit : l'ecran s'eteint entre nightStart et nightEnd.
// Les reglages sont sauvegardes dans la flash et changeables par Telegram.
// ---------------------------------------------------------------------------

Preferences prefs;
bool nightEnabled = true;
uint8_t nightStart = 23;     // heure ou l'ecran s'eteint
uint8_t nightEnd = 7;        // heure ou il se rallume
uint8_t dayBrightness = 100; // luminosite le jour, en %

void loadSettings() {
  prefs.begin("ecran", false);
  nightEnabled = prefs.getBool("nightOn", true);
  nightStart = prefs.getUChar("nightStart", 23);
  nightEnd = prefs.getUChar("nightEnd", 7);
  dayBrightness = prefs.getUChar("brightness", 100);
}

void saveSettings() {
  prefs.putBool("nightOn", nightEnabled);
  prefs.putUChar("nightStart", nightStart);
  prefs.putUChar("nightEnd", nightEnd);
  prefs.putUChar("brightness", dayBrightness);
}

bool isNight() {
  if (!nightEnabled || nightStart == nightEnd) return false;

  time_t now = time(nullptr);
  if (now < 1700000000) return false;  // heure pas encore synchronisee
  struct tm local;
  localtime_r(&now, &local);
  int h = local.tm_hour;

  // Ex. 23 h a 7 h : la periode passe minuit
  if (nightStart > nightEnd) return h >= nightStart || h < nightEnd;
  return h >= nightStart && h < nightEnd;
}

void updateBacklight() {
  displaySetBrightness(isNight() ? 0 : dayBrightness);
}

String nightStatus() {
  if (!nightEnabled) return "Mode nuit désactivé.";
  return "Mode nuit : écran éteint de " + String(nightStart) + " h à " + String(nightEnd) + " h.";
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
  "• /nuit 23 7 : écran éteint de 23 h à 7 h\n"
  "• /nuit off : désactive le mode nuit\n"
  "• /nuit : voir les réglages";

// Reponse apres avoir affiche un message
String shownReply() {
  if (isNight()) {
    return "✅ Affiché! L'écran est en mode nuit, ça va être visible à " + String(nightEnd) + " h.";
  }
  return "✅ Affiché!";
}

void handleBrightnessCommand(const TelegramMessage& msg) {
  int space = msg.text.indexOf(' ');
  if (space == -1) {
    telegramSendMessage(msg.chatId, "Luminosité : " + String(dayBrightness) + " %\n\nPour changer : /luminosite 50");
    return;
  }

  int value = msg.text.substring(space + 1).toInt();
  if (value < 5 || value > 100) {
    telegramSendMessage(msg.chatId, "Choisis une valeur entre 5 et 100.");
    return;
  }

  dayBrightness = value;
  saveSettings();
  updateBacklight();
  String reply = "Luminosité réglée à " + String(dayBrightness) + " %.";
  if (isNight()) reply += " L'écran est en mode nuit pour l'instant.";
  telegramSendMessage(msg.chatId, reply);
}

void handleNightCommand(const TelegramMessage& msg) {
  String arg = msg.text.substring(5);  // apres "/nuit"
  arg.trim();

  if (arg.isEmpty()) {
    telegramSendMessage(msg.chatId, nightStatus() +
      "\n\nPour changer les heures : /nuit 23 7\nPour désactiver : /nuit off");
    return;
  }

  if (arg == "off") {
    nightEnabled = false;
  } else if (arg == "on") {
    nightEnabled = true;
  } else {
    int start, end;
    if (sscanf(arg.c_str(), "%d %d", &start, &end) != 2 ||
        start < 0 || start > 23 || end < 0 || end > 23 || start == end) {
      telegramSendMessage(msg.chatId, "Format : /nuit 23 7 (éteint de 23 h à 7 h)");
      return;
    }
    nightEnabled = true;
    nightStart = start;
    nightEnd = end;
  }

  saveSettings();
  updateBacklight();
  telegramSendMessage(msg.chatId, nightStatus());
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
  } else if (msg.text.startsWith("/nuit")) {
    handleNightCommand(msg);
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

// L'heure sert au mode nuit, et a verifier le certificat HTTPS de Telegram
void syncClock() {
  configTzTime(TIMEZONE, "pool.ntp.org", "time.nist.gov");
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

  if (!LittleFS.begin(true)) {
    Serial.println("Erreur LittleFS");
  }

  connectWifi();
  syncClock();
  updateBacklight();
  telegramBegin(BOT_TOKEN);

  if (OWNER_ID == 0) {
    displayStatus("Installation", "Envoie un message à ton bot sur Telegram pour avoir ton ID.");
  } else {
    showSaved();
  }
}

void loop() {
  updateBacklight();

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
