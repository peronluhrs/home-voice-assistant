
cmake_minimum_required(VERSION 3.12)
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

target_include_directories(home_assistant PRIVATE include)

if (WITH_CURL)
  find_package(CURL)
  if (CURL_FOUND)
    target_compile_definitions(home_assistant PRIVATE HAVE_CURL=1)
    target_link_libraries(home_assistant PRIVATE CURL::libcurl)
  else()
    message(WARNING "libcurl not found. Building without HTTP support (offline echo mode).")
  endif()
endif()

# On Linux, link pthread if needed implicitly by curl; generally not required here.
