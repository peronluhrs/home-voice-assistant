# Home Voice Assistant (C++ • CMake)

Un squelette minimal pour démarrer un **assistant vocal local** que tu pourras
faire évoluer avec **Jules 2** et **OpenGPT-OSS** (ou tout serveur compatible OpenAI API).
Le code **compile sans dépendances audio** par défaut, et peut tomber en mode **REPL texte**.
Quand tu activeras l’audio (ASR/TTS), branche ce que tu veux (Whisper/whisper.cpp, Vosk, Piper, eSpeak-NG, etc.).

## Points clés

- **C++17**, **CMake** (minimum 3.10).
- **Robuste**: `nlohmann/json` pour le parsing JSON, et `libcurl` pour les appels HTTP.
- **Configurable**: Par fichier `.env`, avec surcharge via CLI.
- **Modes online/offline**: Fonctionne avec ou without une connexion à un serveur LLM.
- **VS Code ready**: Tâches et configurations de debug fournies.

## Pré-requis (Ubuntu)

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config
# Optionnel pour appels API (recommandé) :
sudo apt install -y libcurl4-openssl-dev
```
> **Note**: Le projet nécessite CMake version 3.10 ou supérieure.

## Configuration

La configuration se fait via le fichier `config/app.env`. Les valeurs par défaut sont :
```
API_BASE=http://localhost:8000/v1
API_KEY=EMPTY
MODEL=gpt-4o-mini
```
Ces valeurs peuvent être surchargées par des arguments en ligne de commande.

### Arguments CLI

Les arguments suivants ont la priorité sur `config/app.env`:
- `--api-base <url>`: URL de base de l'API (ex: `http://localhost:8000/v1`).
- `--model <name>`: Nom du modèle à utiliser (ex: `llama3.1`).
- `--api-key <token>`: Clé d'API.
- `--offline`: Force le mode hors-ligne, même si `libcurl` est installé.
- `--with-audio`: Active le mode audio (enregistrement et lecture).
- `--list-devices`: Liste les périphériques audio disponibles et quitte.
- `--input-device <nom|index>`: Sélectionne le périphérique d'entrée audio.
- `--output-device <nom|index>`: Sélectionne le périphérique de sortie audio.
- `--record-seconds N`: Durée de l'enregistrement en secondes (défaut: 5).
- `--asr <vosk|whisper|none>`: Moteur ASR à utiliser (défaut: `none`).
- `--vosk-model <dir>`: Chemin vers le modèle Vosk.
- `--with-piper`: Active la synthèse vocale avec Piper.
- `--piper-model <path.onnx>`: Chemin vers le modèle Piper.

## Lancement

Des scripts sont fournis pour faciliter le lancement dans différents modes.

### Mode Texte (Offline)

Ce mode ne nécessite aucune connexion réseau et répond en répétant l'entrée (echo).
```bash
./scripts/run_text_offline.sh
```
Le script va compiler le projet (si nécessaire) et le lancer avec le flag `--offline`.

### Mode Online (OpenGPT-OSS)

Ce mode se connecte à un serveur compatible OpenAI, comme [OpenGPT-OSS](https://github.com/jules-ai/opengpt-oss).
```bash
./scripts/run_online_opengptoss.sh
```
Par défaut, il utilise `API_BASE=http://localhost:8000/v1` et `API_KEY=EMPTY`. Vous pouvez surcharger ces variables d'environnement si besoin.

### Mode Online (Ollama)

Ce mode est configuré pour un serveur [Ollama](https://ollama.ai/) avec un modèle comme Llama 3.1.
```bash
./scripts/run_online_ollama.sh
```
Ce script configure `API_BASE` pour `http://127.0.0.1:11434/v1` et `MODEL` pour `llama3.1`.

### Mode Audio

Pour utiliser les fonctionnalités audio, le projet doit être compilé avec l'option `WITH_AUDIO=ON`.

#### Lister les périphériques audio
Pour voir les périphériques audio disponibles :
```bash
./scripts/run_bt_list_devices.sh
```

#### Enregistrement et lecture
Pour un test simple d'enregistrement et de lecture :
```bash
./scripts/run_audio_offline.sh
```
Ce script enregistre 3 secondes d'audio depuis le périphérique par défaut et le rejoue sur le périphérique par défaut.

### Boucle Vocale Complète (ASR/TTS)

Pour une expérience vocale complète, vous pouvez utiliser Vosk pour l'ASR et Piper pour le TTS.

**Installation des dépendances ASR/TTS:**
- **Vosk**: Suivez les instructions d'installation de la [librairie C++ de Vosk](https://github.com/alphacep/vosk-api). Assurez-vous que `libvosk.so` est trouvable par `pkg-config`.
- **Piper**: Installez Piper via `pipx`:
  ```bash
  python3 -m pip install --user pipx
  python3 -m pipx ensurepath
  ~/.local/bin/pipx install piper-tts
  ```

**Lancement de la boucle vocale:**
```bash
./scripts/run_audio_vosk_piper.sh
```
Ce script nécessite que les variables d'environnement `VOSK_MODEL_PATH` et `PIPER_MODEL_PATH` soient définies.

## Build Manuel

Si vous préférez compiler manuellement :
```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
# Lancer en mode offline
./home_assistant --offline
# ou online
./home_assistant --api-base http://mon-serveur/v1
```

## Arborescence

```
home-voice-assistant/
  ├─ CMakeLists.txt
  ├─ src/
  │   ├─ main.cpp
  │   ├─ Env.cpp
  │   ├─ OpenAIClient.cpp
  │   ├─ Audio.cpp
  │   ├─ AsrVosk.cpp
  │   └─ TtsPiper.cpp
  ├─ include/
  │   ├─ Env.h
  │   ├─ OpenAIClient.h
  │   ├─ Audio.h
  │   ├─ AsrVosk.h
  │   └─ TtsPiper.h
  ├─ third_party/
  │   └─ nlohmann/json.hpp
  ├─ scripts/
  │   ├─ run_text_offline.sh
  │   ├─ run_audio_offline.sh
  │   ├─ run_bt_list_devices.sh
  │   ├─ run_audio_vosk_piper.sh
  │   ├─ run_online_opengptoss.sh
  │   └─ run_online_ollama.sh
  ├─ config/
  │   └─ app.env
  ...
```
