# Remote Display (ESP32 + Telegram)

Un écran 2.8" connecté à internet. Tu envoies une **photo** ou un **texte** à un bot Telegram, de n'importe où, et ça s'affiche sur l'écran.

- Les photos sont réduites et centrées automatiquement. La légende s'affiche en bas.
- La taille du texte s'ajuste toute seule. Les accents et les emojis cœur sont supportés.
- Le dernier message est sauvegardé dans la mémoire flash, donc il revient après une panne de courant.
- Le WiFi se configure avec un téléphone, sans reprogrammer l'ESP32.
- Seul le propriétaire du bot peut envoyer des messages. Les autres sont ignorés.

## Comment ça marche

```
Téléphone (app Telegram)  ──>  serveurs de Telegram  <──  ESP32 (demande les nouveaux messages)  ──>  écran
```

L'ESP32 interroge l'API de Telegram en continu (*long polling*). Quand un message arrive, il télécharge la photo en JPEG, la décode et l'affiche. Aucun serveur à héberger : Telegram sert d'intermédiaire.

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

## Installation

### 1. Créer le bot Telegram
1. Dans Telegram, ouvre une conversation avec **@BotFather**.
2. Envoie `/newbot`, puis choisis un nom et un nom d'utilisateur (qui finit par `bot`).
3. BotFather te donne un **token** qui ressemble à `123456789:ABCdef...`. Garde-le secret : quelqu'un qui a ce token peut contrôler ton bot.

### 2. Configurer le code
1. Copie `firmware/remotedisplay/secrets.example.h` en `secrets.h` dans le même dossier.
2. Mets ton token dans `BOT_TOKEN`. Laisse `OWNER_ID` à `0` pour l'instant.

`secrets.h` est dans le `.gitignore`, donc ton token se ramasse pas sur GitHub.

### 3. Librairies (Arduino IDE → Library Manager)
- Adafruit ILI9341
- Adafruit GFX Library
- ArduinoJson (v7)
- TJpg_Decoder
- WiFiManager (par tzapu)

### 4. Téléverser
Dans l'Arduino IDE :
- **Board** : ESP32 Dev Module
- **Partition Scheme** : *No OTA (2MB APP/2MB SPIFFS)*. Avec le schéma par défaut, le programme prend 94 % de la place, ce qui laisse presque rien pour ajouter des fonctions.

### 5. Premier démarrage
1. **WiFi** : l'écran affiche *Configuration WiFi*. Sur ton téléphone, connecte-toi au WiFi `Ecran-Config`. Une page s'ouvre : choisis ton WiFi et entre le mot de passe. L'ESP32 s'en souvient ensuite.
2. **Ton ID** : envoie n'importe quel message à ton bot. Il te répond ton ID Telegram.
3. Mets cet ID dans `OWNER_ID` dans `secrets.h`, puis re-téléverse.

C'est prêt! Le WiFi est déjà sauvegardé, pas besoin de le reconfigurer.

## Utilisation

| Tu envoies au bot | Résultat |
|---|---|
| Une photo | Affichée sur l'écran (avec la légende, s'il y en a une) |
| Un texte | Affiché en gros, centré |
| `/effacer` | L'écran revient à l'écran d'attente |
| `/aide` | Le bot explique comment l'utiliser |

Le bot répond « ✅ Affiché! » quand c'est fait.

Si l'écran est déménagé ailleurs, il crée tout seul le réseau `Ecran-Config` quand il trouve pas son WiFi. Il suffit de refaire l'étape 1 du premier démarrage.

## Structure du repo

```
hardware/                 Projet KiCad (schéma, PCB)
docs/                     Schéma en PDF
firmware/remotedisplay/
  remotedisplay.ino       Programme principal (WiFi, gestion des messages, sauvegarde)
  display.cpp/.h          Dessin sur l'écran (texte, photos)
  telegram.cpp/.h         Communication avec l'API de Telegram
  certs.h                 Certificat HTTPS de Telegram
  secrets.example.h       Modèle pour secrets.h
```

## Limites
- Les photos sont affichées en 320x240 au maximum. Les photos en portrait sont plus petites, parce que l'écran est en paysage. Pour un cadre en portrait, mets `SCREEN_ROTATION` à `0` dans `display.cpp`.
- Les GIF, les vidéos et les stickers sont pas supportés.
- Le certificat racine de Telegram (Go Daddy G2) expire en 2037. Si Telegram change de certificat avant, il faudra mettre à jour `certs.h`.

## Idées pour la suite
- Utiliser le tactile pour revoir les anciens messages
- Faire le PCB et un boîtier imprimé en 3D
