cmake_minimum_required(VERSION 3.28...3.30)

if(POLICY CMP0169)
  # Keep the explicit FetchContent_Populate flow used to build the SQLite
  # amalgamation as a plain C target when configuring with CMake 3.30+.
  cmake_policy(SET CMP0169 OLD)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version} LANGUAGES C CXX)

option(BUILD_PLUGIN "Build the native OBS plugin" ON)
option(BUILD_TESTS "Build ClipXtudio unit tests" ON)
option(BUILD_UI_TESTS "Build ClipXtudio Qt UI tests" ${BUILD_PLUGIN})
option(CLIPX_ENABLE_QA_LICENSE "Enable the local, non-production ClipXtudio QA license" OFF)
option(CLIPX_ALLOW_INSECURE_LOCAL_API "Allow plain HTTP only for loopback license API development" OFF)
option(ENABLE_FRONTEND_API "Use the OBS Frontend API" ON)
option(ENABLE_QT "Use Qt Widgets for the native dock" ON)
option(ENABLE_COVERAGE "Instrument C++ targets for line coverage" OFF)

if(ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-O0 -g --coverage)
    add_link_options(--coverage)
  else()
    message(FATAL_ERROR "ENABLE_COVERAGE requires GCC or Clang")
  endif()
endif()

set(
  CLIPX_SERVICE_BASE_URL
  "https://clipxtudio.com"
  CACHE STRING
  "Base URL for ClipXtudio web, licensing and AI services"
)
set(
  CLIPX_LOCAL_SERVICE_BASE_URL
  "http://127.0.0.1:8000"
  CACHE STRING
  "Loopback ClipXtudio backend used automatically for local license development"
)
set(
  CLIPX_UPDATE_MANIFEST_URL
  "https://clipxtudio.com/updates/latest.json"
  CACHE STRING
  "HTTPS JSON manifest used by the ClipXtudio update checker"
)
set(
  CLIPX_LOCAL_UPDATE_MANIFEST_URL
  "http://127.0.0.1:8000/updates/latest.json"
  CACHE STRING
  "Loopback update manifest used automatically during local development"
)

if(BUILD_PLUGIN AND OS_WINDOWS)
  # obs-deps supplies OpenSSL/Qt on Windows, so populate CMAKE_PREFIX_PATH
  # before resolving project-wide dependencies.
  include(buildspec)
endif()

if(OS_WINDOWS)
  # The plugin must not require a separately deployed libcrypto DLL.
  set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(SQLite3 QUIET)
if(NOT TARGET SQLite::SQLite3)
  include(FetchContent)
  FetchContent_Declare(
    sqlite_amalgamation
    URL "https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip"
    URL_HASH "SHA256=1e71ddf93849c6a6ecf58b827c0692073d2dd7ee40196158068f7b29f422e87d"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_GetProperties(sqlite_amalgamation)
  if(NOT sqlite_amalgamation_POPULATED)
    FetchContent_Populate(sqlite_amalgamation)
  endif()
  add_library(clipcoach_sqlite STATIC "${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c")
  add_library(SQLite::SQLite3 ALIAS clipcoach_sqlite)
  target_include_directories(clipcoach_sqlite PUBLIC "${sqlite_amalgamation_SOURCE_DIR}")
  target_compile_definitions(clipcoach_sqlite PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION)
  target_link_libraries(clipcoach_sqlite PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
  set_target_properties(clipcoach_sqlite PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

include(compilerconfig)

add_library(clipcoach_core STATIC)
target_sources(
  clipcoach_core
  PRIVATE
    src/core/ai-assistant.cpp
    src/core/clip-library-view-model.cpp
    src/core/clip-manager.cpp
    src/core/export-manager.cpp
    src/core/feature-gate-service.cpp
    src/core/hotkey-manager.cpp
    src/core/score-engine.cpp
    src/core/remote-command-executor.cpp
    src/core/speech-engine.cpp
    src/core/trigger-engine.cpp
    src/core/voice-audio-segmenter.cpp
    src/core/voice-trigger-service.cpp
    src/integrations/chat-integration.cpp
    src/integrations/chat-pulse-service.cpp
    src/core/vertical-canvas-manager.cpp
    src/core/settings.cpp
    src/core/settings-manager.cpp
)
target_include_directories(
  clipcoach_core
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>"
)
target_compile_features(clipcoach_core PUBLIC cxx_std_17)
set_target_properties(clipcoach_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(clipcoach_core PUBLIC OpenSSL::Crypto)

add_library(clipcoach_security STATIC)
target_sources(clipcoach_security PRIVATE src/security/platform-secure-storage.cpp)
target_include_directories(
  clipcoach_security
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>"
)
target_compile_features(clipcoach_security PUBLIC cxx_std_17)
set_target_properties(clipcoach_security PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(WIN32)
  target_link_libraries(clipcoach_security PRIVATE Advapi32)
endif()

add_library(clipcoach_licensing STATIC)
target_sources(
  clipcoach_licensing
  PRIVATE src/core/feature-gate.cpp src/core/license-manager.cpp src/core/machine-fingerprint.cpp
)
target_include_directories(
  clipcoach_licensing
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(clipcoach_licensing PUBLIC clipcoach_core clipcoach_security OpenSSL::Crypto)
target_compile_features(clipcoach_licensing PUBLIC cxx_std_17)
if(CLIPX_ENABLE_QA_LICENSE)
  target_compile_definitions(clipcoach_licensing PUBLIC CLIPX_ENABLE_QA_LICENSE=1)
endif()
set_target_properties(clipcoach_licensing PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(WIN32)
  target_link_libraries(clipcoach_licensing PRIVATE Advapi32)
endif()

add_library(clipcoach_storage STATIC)
target_sources(
  clipcoach_storage
  PRIVATE
    src/storage/clip-library-service.cpp
    src/storage/clip-repository.cpp
    src/storage/database-recovery.cpp
    src/storage/export-job-repository.cpp
    src/storage/session-repository.cpp
    src/storage/sqlite-database.cpp
)
target_include_directories(
  clipcoach_storage
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(clipcoach_storage PUBLIC clipcoach_core SQLite::SQLite3 Threads::Threads)
target_compile_features(clipcoach_storage PUBLIC cxx_std_17)
set_target_properties(clipcoach_storage PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(BUILD_TESTS OR BUILD_UI_TESTS)
  include(CTest)
  enable_testing()
endif()

if(BUILD_TESTS)
  add_subdirectory(tests/unit)
  add_subdirectory(tests/integration)
  add_subdirectory(tests/performance)
  if(WIN32)
    find_program(CLIPCOACH_POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
    add_test(
      NAME packaging.release-metadata
      COMMAND
        "${CLIPCOACH_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/packaging/Test-ReleaseMetadata.ps1" -ProjectRoot
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    set_tests_properties(packaging.release-metadata PROPERTIES LABELS packaging)
  endif()
endif()

if(BUILD_PLUGIN)
  include(defaults)
  include(helpers)
endif()

if(BUILD_PLUGIN OR BUILD_UI_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

  add_library(clipcoach_network STATIC)
  target_sources(
    clipcoach_network
    PRIVATE
      src/network/qt-license-api.cpp
      src/network/qt-ai-api.cpp
      src/network/qt-update-downloader.cpp
      src/network/qt-update-checker.cpp
      src/network/remote-clipper-client.cpp
      src/network/remote-result-outbox.cpp
      src/network/remote-command-poller.cpp
      src/security/rs256-token-verifier.cpp
  )
  target_include_directories(
    clipcoach_network
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>"
  )
  target_link_libraries(clipcoach_network PUBLIC clipcoach_licensing Qt6::Core Qt6::Network OpenSSL::Crypto)
  target_compile_features(clipcoach_network PUBLIC cxx_std_17)
  target_compile_definitions(
    clipcoach_network
    PRIVATE
      CLIPCOACH_VERSION="${PROJECT_VERSION}"
      $<$<BOOL:${CLIPX_ALLOW_INSECURE_LOCAL_API}>:CLIPX_ALLOW_INSECURE_LOCAL_API=1>
  )
  set_target_properties(clipcoach_network PROPERTIES POSITION_INDEPENDENT_CODE ON)

  add_library(clipcoach_export STATIC)
  target_sources(clipcoach_export PRIVATE src/export/ffmpeg-export-backend.cpp)
  target_include_directories(
    clipcoach_export
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>"
  )
  target_link_libraries(clipcoach_export PUBLIC clipcoach_core Qt6::Core)
  target_compile_features(clipcoach_export PUBLIC cxx_std_17)
  set_target_properties(clipcoach_export PROPERTIES POSITION_INDEPENDENT_CODE ON)

  add_library(clipcoach_ui STATIC)
  target_sources(
    clipcoach_ui
    PRIVATE
      src/ui/design-tokens.cpp
      src/ui/caption-generator.cpp
      src/ui/clip-library-controller.cpp
      src/ui/clips-tab.cpp
      src/ui/desktop-clip-action-service.cpp
      src/ui/integrations-panel.cpp
      src/ui/main-dock.cpp
      src/ui/pro-account-tab.cpp
      src/ui/quick-clip-editor-dialog.cpp
      src/ui/settings-controller.cpp
      src/ui/settings-tab.cpp
      src/ui/triggers-tab.cpp
      src/ui/vertical-preview-widget.cpp
      src/ui/vertical-tab.cpp
      src/ui/components/clip-card.cpp
      src/ui/components/empty-state.cpp
      src/ui/components/primary-button.cpp
      src/ui/components/pro-badge.cpp
      src/ui/components/score-badge.cpp
      include/clipcoach/ui/components/pro-badge.hpp
      include/clipcoach/ui/components/preview-interaction-gate.hpp
      include/clipcoach/ui/components/score-badge.hpp
      include/clipcoach/ui/components/wheel-safe-controls.hpp
      include/clipcoach/ui/quick-clip-editor-dialog.hpp
      src/ui/components/secondary-button.cpp
      src/ui/components/context-help-button.cpp
      src/ui/components/settings-section.cpp
      src/ui/components/status-card.cpp
      src/ui/components/tab-bar.cpp
      src/ui/components/toggle-row.cpp
      src/ui/components/upgrade-banner.cpp
      data/clipx-studio.qrc
  )
  target_include_directories(
    clipcoach_ui
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>"
  )
  target_link_libraries(
    clipcoach_ui
    PUBLIC clipcoach_core clipcoach_licensing clipcoach_storage clipcoach_network Qt6::Core Qt6::Widgets
  )
  target_compile_features(clipcoach_ui PUBLIC cxx_std_17)
  target_compile_definitions(
    clipcoach_ui
    PUBLIC
      CLIPCOACH_VERSION="${PROJECT_VERSION}"
      CLIPX_SERVICE_BASE_URL="${CLIPX_SERVICE_BASE_URL}"
      CLIPX_LOCAL_SERVICE_BASE_URL="${CLIPX_LOCAL_SERVICE_BASE_URL}"
      CLIPX_UPDATE_MANIFEST_URL="${CLIPX_UPDATE_MANIFEST_URL}"
      CLIPX_LOCAL_UPDATE_MANIFEST_URL="${CLIPX_LOCAL_UPDATE_MANIFEST_URL}"
      $<$<BOOL:${CLIPX_ALLOW_INSECURE_LOCAL_API}>:CLIPX_ALLOW_INSECURE_LOCAL_API=1>
  )
  set_target_properties(
    clipcoach_ui
    PROPERTIES AUTOMOC ON AUTORCC ON POSITION_INDEPENDENT_CODE ON
  )
endif()

if(BUILD_UI_TESTS)
  add_subdirectory(tests/ui)
  set(_clipcoach_bundled_ffmpeg "${CMAKE_CURRENT_SOURCE_DIR}/data/tools/ffmpeg/ffmpeg${CMAKE_EXECUTABLE_SUFFIX}")
  if(EXISTS "${_clipcoach_bundled_ffmpeg}")
    set(FFMPEG_EXECUTABLE "${_clipcoach_bundled_ffmpeg}")
  else()
    find_program(FFMPEG_EXECUTABLE ffmpeg)
  endif()
  if(TARGET clips-library-tab-test AND FFMPEG_EXECUTABLE)
    target_compile_definitions(clips-library-tab-test PRIVATE CLIPCOACH_TEST_FFMPEG_EXECUTABLE="${FFMPEG_EXECUTABLE}")
  endif()
  find_program(FFPROBE_EXECUTABLE ffprobe)
  if(FFMPEG_EXECUTABLE AND FFPROBE_EXECUTABLE)
    add_executable(ffmpeg-export-smoke tests/integration/ffmpeg-export-smoke.cpp)
    target_link_libraries(ffmpeg-export-smoke PRIVATE clipcoach_export clipcoach_core Qt6::Core)
    target_compile_definitions(
      ffmpeg-export-smoke
      PRIVATE FFMPEG_EXECUTABLE="${FFMPEG_EXECUTABLE}" FFPROBE_EXECUTABLE="${FFPROBE_EXECUTABLE}"
    )
    add_test(NAME integration.ffmpeg-export COMMAND ffmpeg-export-smoke)
    if(WIN32)
      get_filename_component(_clipcoach_ffmpeg_bin_dir "${FFMPEG_EXECUTABLE}" DIRECTORY)
      get_target_property(_clipcoach_ffmpeg_qt_core_dll Qt6::Core IMPORTED_LOCATION_RELEASE)
      if(NOT _clipcoach_ffmpeg_qt_core_dll)
        get_target_property(_clipcoach_ffmpeg_qt_core_dll Qt6::Core IMPORTED_LOCATION)
      endif()
      get_filename_component(_clipcoach_ffmpeg_qt_bin_dir "${_clipcoach_ffmpeg_qt_core_dll}" DIRECTORY)
      set_tests_properties(
        integration.ffmpeg-export
        PROPERTIES
          ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_clipcoach_ffmpeg_bin_dir};PATH=path_list_prepend:${_clipcoach_ffmpeg_qt_bin_dir}"
      )
    endif()
  endif()
endif()

if(BUILD_PLUGIN)
  include(FetchContent)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled dependencies statically" FORCE)
  set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
  set(WHISPER_CURL OFF CACHE BOOL "" FORCE)
  set(GGML_NATIVE OFF CACHE BOOL "Portable CPU dispatch for OBS hosts" FORCE)
  set(GGML_OPENMP OFF CACHE BOOL "Avoid a separate OpenMP runtime in the plugin" FORCE)
  set(GGML_CUDA OFF CACHE BOOL "" FORCE)
  set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
  set(GGML_METAL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    whisper_cpp
    GIT_REPOSITORY "https://github.com/ggml-org/whisper.cpp.git"
    GIT_TAG "f049fff95a089aa9969deb009cdd4892b3e74916"
    GIT_SHALLOW TRUE
  )
  set(_clipx_warning_as_error "${CMAKE_COMPILE_WARNING_AS_ERROR}")
  set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)
  FetchContent_MakeAvailable(whisper_cpp)
  set(CMAKE_COMPILE_WARNING_AS_ERROR "${_clipx_warning_as_error}")
  unset(_clipx_warning_as_error)
  if(MSVC)
    # The project enables /WX for first-party code. Keep upstream conversion
    # warnings visible without treating third-party whisper.cpp as our errors.
    set_property(TARGET whisper PROPERTY COMPILE_WARNING_AS_ERROR OFF)
    if(TARGET parakeet)
      set_property(TARGET parakeet PROPERTY COMPILE_WARNING_AS_ERROR OFF)
      set_property(TARGET parakeet PROPERTY EXCLUDE_FROM_ALL TRUE)
    endif()
  endif()

  add_library(${CMAKE_PROJECT_NAME} MODULE)

  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)

  target_sources(
    ${CMAKE_PROJECT_NAME}
    PRIVATE
      src/plugin/obs-replay-manager.cpp
      src/plugin/obs-hotkey-adapter.cpp
      src/plugin/plugin-main.cpp
      src/plugin/obs-vertical-preview.cpp
      src/plugin/clip-caption-transcriber.cpp
      src/plugin/remote-capture-coordinator.cpp
  )
  target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_link_libraries(
    ${CMAKE_PROJECT_NAME}
    PRIVATE
      clipcoach_core
      clipcoach_licensing
      clipcoach_network
      clipcoach_security
      clipcoach_export
      clipcoach_ui
      OBS::libobs
      OBS::obs-frontend-api
      whisper
  )
  if(WIN32)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin/windows-voice-trigger-controller.cpp)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Wintrust Crypt32)
  endif()
  target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)
  target_compile_definitions(
    ${CMAKE_PROJECT_NAME}
    PRIVATE
      CLIPCOACH_VERSION="${PROJECT_VERSION}"
      CLIPX_SERVICE_BASE_URL="${CLIPX_SERVICE_BASE_URL}"
      CLIPX_LOCAL_SERVICE_BASE_URL="${CLIPX_LOCAL_SERVICE_BASE_URL}"
      CLIPX_LOCAL_UPDATE_MANIFEST_URL="${CLIPX_LOCAL_UPDATE_MANIFEST_URL}"
      $<$<BOOL:${CLIPX_ALLOW_INSECURE_LOCAL_API}>:CLIPX_ALLOW_INSECURE_LOCAL_API=1>
  )
  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTOMOC ON)

  set_target_properties_plugin(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES OUTPUT_NAME ${_name}
  )

  if(WIN32)
    if(NOT TARGET Qt6::QSchannelBackendPlugin)
      message(FATAL_ERROR "Qt Schannel TLS backend is required for HTTPS updates and licensing")
    endif()
    install(
      FILES "$<TARGET_FILE:Qt6::QSchannelBackendPlugin>"
      DESTINATION "${CMAKE_PROJECT_NAME}/data/qt-plugins/tls"
      COMPONENT Runtime
    )
    add_custom_command(
      TARGET ${CMAKE_PROJECT_NAME}
      POST_BUILD
      COMMAND
        ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/${CMAKE_PROJECT_NAME}/qt-plugins/tls"
      COMMAND
        ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt6::QSchannelBackendPlugin>"
        "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/${CMAKE_PROJECT_NAME}/qt-plugins/tls/"
      COMMENT "Packaging Qt Schannel TLS backend"
    )
  endif()

  if(BUILD_TESTS)
    add_test(
      NAME packaging.rundir-layout
      COMMAND
        ${CMAKE_COMMAND} "-DPLUGIN_FILE=$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
        "-DRESOURCE_DIR=${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/${CMAKE_PROJECT_NAME}" -P
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/packaging/verify-rundir.cmake"
    )
    set_tests_properties(packaging.rundir-layout PROPERTIES LABELS packaging)
  endif()
endif()
