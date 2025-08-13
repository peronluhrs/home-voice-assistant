
# Home Voice Assistant (C++ • CMake) — scaffold

Un squelette minimal pour démarrer un **assistant vocal local** que tu pourras
faire évoluer avec **Jules 2** et **OpenGPT-OSS** (ou tout serveur compatible OpenAI API).
Le code **compile sans dépendances audio** par défaut, et peut tomber en mode **REPL texte**.
Quand tu activeras l’audio (ASR/TTS), branche ce que tu veux (Whisper/whisper.cpp, Vosk, Piper, eSpeak-NG, etc.).

## Points clés

- **C++17**, **CMake**.
- **Optionnel**: `libcurl` pour appeler une API de LLM (OpenGPT-OSS via endpoint OpenAI-compatible).
- **Sans lib JSON** : on construit le JSON à la main et on extrait grossièrement le `content`.
  Tu pourras remplacer par *nlohmann/json* plus tard.
- **Config par `.env`** (fichier `config/app.env`). Facile à éditer.
- **VS Code ready** : tâches et debug fournis.

## Arborescence

```
home-voice-assistant/
  ├─ CMakeLists.txt
  ├─ src/
  │   ├─ main.cpp
  │   ├─ Env.cpp
  │   ├─ OpenAIClient.cpp
  │   └─ Utils.cpp
  ├─ include/
  │   ├─ Env.h
  │   ├─ OpenAIClient.h
  │   └─ Utils.h
  ├─ config/
  │   └─ app.env
  ├─ .vscode/
  │   ├─ settings.json
  │   ├─ tasks.json
  │   └─ launch.json
  ├─ .gitignore
  ├─ LICENSE
  └─ README.md
```

## Pré-requis (Ubuntu)

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config
# Optionnel pour appels API :
sudo apt install -y libcurl4-openssl-dev
```

## Configurer l’endpoint (OpenGPT-OSS / OpenAI-compat)

Édite `config/app.env` :
```
API_BASE=http://localhost:8000/v1
API_KEY=EMPTY
MODEL=gpt-4o-mini
LANG=fr-FR
WAKE_WORD=jarvis
ASR_ENGINE=disabled
TTS_ENGINE=disabled
```

> Pour **OpenGPT-OSS**, mets `API_BASE` sur ton instance (souvent `http://localhost:8000/v1`) et `API_KEY` selon la conf.
> Le client est compatible avec l’API OpenAI "chat completions".

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Lancer (REPL texte)

```bash
./build/home_assistant
```

Exemple :
```
[assistant] prêt. tape /exit pour quitter.
you> bonjour
assistant> (offline) Echo: bonjour
```

Si `libcurl` est installée, et l’endpoint LLM dispo, les prompts sont envoyés à
`$API_BASE/chat/completions` avec le modèle `$MODEL`.

## VS Code

- Ouvre le dossier.
- Tape **Ctrl+Shift+B** pour builder (tâche “CMake: Build”).
- Lancement debug via **Run and Debug** ⇒ *Home Assistant*.

## Activer l’audio plus tard

Le code est organisé pour accueillir des modules :
- `audio/` (capture micro, ex: PortAudio)
- `asr/` (reconnaissance vocale, ex: Whisper.cpp, Vosk)
- `tts/` (synthèse, ex: Piper, eSpeak-NG)

Tu pourras ajouter des options CMake (e.g. `-DWITH_AUDIO=ON`) et les backends désirés.

## Git

Le zip contient déjà l’arborescence. Si tu veux un repo local :
```bash
git init
git add .
git commit -m "bootstrap: home voice assistant scaffold"
```

## Avertissement

- L’extraction JSON est volontairement minimaliste (regex naïve). Remplace par une
  lib JSON propre (ex: **nlohmann/json**) quand tu branches vraiment l’API.
- Le mode “offline” te permet d’itérer sans dépendances externes.
