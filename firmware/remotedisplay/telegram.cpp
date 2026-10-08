#include "telegram.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "certs.h"

#define API_HOST "https://api.telegram.org"
#define POLL_TIMEOUT_S 20  // "long polling" : Telegram garde la connexion ouverte

static String botToken;
static int64_t nextOffset = 0;  // ID du prochain message a demander
static int errorCount = 0;
static WiFiClientSecure client;

static String apiUrl(const char* method) {
  return String(API_HOST "/bot") + botToken + "/" + method;
}

void telegramBegin(const char* token) {
  botToken = token;
  client.setCACert(TELEGRAM_ROOT_CA);
}

int telegramErrorCount() {
  return errorCount;
}

bool telegramPoll(TelegramMessage& msg) {
  // limit=1 : un message a la fois. offset : confirme a Telegram qu'on a
  // deja traite les messages d'avant, pour pas les recevoir deux fois.
  String url = apiUrl("getUpdates") + "?timeout=" + POLL_TIMEOUT_S +
               "&limit=1&offset=" + String(nextOffset) +
               "&allowed_updates=%5B%22message%22%5D";

  HTTPClient http;
  http.setTimeout((POLL_TIMEOUT_S + 10) * 1000);
  if (!http.begin(client, url)) {
    errorCount++;
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    Serial.printf("getUpdates: erreur HTTP %d\n", code);
    if (code == 401) Serial.println("-> BOT_TOKEN invalide dans secrets.h?");
    http.end();
    errorCount++;
    return false;
  }
  String body = http.getString();
  http.end();

  // On garde juste les champs dont on a besoin (economise la memoire)
  JsonDocument filter;
  JsonObject f = filter["result"][0].to<JsonObject>();
  f["update_id"] = true;
  f["message"]["from"]["id"] = true;
  f["message"]["chat"]["id"] = true;
  f["message"]["text"] = true;
  f["message"]["caption"] = true;
  f["message"]["photo"][0]["file_id"] = true;
  f["message"]["photo"][0]["width"] = true;
  f["message"]["photo"][0]["height"] = true;
  f["message"]["photo"][0]["file_size"] = true;
  f["message"]["document"]["file_id"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("getUpdates: JSON invalide (%s)\n", err.c_str());
    errorCount++;
    return false;
  }
  errorCount = 0;

  JsonArray result = doc["result"];
  if (result.size() == 0) return false;  // pas de nouveau message

  JsonObject update = result[0];
  nextOffset = update["update_id"].as<int64_t>() + 1;

  JsonObject m = update["message"];
  if (m.isNull()) return false;

  msg = TelegramMessage();
  msg.fromId = m["from"]["id"].as<int64_t>();
  msg.chatId = m["chat"]["id"].as<int64_t>();

  const char* text = m["text"];
  if (!text) text = m["caption"];
  msg.text = text ? text : "";

  for (JsonObject p : m["photo"].as<JsonArray>()) {
    PhotoSize size;
    size.fileId = p["file_id"].as<String>();
    size.width = p["width"];
    size.height = p["height"];
    size.fileSize = p["file_size"] | 0;
    msg.photo.push_back(size);
  }

  msg.isDocument = !m["document"].isNull();
  msg.isOther = msg.text.isEmpty() && msg.photo.empty() && !msg.isDocument;
  return true;
}

bool telegramSendMessage(int64_t chatId, const String& text) {
  JsonDocument body;
  body["chat_id"] = chatId;
  body["text"] = text;
  String json;
  serializeJson(body, json);

  HTTPClient http;
  if (!http.begin(client, apiUrl("sendMessage"))) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(json);
  http.end();

  if (code != 200) Serial.printf("sendMessage: erreur HTTP %d\n", code);
  return code == 200;
}

uint8_t* telegramDownloadFile(const String& fileId, size_t maxSize, size_t& len) {
  HTTPClient http;

  // 1. Demander a Telegram ou est le fichier
  if (!http.begin(client, apiUrl("getFile") + "?file_id=" + fileId)) return nullptr;
  if (http.GET() != 200) {
    http.end();
    return nullptr;
  }
  JsonDocument doc;
  deserializeJson(doc, http.getString());
  http.end();

  const char* path = doc["result"]["file_path"];
  if (!path) return nullptr;

  // 2. Telecharger le fichier
  if (!http.begin(client, String(API_HOST "/file/bot") + botToken + "/" + path)) return nullptr;
  if (http.GET() != 200) {
    http.end();
    return nullptr;
  }

  int size = http.getSize();
  if (size <= 0 || (size_t)size > maxSize) {
    Serial.printf("Fichier trop gros ou taille inconnue (%d octets)\n", size);
    http.end();
    return nullptr;
  }

  uint8_t* buffer = (uint8_t*)malloc(size);
  if (!buffer) {
    Serial.println("Pas assez de memoire pour l'image");
    http.end();
    return nullptr;
  }

  auto* stream = http.getStreamPtr();
  size_t received = 0;
  unsigned long lastData = millis();
  while (received < (size_t)size && millis() - lastData < 10000) {
    size_t available = stream->available();
    if (available > 0) {
      int n = stream->read(buffer + received, min(available, (size_t)size - received));
      if (n > 0) {
        received += n;
        lastData = millis();
      }
    } else {
      delay(1);
    }
  }
  http.end();

  if (received != (size_t)size) {
    Serial.printf("Telechargement incomplet (%u / %d octets)\n", received, size);
    free(buffer);
    return nullptr;
  }

  len = received;
  return buffer;
}
