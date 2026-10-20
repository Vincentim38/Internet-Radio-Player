# InternetRadioPlayer

Ce dépôt contient [les sources](./RadioWifi.ino) pour faire fonctionner un lecteur de radios internet, ainsi que les [fichiers 3D](./RadioWifi.step) permettant de construire une boite contenant le board et son écran, la batterie et le haut-parleur.

> [!WARNING]
> _Attention_
Vérifier que les règlages dans l'onglet 'Outils' d'Arduino IDE sont bien semblables à [ceux-ci](https://github.com/Vincentim38/InternetRadioPlayer/blob/main/ArduinoIDETools.png).

## Board
Le board est un Freenove FNK0104B, basé sur une ESP-S3 avec un écran 2.8" (https://store.freenove.com/products/fnk0104?variant=45040270213318) dont la doc complète est ici : https://github.com/Freenove/Freenove_ESP32_S3_Display. Un haut-parleur est inclus dans le kit.

<img width="490" height="488" alt="BoardESP32-S3" src="https://github.com/user-attachments/assets/f4cc6e01-d229-4323-8f41-2e88f5e93d85" />

Une autre doc est disponible sur LCDwiki : https://www.lcdwiki.com/2.8inch_ESP32-S3_Display

Le pinout est dispo [ici](https://github.com/Vincentim38/InternetRadioPlayer/blob/main/PinOut%20Freenove%20Freenove%20FNK0104B) dans ce dépôt.
 


## Batterie
Vous pouvez utiliser ce type de batterie : https://www.amazon.fr/dp/B0H2YNVYJ4.

> [!WARNING]
> Attention à choisir une batterie avec un connecteur JST 1.25mm 2Pin.

<img width="430" height="366" alt="Battery" src="https://github.com/user-attachments/assets/6691fd67-dc1f-427b-8fcb-e9dfa019fda2" />

## Boitier
Le boitier est de ma conception aussi, il resssemble à ça : 

<img width="479" height="378" alt="RadioWifiRendu1" src="https://github.com/user-attachments/assets/d1c2e937-7415-41f4-a480-ca1be89848d7" />

<img width="359" height="385" alt="RadioWifiRendu2" src="https://github.com/user-attachments/assets/c91da9df-b7d3-4e35-9750-fcb5de20c011" />


Après avoir imprimé le boitier, il ne reste plus qu'à placer à chaud 4 inserts taraudés en M3 de 4mm le long aux 4 coins de l'écran.



---

> [!NOTE]
> _English version_

This repository contains the source code for running an internet radio player, as well as 3D files for an enclosure to house the board and its display, the battery, and the speaker. ## Board
The board is a Freenove FNK0104B, based on an ESP32-S3 with a 2.8" display (https://store.freenove.com/products/fnk0104?variant=45040270213318). Full documentation can be found here: https://github.com/Freenove/Freenove_ESP32_S3_Display. A speaker is included in the kit.

<img width="490" height="488" alt="BoardESP32-S3" src="https://github.com/user-attachments/assets/f4cc6e01-d229-4323-8f41-2e88f5e93d85" />

Additional documentation is available on LCDwiki: https://www.lcdwiki.com/2.8inch_ESP32-S3_Display



## Battery
You can use this type of battery: https://www.amazon.fr/dp/B0H2YNVYJ4.
> [!WARNING]
> Make sure to choose a battery with a 2-pin 1.25mm JST connector.

<img width="430" height="366" alt="Battery" src="https://github.com/user-attachments/assets/6691fd67-dc1f-427b-8fcb-e9dfa019fda2" />

## Enclosure
The enclosure is also of my own design; it looks like this:

<img width="479" height="378" alt="RadioWifiRendu1" src="https://github.com/user-attachments/assets/d1c2e937-7415-41f4-a480-ca1be89848d7" />

<img width="359" height="385" alt="RadioWifiRendu2" src="https://github.com/user-attachments/assets/c91da9df-b7d3-4e35-9750-fcb5de20c011" />


Once the enclosure has been printed, all that remains is to heat-set four M3 4mm threaded inserts into the four corners around the screen.

