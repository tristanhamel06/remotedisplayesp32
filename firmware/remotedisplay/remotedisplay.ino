// Remote Text Display - ESP32-DevKitC + ecran TFT ILI9341 2.8" (SPI)
//
// L'ESP32 se connecte au WiFi et heberge une petite page web.
// Ouvre l'adresse IP affichee sur l'ecran avec ton telephone,
// ecris ton texte, et il s'affiche sur l'ecran.
//
// Si le WiFi marche pas, l'ESP32 cree son propre reseau
// "RemoteDisplay" (mot de passe : display123), adresse 192.168.4.1.
//
// Branchement : voir README.md ou hardware/esp32display.kicad_sch

#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "secrets.h"

// ----- Pins (selon le schema KiCad) -----
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  17
// SCK = GPIO18, MOSI = GPIO23, MISO = GPIO19 (bus SPI par defaut de l'ESP32)
// Tactile (pas utilise pour l'instant) : T_CS = GPIO21, T_IRQ = GPIO34

#define AP_SSID     "RemoteDisplay"
#define AP_PASSWORD "display123"

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
WebServer server(80);

String currentText = "Pret!";
uint8_t currentSize = 3;
uint16_t currentColor = ILI9341_WHITE;
String ipText;

struct ColorOption {
  const char* name;
  uint16_t value;
};

const ColorOption COLORS[] = {
  {"blanc", ILI9341_WHITE},
  {"jaune", ILI9341_YELLOW},
  {"vert", ILI9341_GREEN},
  {"cyan", ILI9341_CYAN},
  {"rouge", ILI9341_RED},
  {"magenta", ILI9341_MAGENTA},
  {"orange", ILI9341_ORANGE},
};
const int NUM_COLORS = sizeof(COLORS) / sizeof(COLORS[0]);

// La police de base d'Adafruit GFX utilise l'encodage CP437, pas l'UTF-8.
// On convertit les accents francais pour qu'ils s'affichent comme du monde.
String utf8ToCp437(const String& in) {
  String out;
  for (unsigned int i = 0; i < in.length(); i++) {
    uint8_t c = in[i];
    if (c < 0x80) {
      out += (char)c;
      continue;
    }
    if (c == 0xC3 && i + 1 < in.length()) {
      uint8_t d = in[++i];
      char r;
      switch (d) {
        case 0xA9: r = 0x82; break;  // é
        case 0xA8: r = 0x8A; break;  // è
        case 0xAA: r = 0x88; break;  // ê
        case 0xAB: r = 0x89; break;  // ë
        case 0xA0: r = 0x85; break;  // à
        case 0xA2: r = 0x83; break;  // â
        case 0xA7: r = 0x87; break;  // ç
        case 0xAE: r = 0x8C; break;  // î
        case 0xAF: r = 0x8B; break;  // ï
        case 0xB4: r = 0x93; break;  // ô
        case 0xB9: r = 0x97; break;  // ù
        case 0xBB: r = 0x96; break;  // û
        case 0xBC: r = 0x81; break;  // ü
        case 0x89: r = 0x90; break;  // É
        case 0x87: r = 0x80; break;  // Ç
        case 0x80: case 0x82: r = 'A'; break;  // À Â
        case 0x88: case 0x8A: case 0x8B: r = 'E'; break;  // È Ê Ë
        case 0x8E: case 0x8F: r = 'I'; break;  // Î Ï
        case 0x94: r = 'O'; break;  // Ô
        case 0x99: case 0x9B: r = 'U'; break;  // Ù Û
        default: r = '?';
      }
      out += r;
      continue;
    }
    // Autre caractere UTF-8 (emoji, etc.) : on saute les octets de suite
    while (i + 1 < in.length() && (in[i + 1] & 0xC0) == 0x80) i++;
    out += '?';
  }
  return out;
}

// Affiche le texte avec retour a la ligne par mot (pas au milieu d'un mot)
void drawWrappedText(const String& text, uint8_t size, uint16_t color, int16_t top) {
  const int16_t margin = 6;
  const int16_t charW = 6 * size;
  const int16_t lineH = 8 * size + 4;
  const int maxChars = (tft.width() - 2 * margin) / charW;

  tft.setTextSize(size);
  tft.setTextColor(color);
  int16_t y = top;

  int start = 0;
  while (start < (int)text.length() && y + lineH <= tft.height()) {
    int nl = text.indexOf('\n', start);
    int paraEnd = (nl == -1) ? text.length() : nl;

    // Decoupe le paragraphe en lignes qui rentrent dans l'ecran
    int lineStart = start;
    do {
      int lineEnd = paraEnd;
      if (lineEnd - lineStart > maxChars) {
        lineEnd = lineStart + maxChars;
        int space = text.lastIndexOf(' ', lineEnd);
        if (space > lineStart) lineEnd = space;
      }
      tft.setCursor(margin, y);
      tft.print(text.substring(lineStart, lineEnd));
      y += lineH;
      lineStart = lineEnd;
      while (lineStart < paraEnd && text[lineStart] == ' ') lineStart++;
    } while (lineStart < paraEnd && y + lineH <= tft.height());

    start = paraEnd + 1;
  }
}

void drawScreen() {
  tft.fillScreen(ILI9341_BLACK);

  // Barre du haut avec l'adresse IP
  tft.fillRect(0, 0, tft.width(), 14, ILI9341_NAVY);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(4, 3);
  tft.print(ipText);

  drawWrappedText(currentText, currentSize, currentColor, 22);
}

String htmlEscape(const String& s) {
  String out;
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '&') out += "&amp;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

String lastRawText = "Pret!";

void handleRoot() {
  String colorOptions;
  for (int i = 0; i < NUM_COLORS; i++) {
    colorOptions += "<option value=\"" + String(COLORS[i].name) + "\"";
    if (COLORS[i].value == currentColor) colorOptions += " selected";
    colorOptions += ">" + String(COLORS[i].name) + "</option>";
  }
  String sizeOptions;
  for (int s = 1; s <= 5; s++) {
    sizeOptions += "<option value=\"" + String(s) + "\"";
    if (s == currentSize) sizeOptions += " selected";
    sizeOptions += ">" + String(s) + "</option>";
  }

  String page =
    "<!DOCTYPE html><html lang=\"fr\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Remote Display</title><style>"
    "body{font-family:sans-serif;max-width:480px;margin:20px auto;padding:0 16px;background:#111;color:#eee}"
    "textarea,select,button{font-size:18px;width:100%;box-sizing:border-box;margin:6px 0;padding:10px;border-radius:8px;border:1px solid #444;background:#222;color:#eee}"
    "textarea{height:140px}button{background:#2563eb;border:none;color:white;font-weight:bold}"
    ".row{display:flex;gap:10px}.row label{flex:1}"
    "</style></head><body><h2>Remote Text Display</h2>"
    "<form method=\"POST\" action=\"/send\">"
    "<textarea name=\"text\" placeholder=\"Ecris ton texte ici...\">" + htmlEscape(lastRawText) + "</textarea>"
    "<div class=\"row\"><label>Taille<select name=\"size\">" + sizeOptions + "</select></label>"
    "<label>Couleur<select name=\"color\">" + colorOptions + "</select></label></div>"
    "<button type=\"submit\">Afficher</button></form>"
    "<form method=\"POST\" action=\"/clear\"><button style=\"background:#555\">Effacer</button></form>"
    "</body></html>";

  server.send(200, "text/html; charset=utf-8", page);
}

// Accepte POST (formulaire) ou GET, ex: /send?text=Bonjour&size=4&color=jaune
void handleSend() {
  if (server.hasArg("text")) {
    lastRawText = server.arg("text");
    lastRawText.replace("\r", "");
    currentText = utf8ToCp437(lastRawText);
  }
  if (server.hasArg("size")) {
    currentSize = constrain(server.arg("size").toInt(), 1, 5);
  }
  if (server.hasArg("color")) {
    String c = server.arg("color");
    for (int i = 0; i < NUM_COLORS; i++) {
      if (c == COLORS[i].name) currentColor = COLORS[i].value;
    }
  }
  drawScreen();
  Serial.println("Nouveau texte : " + lastRawText);

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleClear() {
  lastRawText = "";
  currentText = "";
  drawScreen();
  server.sendHeader("Location", "/");
  server.send(303);
}

void startWifi() {
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(6, 6);
  tft.print("Connexion WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    ipText = "http://" + WiFi.localIP().toString();
    Serial.println("Connecte! Ouvre " + ipText);
  } else {
    // Plan B : l'ESP32 cree son propre WiFi
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    ipText = "WiFi " AP_SSID " -> http://" + WiFi.softAPIP().toString();
    Serial.println("WiFi pas trouve. Point d'acces : " + ipText);
  }
}

void setup() {
  Serial.begin(115200);

  tft.begin();
  tft.setRotation(1);  // paysage 320x240
  tft.cp437(true);
  tft.fillScreen(ILI9341_BLACK);

  startWifi();

  server.on("/", handleRoot);
  server.on("/send", handleSend);
  server.on("/clear", HTTP_POST, handleClear);
  server.begin();

  drawScreen();
}

void loop() {
  server.handleClient();
}
