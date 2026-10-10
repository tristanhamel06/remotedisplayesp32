# Licence du boîtier

Le boîtier est basé sur le modèle
**[Display for ESP32 with ILI9341](https://www.printables.com/model/1205379-display-for-esp32-with-ili9341)**,
conçu par **[@NachiS](https://www.printables.com/@NachiS)** sur Printables.

Il est distribué sous la licence
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)](https://creativecommons.org/licenses/by-nc-sa/4.0/).

- **Attribution** : le crédit du design original revient à @NachiS.
- **Pas d'utilisation commerciale.**
- **Partage dans les mêmes conditions** : toute version modifiée doit être partagée sous la même licence.

## Fichiers originaux

Les fichiers de `original-nachis/` sont identiques à ceux publiés par l'auteur. Ce modèle est fait pour un écran ILI9341 de **2.4"**.

## Modifications

Les fichiers de `a-imprimer/` sont une version modifiée, partagée sous la même licence CC BY-NC-SA 4.0. Ils sont générés par `generer.py` :

- `arriere.stl` : `display-back.stl` élargi de 16 mm (coupé entre le bouton et la poche de l'ESP32, murs et plancher prolongés). Ajout d'un trou de 6.4 mm dans le mur du haut et, derrière ce mur, d'un support pour un bouton poussoir TL1105 (canal en U avec deux rainures et un renfort à 45°).
- `devant.stl` : contour et trous de vis de `display-front.stl`, élargi de 16 mm. L'intérieur a été refait pour un écran **2.8" MSP2807** : nouvelle fenêtre avec biseau, poche pour la vitre et le circuit, et 4 appuis avec des tiges qui passent dans les trous du circuit.
- `bouton-piston.stl` et `bouton-porte-switch.stl` : nouvelles pièces pour le bouton.

`display-ili9341-pin-block.stl` et `display-light-bleed-blocker.stl` ne servent pas dans la version 2.8".

Cette licence s'applique seulement aux fichiers du boîtier, pas au reste du projet.
