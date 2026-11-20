cmake_minimum_required(VERSION 3.22)
project(dsda_android LANGUAGES C CXX)

# Third-party sources are downloaded by Gradle (task downloadDeps) into DSDA_DEPS_DIR.
set(DSDA_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party" CACHE PATH "Downloaded dependencies")
foreach(d SDL2 SDL2_mixer libzip)
  if(NOT EXISTS "${DSDA_DEPS_DIR}/${d}/CMakeLists.txt")
    message(FATAL_ERROR "Missing ${DSDA_DEPS_DIR}/${d}. Run the Gradle task 'downloadDeps' (it runs automatically on build).")
  endif()
endforeach()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# ---------------------------------------------------------------- SDL2 (shared)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${DSDA_DEPS_DIR}/SDL2" SDL2)
# SDL2_mixer is built static and looks for SDL2::SDL2-static; point it at the shared SDL2.
if(NOT TARGET SDL2::SDL2-static)
  add_library(SDL2::SDL2-static INTERFACE IMPORTED)
  target_link_libraries(SDL2::SDL2-static INTERFACE SDL2::SDL2)
endif()

# -------------------------------------------------------------------- libzip
# libzip's own CMake probes crash cmake on some on-device setups, so build the
# sources directly with pre-generated config headers (android/libzip).
if(ANDROID)
  find_library(ANDROID_Z_LIB z)
else()
  find_package(ZLIB REQUIRED)
  set(ANDROID_Z_LIB ZLIB::ZLIB)
endif()
set(LZ "${DSDA_DEPS_DIR}/libzip/lib")
file(GLOB LIBZIP_SRC "${LZ}/zip_*.c")
set(LIBZIP_SKIP zip_algorithm_bzip2 zip_algorithm_xz zip_algorithm_zstd
    zip_crypto_commoncrypto zip_crypto_gnutls zip_crypto_mbedtls zip_crypto_openssl zip_crypto_win
    zip_random_uwp zip_random_win32 zip_source_file_win32 zip_source_file_win32_ansi
    zip_source_file_win32_named zip_source_file_win32_utf16 zip_source_file_win32_utf8
    zip_winzip_aes zip_source_winzip_aes_decode zip_source_winzip_aes_encode)
set(LIBZIP_FILES "")
foreach(f ${LIBZIP_SRC})
  get_filename_component(n "${f}" NAME_WE)
  list(FIND LIBZIP_SKIP "${n}" idx)
  if(idx EQUAL -1)
    list(APPEND LIBZIP_FILES "${f}")
  endif()
endforeach()
add_library(zip STATIC ${LIBZIP_FILES} "${CMAKE_CURRENT_SOURCE_DIR}/android/libzip/zip_err_str.c")
add_library(libzip::zip ALIAS zip)
target_include_directories(zip PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/android/libzip" "${CMAKE_CURRENT_SOURCE_DIR}/android/libzip/inc" "${LZ}")
target_include_directories(zip PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/android/libzip/inc" "${LZ}")
target_compile_definitions(zip PRIVATE HAVE_CONFIG_H _GNU_SOURCE)
target_compile_options(zip PRIVATE -w)
set_target_properties(zip PROPERTIES C_VISIBILITY_PRESET default)
target_link_libraries(zip PRIVATE ${ANDROID_Z_LIB})

# ------------------------------------------------------------------ SDL2_mixer
set(SDL2MIXER_VENDORED OFF CACHE BOOL "" FORCE)
foreach(o SDL2MIXER_SAMPLES SDL2MIXER_INSTALL SDL2MIXER_CMD SDL2MIXER_GME
          SDL2MIXER_MOD SDL2MIXER_MIDI SDL2MIXER_OPUS SDL2MIXER_WAVPACK)
  set(${o} OFF CACHE BOOL "" FORCE)
endforeach()
set(SDL2MIXER_FLAC ON CACHE BOOL "" FORCE)
set(SDL2MIXER_MP3 ON CACHE BOOL "" FORCE)
set(SDL2MIXER_VORBIS "STB" CACHE STRING "" FORCE)
set(SDL2MIXER_WAVE ON CACHE BOOL "" FORCE)
set(SDL2MIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory("${DSDA_DEPS_DIR}/SDL2_mixer" SDL2_mixer EXCLUDE_FROM_ALL)

# ----------------------------------------------------------------------- DSDA
include(sources.cmake)
set(DSDA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dsda-doom")
set(DSDA_SOURCES "")
foreach(f ${DSDA_SRC_LIST})
  list(APPEND DSDA_SOURCES "${DSDA_DIR}/${f}")
endforeach()
list(APPEND DSDA_SOURCES
  android/dsda_android.c
  android/gl_stubs.c
  android/sndload.c)

# SDLActivity loads "main" -> libmain.so which exports SDL_main
add_library(main SHARED ${DSDA_SOURCES})

target_include_directories(main PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/android"          # config.h, dsda_android.h
  "${CMAKE_CURRENT_SOURCE_DIR}/android/shim"     # GL/gl.h GL/glu.h
  "${DSDA_DIR}")

target_compile_definitions(main PRIVATE
  HAVE_CONFIG_H
  DSDA_ANDROID=1
  _GNU_SOURCE
  stricmp=strcasecmp
  strnicmp=strncasecmp)

set_source_files_properties("${DSDA_DIR}/textscreen/txt_sdl.c" PROPERTIES
  COMPILE_DEFINITIONS "glBegin=dsda_stub_glBegin;glBindTexture=dsda_stub_glBindTexture;glClear=dsda_stub_glClear;glClearColor=dsda_stub_glClearColor;glDeleteTextures=dsda_stub_glDeleteTextures;glDisable=dsda_stub_glDisable;glEnable=dsda_stub_glEnable;glEnd=dsda_stub_glEnd;glGenTextures=dsda_stub_glGenTextures;glLoadIdentity=dsda_stub_glLoadIdentity;glMatrixMode=dsda_stub_glMatrixMode;glOrtho=dsda_stub_glOrtho;glScalef=dsda_stub_glScalef;glTexCoord2f=dsda_stub_glTexCoord2f;glTexImage2D=dsda_stub_glTexImage2D;glTexParameteri=dsda_stub_glTexParameteri;glTexSubImage2D=dsda_stub_glTexSubImage2D;glTranslatef=dsda_stub_glTranslatef;glVertex2f=dsda_stub_glVertex2f;glViewport=dsda_stub_glViewport")

set_target_properties(main PROPERTIES C_STANDARD 99 C_EXTENSIONS ON CXX_STANDARD 11)

target_compile_options(main PRIVATE
  -O2 -ffast-math -w
  $<$<COMPILE_LANGUAGE:C>:-fcommon>)

if(TARGET SDL2_mixer::SDL2_mixer)
  set(DSDA_MIXER_TARGET SDL2_mixer::SDL2_mixer)
else()
  set(DSDA_MIXER_TARGET SDL2_mixer::SDL2_mixer-static)
endif()

target_link_libraries(main PRIVATE
  SDL2::SDL2
  ${DSDA_MIXER_TARGET}
  libzip::zip)

if(ANDROID)
  target_link_libraries(main PRIVATE ${ANDROID_Z_LIB} log android m)
else()
  find_package(ZLIB REQUIRED)
  target_link_libraries(main PRIVATE ZLIB::ZLIB m)
endif()
target_link_options(main PRIVATE -Wl,--no-undefined)
