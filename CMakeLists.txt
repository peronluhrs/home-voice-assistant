
cmake_minimum_required(VERSION 3.10)
project(home_voice_assistant LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WITH_CURL "Use libcurl for HTTP requests" ON)

add_executable(home_assistant
    src/main.cpp
    src/Env.cpp
    src/OpenAIClient.cpp
    src/Audio.cpp
    src/AsrVosk.cpp
    src/TtsPiper.cpp
)

target_include_directories(home_assistant PRIVATE include third_party)

if (WITH_CURL)
  find_package(CURL)
  if (CURL_FOUND)
    target_compile_definitions(home_assistant PRIVATE HAVE_CURL=1)
    target_link_libraries(home_assistant PRIVATE CURL::libcurl)
  else()
    message(WARNING "libcurl not found. Building without HTTP support (offline echo mode).")
  endif()
endif()

option(WITH_AUDIO "Enable audio support with PortAudio" OFF)
if (WITH_AUDIO)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
  if (PORTAUDIO_FOUND)
    target_compile_definitions(home_assistant PRIVATE WITH_AUDIO=1)
    target_include_directories(home_assistant PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
    target_link_libraries(home_assistant PRIVATE ${PORTAUDIO_LIBRARIES})
  else()
    message(WARNING "PortAudio not found. Building without audio support.")
  endif()
endif()

option(WITH_VOSK "Enable Vosk ASR support" OFF)
if (WITH_VOSK)
  pkg_check_modules(VOSK vosk)
  if (VOSK_FOUND)
    target_compile_definitions(home_assistant PRIVATE WITH_VOSK=1)
    target_include_directories(home_assistant PRIVATE ${VOSK_INCLUDE_DIRS})
    target_link_libraries(home_assistant PRIVATE ${VOSK_LIBRARIES})
  else()
    message(WARNING "Vosk not found. Building without Vosk support.")
  endif()
endif()

option(WITH_PIPER "Enable Piper TTS support" OFF)
if (WITH_PIPER)
  find_program(PIPER_EXECUTABLE piper)
  if (PIPER_EXECUTABLE)
    target_compile_definitions(home_assistant PRIVATE WITH_PIPER=1)
  else()
    message(WARNING "Piper executable not found. Building without Piper TTS support.")
  endif()
endif()

# On Linux, link pthread if needed implicitly by curl; generally not required here.
