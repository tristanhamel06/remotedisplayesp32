#include "display.h"
#include <SPI.h>
#include <vector>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <TJpg_Decoder.h>

// ----- Pins (selon le schema KiCad) -----
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  17
// SCK = GPIO18, MOSI = GPIO23, MISO = GPIO19 (bus SPI par defaut de l'ESP32)

// 1 = paysage (320x240). Mets 0 si ton cadre est en portrait (240x320).
#define SCREEN_ROTATION 1

#define MARGIN 8
#define COLOR_BG    ILI9341_BLACK
#define COLOR_TEXT  ILI9341_WHITE
#define COLOR_PINK  0xFB56  // rose (255, 105, 180)

// Caracteres speciaux de la police CP437 d'Adafruit GFX
#define CHAR_SMILEY 0x01
#define CHAR_HEART  0x03

static Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// ---------------------------------------------------------------------------
// Conversion UTF-8 -> CP437
// La police de base d'Adafruit GFX utilise l'encodage CP437. On convertit les
// accents francais, et les emojis coeur deviennent le petit coeur de la police.
// ---------------------------------------------------------------------------

static bool isHeartEmoji(uint32_t cp) {
  return cp == 0x2764 || cp == 0x2665 ||                // ❤ ♥
         (cp >= 0x1F493 && cp <= 0x1F49F) ||            // 💓 à 💟
         cp == 0x1F9E1 || cp == 0x1F90D || cp == 0x1F5A4 || cp == 0x1F90E ||
         cp == 0x1F60D || cp == 0x1F618 || cp == 0x1F970 || cp == 0x1F63B;  // 😍 😘 🥰 😻
}

static bool isSmileyEmoji(uint32_t cp) {
  return cp == 0x263A || (cp >= 0x1F600 && cp <= 0x1F606) ||
         cp == 0x1F60A || cp == 0x1F642 || cp == 0x1F917;
}

static void appendCodepoint(String& out, uint32_t cp) {
  if (cp == '\r') return;
  if (cp < 0x80) { out += (char)cp; return; }
  if (isHeartEmoji(cp)) { out += (char)CHAR_HEART; return; }
  if (isSmileyEmoji(cp)) { out += (char)CHAR_SMILEY; return; }

  switch (cp) {
    case 0xE9: out += (char)0x82; return;  // é
    case 0xE8: out += (char)0x8A; return;  // è
    case 0xEA: out += (char)0x88; return;  // ê
    case 0xEB: out += (char)0x89; return;  // ë
    case 0xE0: out += (char)0x85; return;  // à
    case 0xE2: out += (char)0x83; return;  // â
    case 0xE4: out += (char)0x84; return;  // ä
    case 0xE7: out += (char)0x87; return;  // ç
    case 0xEE: out += (char)0x8C; return;  // î
    case 0xEF: out += (char)0x8B; return;  // ï
    case 0xF4: out += (char)0x93; return;  // ô
    case 0xF6: out += (char)0x94; return;  // ö
    case 0xF9: out += (char)0x97; return;  // ù
    case 0xFB: out += (char)0x96; return;  // û
    case 0xFC: out += (char)0x81; return;  // ü
    case 0xF1: out += (char)0xA4; return;  // ñ
    case 0xC9: out += (char)0x90; return;  // É
    case 0xC7: out += (char)0x80; return;  // Ç
    case 0xAB: out += (char)0xAE; return;  // «
    case 0xBB: out += (char)0xAF; return;  // »
    case 0xB0: out += (char)0xF8; return;  // °
    case 0xC0: case 0xC2: out += 'A'; return;              // À Â
    case 0xC8: case 0xCA: case 0xCB: out += 'E'; return;   // È Ê Ë
    case 0xCE: case 0xCF: out += 'I'; return;              // Î Ï
    case 0xD4: out += 'O'; return;                         // Ô
    case 0xD9: case 0xDB: out += 'U'; return;              // Ù Û
    case 0x2018: case 0x2019: out += '\''; return;         // ‘ ’ (iPhone)
    case 0x201C: case 0x201D: out += '"'; return;          // “ ”
    case 0x2013: case 0x2014: out += '-'; return;          // – —
    case 0x2026: out += "..."; return;                     // …
    case 0x2B50: case 0x2605: out += '*'; return;          // ⭐ ★
    case 0x00A0: out += ' '; return;                       // espace insecable
  }
  // Tout le reste (autres emojis, FE0F, ZWJ...) est ignore
}

static String utf8ToCp437(const String& in) {
  String out;
  size_t i = 0;
  while (i < in.length()) {
    uint8_t c = in[i];
    uint32_t cp;
    int extra;
    if (c < 0x80)              { cp = c;        extra = 0; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { i++; continue; }  // octet invalide

    i++;
    for (int k = 0; k < extra && i < in.length(); k++, i++) {
      cp = (cp << 6) | (in[i] & 0x3F);
    }
    appendCodepoint(out, cp);
  }
  return out;
}

// ---------------------------------------------------------------------------
// Texte
// ---------------------------------------------------------------------------

// Coupe le texte en lignes de maxChars caracteres max, sans couper les mots
static std::vector<String> wrapLines(const String& text, int maxChars) {
  std::vector<String> lines;
  int start = 0;
  int n = text.length();

  while (true) {
    int nl = text.indexOf('\n', start);
    int paraEnd = (nl == -1) ? n : nl;

    if (start == paraEnd) lines.push_back("");  // ligne vide voulue

    int lineStart = start;
    while (lineStart < paraEnd) {
      int lineEnd = paraEnd;
      if (lineEnd - lineStart > maxChars) {
        lineEnd = lineStart + maxChars;
        int space = text.lastIndexOf(' ', lineEnd);
        if (space > lineStart) lineEnd = space;
      }
      lines.push_back(text.substring(lineStart, lineEnd));
      lineStart = lineEnd;
      while (lineStart < paraEnd && text[lineStart] == ' ') lineStart++;
    }

    if (nl == -1) break;
    start = nl + 1;
  }
  return lines;
}

static int lineHeight(uint8_t size) {
  return 8 * size + 2 * size;
}

// Dessine des lignes centrees. Les coeurs sont en rose.
static void drawCenteredLines(const std::vector<String>& lines, uint8_t size,
                              int16_t top, uint16_t color) {
  int16_t y = top;
  for (const String& line : lines) {
    int16_t x = (tft.width() - (int)line.length() * 6 * size) / 2;
    for (size_t i = 0; i < line.length(); i++) {
      uint8_t c = line[i];
      uint16_t col = (c == CHAR_HEART) ? COLOR_PINK : color;
      tft.drawChar(x, y, c, col, COLOR_BG, size);
      x += 6 * size;
    }
    y += lineHeight(size);
  }
}

void displayText(const String& utf8Text) {
  String text = utf8ToCp437(utf8Text);
  const int availW = tft.width() - 2 * MARGIN;
  const int availH = tft.height() - 2 * MARGIN;

  // On essaie la plus grosse taille d'abord, jusqu'a ce que ca rentre
  uint8_t size = 1;
  std::vector<String> lines;
  for (uint8_t s = 5; s >= 1; s--) {
    lines = wrapLines(text, availW / (6 * s));
    int totalH = lines.size() * lineHeight(s) - 2 * s;
    size = s;
    if (totalH <= availH) break;
  }

  // Meme en petit ca rentre pas : on coupe la fin
  int maxLines = (availH + 2 * size) / lineHeight(size);
  if ((int)lines.size() > maxLines) lines.resize(maxLines);

  int totalH = lines.size() * lineHeight(size) - 2 * size;
  tft.fillScreen(COLOR_BG);
  drawCenteredLines(lines, size, (tft.height() - totalH) / 2, COLOR_TEXT);
}

void displayStatus(const char* title, const String& body) {
  tft.fillScreen(COLOR_BG);
  tft.setTextWrap(false);

  tft.setTextSize(2);
  tft.setTextColor(COLOR_PINK);
  tft.setCursor(MARGIN, MARGIN);
  tft.print(utf8ToCp437(title));

  tft.setTextColor(COLOR_TEXT);
  int16_t y = MARGIN + 30;
  for (const String& line : wrapLines(utf8ToCp437(body), (tft.width() - 2 * MARGIN) / 12)) {
    tft.setCursor(MARGIN, y);
    tft.print(line);
    y += lineHeight(2);
  }
}

void displayWaiting() {
  tft.fillScreen(COLOR_BG);

  // Un gros coeur : deux cercles + un triangle
  int16_t cx = tft.width() / 2;
  int16_t cy = tft.height() / 2 - 30;
  int16_t r = 26;
  tft.fillCircle(cx - r, cy, r, COLOR_PINK);
  tft.fillCircle(cx + r, cy, r, COLOR_PINK);
  tft.fillTriangle(cx - 2 * r, cy + 4, cx + 2 * r, cy + 4, cx, cy + 2 * r + 12, COLOR_PINK);

  std::vector<String> lines = {"En attente d'un message..."};
  drawCenteredLines(lines, 2, tft.height() - 50, COLOR_TEXT);
}

// ---------------------------------------------------------------------------
// Images JPEG
// ---------------------------------------------------------------------------

// TJpg_Decoder decode l'image par petits blocs et appelle cette fonction
// pour chaque bloc a dessiner
static bool jpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;  // le reste est en dehors de l'ecran
  tft.drawRGBBitmap(x, y, bitmap, w, h);
  return true;
}

// Plus petite reduction (1, 2, 4 ou 8) pour que l'image rentre dans l'ecran
static uint8_t pickScale(uint16_t w, uint16_t h) {
  for (uint8_t s = 1; s <= 8; s *= 2) {
    if (w / s <= tft.width() && h / s <= tft.height()) return s;
  }
  return 0;
}

uint32_t displayedArea(uint16_t w, uint16_t h) {
  uint8_t s = pickScale(w, h);
  return s ? (uint32_t)(w / s) * (h / s) : 0;
}

static void drawCaption(const String& utf8Caption) {
  String caption = utf8ToCp437(utf8Caption);

  // Taille 2 si ca rentre en 2 lignes, sinon taille 1 (max 4 lignes)
  uint8_t size = 2;
  std::vector<String> lines = wrapLines(caption, (tft.width() - 2 * MARGIN) / 12);
  if (lines.size() > 2) {
    size = 1;
    lines = wrapLines(caption, (tft.width() - 2 * MARGIN) / 6);
    if (lines.size() > 4) lines.resize(4);
  }

  int16_t bandH = lines.size() * lineHeight(size) + 6;
  int16_t top = tft.height() - bandH;
  tft.fillRect(0, top, tft.width(), bandH, COLOR_BG);
  drawCenteredLines(lines, size, top + 4, COLOR_TEXT);
}

bool displayJpeg(const uint8_t* jpeg, size_t len, const String& caption) {
  uint16_t w, h;
  if (TJpgDec.getJpgSize(&w, &h, jpeg, len) != JDR_OK) return false;

  uint8_t scale = pickScale(w, h);
  if (scale == 0) return false;

  tft.fillScreen(COLOR_BG);
  TJpgDec.setJpgScale(scale);
  int16_t x = (tft.width() - w / scale) / 2;
  int16_t y = (tft.height() - h / scale) / 2;
  if (TJpgDec.drawJpg(x, y, jpeg, len) != JDR_OK) return false;

  if (caption.length() > 0) drawCaption(caption);
  return true;
}

void displayBegin() {
  tft.begin();
  tft.setRotation(SCREEN_ROTATION);
  tft.cp437(true);
  tft.fillScreen(COLOR_BG);

  TJpgDec.setCallback(jpegOutput);
}
