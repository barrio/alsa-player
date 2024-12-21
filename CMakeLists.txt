cmake_minimum_required(VERSION 3.10)

# Projektname und Version
project(AlsaPlayer VERSION 1.0 LANGUAGES CXX)

# Setze den C++-Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Suchpfad für die ALSA-Bibliothek und Header-Dateien
find_package(ALSA REQUIRED)

# Füge die ausführbare Datei hinzu
add_executable(alsaPlayer alsaPlayerOOP.cpp)

# Linke die ALSA-Bibliothek
target_link_libraries(alsaPlayer PRIVATE ALSA::ALSA)

# Compiler-Warnungen (optional, für bessere Codequalität)
target_compile_options(alsaPlayer PRIVATE -Wall -Wextra -pedantic)
