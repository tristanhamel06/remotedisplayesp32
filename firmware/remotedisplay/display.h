// Tout ce qui dessine sur l'ecran ILI9341.
#pragma once
#include <Arduino.h>

void displayBegin();

// Ecran d'info (connexion, configuration WiFi, erreurs...)
void displayStatus(const char* title, const String& body);

// Ecran d'attente quand y'a encore aucun message
void displayWaiting();

// Affiche un texte (UTF-8) centre, avec la plus grosse taille qui rentre
void displayText(const String& utf8Text);

// Affiche une photo JPEG centree, avec une legende optionnelle en bas.
// Retourne false si l'image peut pas etre decodee.
bool displayJpeg(const uint8_t* jpeg, size_t len, const String& caption);

// Surface (en pixels) qu'une image w x h va occuper une fois reduite pour
// rentrer dans l'ecran. 0 si elle est trop grosse meme reduite 8 fois.
// Sert a choisir la meilleure taille parmi celles que Telegram offre.
uint32_t displayedArea(uint16_t w, uint16_t h);
