# Remote Text Display (ESP32)

Un écran TFT 2.8" contrôlé à distance. L'ESP32 héberge une page web : tu l'ouvres sur ton téléphone, tu écris un texte, et il s'affiche sur l'écran.

## Matériel

| Composant | Détails |
|---|---|
| Microcontrôleur | ESP32-DevKitC (ESP32-WROOM-32) |
| Écran | TFT 2.8" 240x320, contrôleur **ILI9341**, SPI, 14 pins (avec tactile XPT2046) |

## Branchement

Le schéma complet est dans [`hardware/`](hardware/) (KiCad 10) ou en PDF dans [`docs/schema.pdf`](docs/schema.pdf).

| Pin écran | Fonction | ESP32 |
|---|---|---|
| 1 | VCC | 3V3 |
| 2 | GND | GND |
| 3 | CS | GPIO 5 |
| 4 | RESET | GPIO 17 |
| 5 | DC | GPIO 16 |
| 6 | SDI (MOSI) | GPIO 23 |
| 7 | SCK | GPIO 18 |
| 8 | LED (rétroéclairage) | 3V3 |
| 9 | SDO (MISO) | GPIO 19 |
| 10 | T_CLK | GPIO 18 (partagé avec SCK) |
| 11 | T_CS | GPIO 21 |
| 12 | T_DIN | GPIO 23 (partagé avec MOSI) |
| 13 | T_DO | GPIO 19 (partagé avec MISO) |
| 14 | T_IRQ | GPIO 34 |

L'écran et le tactile partagent le même bus SPI (VSPI de l'ESP32). Chacun a son propre CS.

## Firmware

Le code est dans [`firmware/remotedisplay/`](firmware/remotedisplay/).

### Librairies (Arduino IDE → Library Manager)
- **Adafruit ILI9341**
- **Adafruit GFX Library**

Le core ESP32 d'Espressif doit être installé (Board : *ESP32 Dev Module*).

### Configuration du WiFi
1. Copie `firmware/remotedisplay/secrets.example.h` en `secrets.h` dans le même dossier.
2. Mets le nom et le mot de passe de ton WiFi dedans.

`secrets.h` est dans le `.gitignore`, donc ton mot de passe se ramasse pas sur GitHub.

### Utilisation
1. Téléverse le sketch sur l'ESP32.
2. L'adresse IP s'affiche en haut de l'écran (ex. `http://192.168.1.42`).
3. Ouvre cette adresse sur un téléphone ou un ordi connecté au même WiFi.
4. Écris ton texte, choisis la taille et la couleur, puis clique **Afficher**.

Si l'ESP32 trouve pas le WiFi après 15 secondes, il crée son propre réseau :
- Réseau : `RemoteDisplay`
- Mot de passe : `display123`
- Adresse : `http://192.168.4.1`

Tu peux aussi envoyer du texte sans la page web, par exemple depuis un terminal :

```bash
curl "http://192.168.1.42/send?text=Bonjour&size=4&color=jaune"
```

Couleurs disponibles : `blanc`, `jaune`, `vert`, `cyan`, `rouge`, `magenta`, `orange`. Tailles : 1 à 5.
Les accents français (é, è, à, ç, etc.) sont supportés.

## Structure du repo

```
hardware/   Projet KiCad (schéma, PCB)
firmware/   Code Arduino pour l'ESP32
docs/       Schéma en PDF
```

## Idées pour la suite
- Utiliser le tactile (librairie XPT2046_Touchscreen) pour effacer l'écran ou changer de page
- Garder le dernier message en mémoire (Preferences / LittleFS) après un redémarrage
- Faire le PCB dans KiCad
