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
    src/Utils.cpp
)

# Headers locaux + nlohmann/json.hpp (vendored) si présent
target_include_directories(home_assistant PRIVATE include third_party)

if (WITH_CURL)
  find_package(CURL)
  if (CURL_FOUND)
    target_compile_definitions(home_assistant PRIVATE HAVE_CURL=1)
    # Rétro-compatibilité: certaines versions n'ont pas la cible CURL::libcurl
    if (TARGET CURL::libcurl)
      target_link_libraries(home_assistant PRIVATE CURL::libcurl)
    else()
      target_include_directories(home_assistant PRIVATE ${CURL_INCLUDE_DIRS})
      target_link_libraries(home_assistant PRIVATE ${CURL_LIBRARIES})
    endif()
  else()
    message(WARNING "libcurl not found. Building without HTTP support (offline echo mode).")
  endif()
endif()

# On Linux, pthread est généralement tiré transitivement par curl s'il est utilisé.
