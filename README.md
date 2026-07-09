# Telemetry V0.5 Core3.x

Firmware PlatformIO pour une telemetrie embarquee ESP32 de jetboat. Le projet lit les capteurs, enregistre les logs CSV sur carte SD, expose un HUD Wi-Fi en portail captif et renvoie quelques valeurs vers un recepteur I-Bus.

## Fonctionnalites

- ESP32 sous framework Arduino avec PlatformIO.
- Point d'acces Wi-Fi local et portail captif sur `192.168.4.1`.
- HUD LittleFS avec vitesse GPS, tension, courant, puissance, cap magnetique et etat d'enregistrement.
- API JSON temps reel sur `/data`.
- Liste et telechargement des logs SD via `/liste`, `/files` et `/download?file=...`.
- Enregistrement CSV a 10 Hz sur carte SD (`Log_001.csv`, `Log_002.csv`, etc.).
- Telemetrie I-Bus pour tension, courant et vitesse.

## Materiel cible

- Carte ESP32 compatible `esp32dev`.
- GPS serie.
- Boussole QMC5883L.
- Recepteur/telemetrie I-Bus.
- Carte SD.
- Capteur de tension et capteur de courant analogiques.

### Broches principales

| Fonction | Broche |
| --- | --- |
| SD CS | GPIO 5 |
| Tension analogique | GPIO 35 |
| Courant analogique | GPIO 34 |
| I-Bus telemetrie RX/TX | GPIO 27 / GPIO 26 |
| I-Bus canaux RX/TX | GPIO 32 / GPIO 1 |
| GPS RX/TX | GPIO 16 / GPIO 17 |
| LED statut | GPIO 2 |
| I2C boussole SDA/SCL | GPIO 21 / GPIO 22 |

## Preparation

1. Installer PlatformIO.
2. Cloner le depot.
3. Copier la configuration d'exemple :

   ```powershell
   Copy-Item include/config.example.h include/config.local.h
   ```

4. Modifier `include/config.local.h` avec le SSID et le mot de passe du point d'acces.
5. Compiler :

   ```powershell
   pio run
   ```

6. Televerser le firmware :

   ```powershell
   pio run -t upload
   ```

7. Televerser les fichiers web LittleFS :

   ```powershell
   pio run -t uploadfs
   ```

8. Ouvrir le moniteur serie si besoin :

   ```powershell
   pio device monitor
   ```

## Utilisation

Apres demarrage, se connecter au point d'acces Wi-Fi configure, puis ouvrir `http://192.168.4.1/`.

L'enregistrement est pilote par le canal I-Bus CH4. Un appui court entre 1 et 3 secondes alterne entre demarrage et arret de l'enregistrement. Pendant un enregistrement, la LED clignote et l'acces lecture a la carte SD est bloque pour eviter les conflits.

Les logs CSV sont ecrits au format :

```text
Temps(ms);Tension(V);Courant(A);Vitesse(km/h);Lat;Lon;Cap(deg);Ch1;Ch2;Ch3;Ch4
```

## Structure

```text
data/      Interface web LittleFS
include/   Configuration locale et en-tetes
lib/       Bibliotheques embarquees utilisees par le firmware
src/       Firmware principal
test/      Espace de tests PlatformIO
```

## Notes pour publication GitHub

- `include/config.local.h` est ignore par Git : ne pas versionner les mots de passe locaux.
- Les bibliotheques dans `lib/` sont conservees car le firmware les inclut directement.
- Choisir une licence avant de publier le depot publiquement.
