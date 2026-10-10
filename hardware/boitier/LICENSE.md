# Licence du boîtier

Les fichiers STL de ce dossier proviennent du modèle
**[Display for ESP32 with ILI9341](https://www.printables.com/model/1205379-display-for-esp32-with-ili9341)**,
conçu par **[@NachiS](https://www.printables.com/@NachiS)** sur Printables.

Ils sont distribués sous la licence
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)](https://creativecommons.org/licenses/by-nc-sa/4.0/).

- **Attribution** : le crédit revient à @NachiS.
- **Pas d'utilisation commerciale.**
- **Partage dans les mêmes conditions** : toute version modifiée doit être partagée sous la même licence.

## Modifications

Les fichiers `display-front.stl`, `display-back.stl`, `display-ili9341-pin-block.stl` et `display-light-bleed-blocker.stl` sont identiques à ceux publiés par l'auteur.

Fichiers modifiés ou ajoutés dans ce repo (aussi sous CC BY-NC-SA 4.0) :

- `display-back-bouton.stl` : version modifiée de `display-back.stl`. Ajout d'un trou de 6.4 mm dans le mur du haut (26 mm à gauche du centre) et, derrière ce mur, d'un support pour un bouton poussoir TL1105 (canal en U avec deux rainures et un renfort à 45°).
- `bouton-piston.stl` et `bouton-porte-switch.stl` : nouvelles pièces pour le bouton.
- `bouton.py` : script qui génère ces trois fichiers à partir de `display-back.stl`.

Cette licence s'applique seulement aux fichiers du boîtier, pas au reste du projet.
