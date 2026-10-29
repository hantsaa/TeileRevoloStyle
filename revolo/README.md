# RevoloStyle — VST3 pour FL Studio (Windows)

Reverb + trémolo façon pédale DJ, inspiré du manuel du TEIL2 Revolo (projet non officiel).
Formats : **VST3** (FL Studio 20.8 ou plus récent) et **Standalone** (test sans DAW).

## 1. Obtenir les fichiers sans rien installer (GitHub Actions)
1. Crée un dépôt GitHub et pousse ce dossier dedans.
2. Onglet **Actions** → « Build VST3 (Windows) » → attends ~10 min.
3. Télécharge l'artefact `RevoloStyle-Windows` et dézippe-le : tu y trouves `RevoloStyle.vst3`,
   `RevoloStyle.exe` (standalone) et `install.bat`.

## 2. Installer dans FL Studio
1. Clic droit sur `install.bat` → **Exécuter en tant qu'administrateur**
   (copie le dossier dans `C:\Program Files\Common Files\VST3`).
2. FL Studio : **Options → Manage plugins → Find installed plugins**
   (vérifie que ce dossier VST3 est bien dans la liste des dossiers scannés).
3. Dans le Mixer, clique un emplacement d'effet → **Plugin database → Installed → Effects → RevoloStyle**.
   Pour l'usage DJ classique : met l'effet sur une piste d'envoi (send), mode **WET** ;
   pour traiter une piste directement : mode **MIX**.

## 3. Utilisation
- **ONF** : active/coupe l'effet. En mode **MOM** (momentané) il n'agit que pendant l'appui ; la queue de reverb continue ensuite.
- **TEMPO** : vitesse du trémolo (0,2–40 Hz), ou **TAP TEMPO** (4 derniers taps).
- **SYNC** : le trémolo se cale sur le tempo du projet FL Studio ; le bouton TEMPO choisit alors la division (1/2 … 1/32, triolets).
- Tous les boutons sont automatisables (clic droit sur un bouton dans FL → *Create automation clip*, ou via le wrapper VST).
- Double-clic sur un bouton = valeur par défaut. La fenêtre est redimensionnable.

## Compiler en local
Prérequis : Visual Studio 2022 (charge « Desktop development with C++ »), CMake ≥ 3.22, Git.

    cmake -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release

Résultat : `build/RevoloStyle_artefacts/Release/VST3/RevoloStyle.vst3`

## Test du moteur audio (sans JUCE)
    g++ -O2 -std=c++17 -o test_dsp test/test_dsp.cpp && ./test_dsp
