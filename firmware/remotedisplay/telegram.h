// Communication avec l'API de Telegram (https://core.telegram.org/bots/api)
#pragma once
#include <Arduino.h>
#include <vector>

// Une des tailles d'une photo. Telegram garde chaque photo en plusieurs
// tailles (ex. 90, 320, 800 et 1280 pixels de large).
struct PhotoSize {
  String fileId;
  uint16_t width;
  uint16_t height;
  uint32_t fileSize;
};

struct TelegramMessage {
  int64_t fromId;    // qui a envoye le message
  int64_t chatId;    // ou repondre
  String text;       // texte du message, ou legende de la photo
  std::vector<PhotoSize> photo;  // vide si c'est pas une photo
  bool isDocument;   // fichier envoye "comme fichier" (on le gere pas)
  bool isOther;      // sticker, vocal, video...
};

void telegramBegin(const char* token);

// Attend jusqu'a ~20 secondes un nouveau message.
// Retourne true si un message est arrive.
bool telegramPoll(TelegramMessage& msg);

// Nombre d'erreurs reseau de suite (0 = tout va bien)
int telegramErrorCount();

bool telegramSendMessage(int64_t chatId, const String& text);

// Telecharge un fichier. Retourne un buffer alloue avec malloc
// (a liberer avec free), ou nullptr si erreur ou plus gros que maxSize.
uint8_t* telegramDownloadFile(const String& fileId, size_t maxSize, size_t& len);
