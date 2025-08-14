cmake_minimum_required(VERSION 3.10)
project(home_voice_assistant LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options (TOUTES OFF par défaut, sauf CURL pour garder l'HTTP quand dispo)
option(WITH_CURL   "Use libcurl for HTTP requests" ON)
option(WITH_AUDIO  "Enable PortAudio capture/playback" OFF)
option(WITH_VOSK   "Enable Vosk ASR backend" OFF)
option(WITH_WHISPER "Enable whisper.cpp ASR backend" OFF)  # stub/optionnel
option(WITH_PIPER  "Enable Piper TTS backend (CLI)" OFF)

# Sources de base (texte uniquement)
set(SRCS
  src/main.cpp
  src/Env.cpp
  src/OpenAIClient.cpp
  src/Utils.cpp
  src/Vad.cpp
  src/dr_wav_impl.cpp
)

# Audio / ASR / TTS optionnels (n'ajoute les .cpp que si l'option est active)
if (WITH_AUDIO)
  list(APPEND SRCS src/Audio.cpp)
  add_definitions(-DWITH_AUDIO=1)
endif()

if (WITH_VOSK)
  list(APPEND SRCS src/AsrVosk.cpp)
  add_definitions(-DWITH_VOSK=1)
endif()

if (WITH_PIPER)
  list(APPEND SRCS src/TtsPiper.cpp)
  add_definitions(-DWITH_PIPER=1)
endif()

add_executable(home_assistant ${SRCS})

# Headers locaux + éventuel third_party (nlohmann/json.hpp, etc.)
target_include_directories(home_assistant PRIVATE include third_party)

# ----- CURL rétro-compatible (ne JAMAIS forcer CURL::libcurl en string) -----
if (WITH_CURL)
  find_package(CURL)
  if (CURL_FOUND)
    target_compile_definitions(home_assistant PRIVATE HAVE_CURL=1)
    if (TARGET CURL::libcurl) # nouveaux FindCURL
      target_link_libraries(home_assistant PRIVATE CURL::libcurl)
    else()                    # anciens FindCURL (CMake 3.10)
      target_include_directories(home_assistant PRIVATE ${CURL_INCLUDE_DIRS})
      target_link_libraries(home_assistant PRIVATE ${CURL_LIBRARIES})
    endif()
  else()
    message(WARNING "libcurl not found. Building without HTTP support (offline echo mode).")
  endif()
endif()

# ----- PortAudio optionnel (sans Find module officiel) -----
if (WITH_AUDIO)
  # On tente d'abord find_library/find_path simples
  find_path(PORTAUDIO_INCLUDE_DIR portaudio.h)
  find_library(PORTAUDIO_LIBRARY portaudio)
  if (PORTAUDIO_INCLUDE_DIR AND PORTAUDIO_LIBRARY)
    target_include_directories(home_assistant PRIVATE ${PORTAUDIO_INCLUDE_DIR})
    target_link_libraries(home_assistant PRIVATE ${PORTAUDIO_LIBRARY})
  else()
    message(WARNING "PortAudio not found. Audio will be disabled at runtime.")
  endif()
endif()

# ----- Vosk optionnel -----
if (WITH_VOSK)
  find_path(VOSK_INCLUDE_DIR vosk_api.h)
  find_library(VOSK_LIBRARY vosk)
  if (VOSK_INCLUDE_DIR AND VOSK_LIBRARY)
    target_include_directories(home_assistant PRIVATE ${VOSK_INCLUDE_DIR})
    target_link_libraries(home_assistant PRIVATE ${VOSK_LIBRARY})
  else()
    message(WARNING "Vosk not found. ASR Vosk will be disabled at runtime.")
  endif()
endif()

# Whisper/Piper : rien à lier par défaut (whisper: à brancher plus tard ; piper: CLI externe)

# --- FS compatibility link for old GCC ---
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(home_assistant PRIVATE stdc++fs)
  endif()
endif()

# --- Ensure VAD + dr_wav implementation are compiled ---
target_sources(home_assistant PRIVATE
  src/Vad.cpp
  src/dr_wav_impl.cpp
)
