if(WIN32)
  set(CLIPCOACH_QT_TEST_PLATFORM "minimal")
else()
  set(CLIPCOACH_QT_TEST_PLATFORM "offscreen")
endif()

add_executable(components-construction-test components-construction-test.cpp)
target_link_libraries(components-construction-test PRIVATE clipcoach_ui Qt6::Core Qt6::Widgets)
add_test(NAME ui.components-construction COMMAND components-construction-test)
set_tests_properties(
  ui.components-construction
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=${CLIPCOACH_QT_TEST_PLATFORM}"
)

add_executable(wheel-safe-controls-test wheel-safe-controls-test.cpp)
target_link_libraries(wheel-safe-controls-test PRIVATE clipcoach_ui Qt6::Core Qt6::Widgets)
add_test(NAME ui.wheel-safe-controls COMMAND wheel-safe-controls-test)
set_tests_properties(
  ui.wheel-safe-controls
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=${CLIPCOACH_QT_TEST_PLATFORM}"
)

add_executable(preview-interaction-gate-test preview-interaction-gate-test.cpp)
target_link_libraries(preview-interaction-gate-test PRIVATE clipcoach_ui)
add_test(NAME ui.preview-interaction-gate COMMAND preview-interaction-gate-test)

add_executable(main-dock-ui-test main-dock-ui-test.cpp)
target_link_libraries(main-dock-ui-test PRIVATE clipcoach_ui Qt6::Core Qt6::Widgets)
add_test(NAME ui.main-dock COMMAND main-dock-ui-test)
set_tests_properties(ui.main-dock PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=${CLIPCOACH_QT_TEST_PLATFORM}")

add_executable(capture-tab-test capture-tab-test.cpp)
target_link_libraries(
  capture-tab-test
  PRIVATE clipcoach_core clipcoach_ui Qt6::Core Qt6::Widgets
)
add_test(NAME ui.capture-tab COMMAND capture-tab-test)
set_tests_properties(ui.capture-tab PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=${CLIPCOACH_QT_TEST_PLATFORM}")

add_executable(clips-library-tab-test clips-library-tab-test.cpp)
target_link_libraries(
  clips-library-tab-test
  PRIVATE clipcoach_storage clipcoach_ui Qt6::Core Qt6::Widgets
)
target_compile_definitions(
  clips-library-tab-test
  PRIVATE
    CLIPCOACH_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    CLIPCOACH_UI_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/ui-artifacts"
)
add_test(NAME ui.clips-library-tab COMMAND clips-library-tab-test)
set_tests_properties(ui.clips-library-tab PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=${CLIPCOACH_QT_TEST_PLATFORM}")

add_executable(clips-tab-smoke-test clips-tab-smoke-test.cpp)
target_link_libraries(
  clips-tab-smoke-test
  PRIVATE clipcoach_ui Qt6::Core Qt6::Widgets
)
target_compile_definitions(
  clips-tab-smoke-test
  PRIVATE CLIPCOACH_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
add_test(NAME ui.clips-tab-smoke COMMAND clips-tab-smoke-test)
set_tests_properties(ui.clips-tab-smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=${CLIPCOACH_QT_TEST_PLATFORM}")

add_executable(settings-tab-smoke-test settings-tab-smoke-test.cpp)
target_link_libraries(
  settings-tab-smoke-test
  PRIVATE clipcoach_ui Qt6::Core Qt6::Widgets
)
target_compile_definitions(
  settings-tab-smoke-test
  PRIVATE CLIPCOACH_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
add_test(NAME ui.settings-tab-smoke COMMAND settings-tab-smoke-test)
set_tests_properties(ui.settings-tab-smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=${CLIPCOACH_QT_TEST_PLATFORM}")

add_executable(localization-keys-test localization-keys-test.cpp)
target_link_libraries(localization-keys-test PRIVATE clipcoach_ui)
target_compile_definitions(
  localization-keys-test
  PRIVATE CLIPCOACH_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
add_test(NAME ui.localization-keys COMMAND localization-keys-test)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/ui-artifacts")
add_executable(main-dock-render-test main-dock-render-test.cpp)
target_link_libraries(main-dock-render-test PRIVATE clipcoach_ui Qt6::Core Qt6::Widgets)
target_compile_definitions(
  main-dock-render-test
  PRIVATE
    CLIPCOACH_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    CLIPCOACH_UI_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/ui-artifacts"
)
add_test(NAME ui.main-dock-render COMMAND main-dock-render-test)
set_tests_properties(
  ui.main-dock-render
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=${CLIPCOACH_QT_TEST_PLATFORM}"
)

add_executable(vertical-tab-test vertical-tab-test.cpp)
target_link_libraries(vertical-tab-test PRIVATE clipcoach_ui Qt6::Core Qt6::Widgets)
target_compile_definitions(
  vertical-tab-test
  PRIVATE CLIPCOACH_UI_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/ui-artifacts"
)
add_test(NAME ui.vertical-tab COMMAND vertical-tab-test)
set_tests_properties(ui.vertical-tab PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=${CLIPCOACH_QT_TEST_PLATFORM}")

add_executable(triggers-tab-test triggers-tab-test.cpp)
target_link_libraries(triggers-tab-test PRIVATE clipcoach_ui Qt6::Core Qt6::Widgets)
add_test(NAME ui.triggers-tab COMMAND triggers-tab-test)
set_tests_properties(ui.triggers-tab PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=${CLIPCOACH_QT_TEST_PLATFORM}")

add_executable(integrations-panel-test integrations-panel-test.cpp)
target_link_libraries(integrations-panel-test PRIVATE clipcoach_ui Qt6::Core Qt6::Widgets)
add_test(NAME ui.integrations-panel COMMAND integrations-panel-test)
set_tests_properties(ui.integrations-panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=${CLIPCOACH_QT_TEST_PLATFORM}")

add_executable(rs256-token-verifier-test rs256-token-verifier-test.cpp)
target_link_libraries(
  rs256-token-verifier-test
  PRIVATE clipcoach_network Qt6::Core OpenSSL::Crypto
)
add_test(NAME ui.rs256-token-verifier COMMAND rs256-token-verifier-test)

add_executable(qt-license-api-test qt-license-api-test.cpp)
target_link_libraries(qt-license-api-test PRIVATE clipcoach_network Qt6::Core Qt6::Network)
add_test(NAME ui.qt-license-api COMMAND qt-license-api-test)

add_executable(qt-ai-api-test qt-ai-api-test.cpp ../../src/network/qt-ai-api.cpp)
target_include_directories(qt-ai-api-test PRIVATE ../../include)
target_compile_definitions(qt-ai-api-test PRIVATE CLIPX_ALLOW_INSECURE_LOCAL_API=1)
target_link_libraries(qt-ai-api-test PRIVATE clipcoach_core Qt6::Core Qt6::Network)
add_test(NAME ui.qt-ai-api COMMAND qt-ai-api-test)

add_executable(
  remote-clipper-client-test
  remote-clipper-client-test.cpp
  ../../src/network/remote-clipper-client.cpp
)
target_include_directories(remote-clipper-client-test PRIVATE ../../include)
target_compile_definitions(remote-clipper-client-test PRIVATE CLIPX_ALLOW_INSECURE_LOCAL_API=1)
target_link_libraries(remote-clipper-client-test PRIVATE clipcoach_core Qt6::Core Qt6::Network)
add_test(NAME ui.remote-clipper-client COMMAND remote-clipper-client-test)

add_executable(remote-result-outbox-test remote-result-outbox-test.cpp)
target_include_directories(remote-result-outbox-test PRIVATE ../../include)
target_link_libraries(remote-result-outbox-test PRIVATE clipcoach_network Qt6::Core)
add_test(NAME ui.remote-result-outbox COMMAND remote-result-outbox-test)
set_tests_properties(ui.remote-result-outbox PROPERTIES
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${Qt6_DIR}/../../../bin"
)

add_executable(remote-command-poller-test remote-command-poller-test.cpp)
target_link_libraries(remote-command-poller-test PRIVATE clipcoach_network Qt6::Core Qt6::Network)
add_test(NAME ui.remote-command-poller COMMAND remote-command-poller-test)

add_executable(
  remote-capture-coordinator-test
  remote-capture-coordinator-test.cpp
  ../../src/plugin/remote-capture-coordinator.cpp
)
target_include_directories(remote-capture-coordinator-test PRIVATE ../../include)
target_link_libraries(remote-capture-coordinator-test PRIVATE clipcoach_core Qt6::Core)
add_test(NAME ui.remote-capture-coordinator COMMAND remote-capture-coordinator-test)

add_executable(
  qt-update-service-test
  qt-update-service-test.cpp
  ../../src/network/qt-update-checker.cpp
  ../../src/network/qt-update-downloader.cpp
)
target_include_directories(qt-update-service-test PRIVATE ../../include)
target_compile_definitions(qt-update-service-test PRIVATE CLIPX_ALLOW_INSECURE_LOCAL_API=1)
target_link_libraries(qt-update-service-test PRIVATE Qt6::Core Qt6::Network)
add_test(NAME ui.qt-update-service COMMAND qt-update-service-test)

add_executable(pro-account-tab-test pro-account-tab-test.cpp)
target_link_libraries(
  pro-account-tab-test
  PRIVATE clipcoach_ui clipcoach_licensing Qt6::Core Qt6::Widgets
)
add_test(NAME ui.pro-account-tab COMMAND pro-account-tab-test)
set_tests_properties(
  ui.pro-account-tab
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=${CLIPCOACH_QT_TEST_PLATFORM}"
)

if(WIN32)
  get_target_property(_clipcoach_qt_core_dll Qt6::Core IMPORTED_LOCATION_RELEASE)
  if(NOT _clipcoach_qt_core_dll)
    get_target_property(_clipcoach_qt_core_dll Qt6::Core IMPORTED_LOCATION)
  endif()
  get_filename_component(_clipcoach_qt_bin_dir "${_clipcoach_qt_core_dll}" DIRECTORY)
  get_filename_component(_clipcoach_qt_root_dir "${_clipcoach_qt_bin_dir}" DIRECTORY)
  set(_clipcoach_qt_platform_dir "${_clipcoach_qt_root_dir}/plugins/platforms")
  set(
    _clipcoach_ui_tests
    ui.components-construction
    ui.wheel-safe-controls
    ui.preview-interaction-gate
    ui.main-dock
    ui.capture-tab
    ui.clips-library-tab
    ui.clips-tab-smoke
    ui.settings-tab-smoke
    ui.localization-keys
    ui.main-dock-render
    ui.vertical-tab
    ui.triggers-tab
    ui.integrations-panel
    ui.rs256-token-verifier
    ui.qt-license-api
    ui.qt-ai-api
    ui.remote-clipper-client
    ui.remote-command-poller
    ui.remote-capture-coordinator
    ui.qt-update-service
    ui.pro-account-tab
  )
  set_tests_properties(
    ${_clipcoach_ui_tests}
    PROPERTIES
      ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_clipcoach_qt_bin_dir};QT_QPA_PLATFORM_PLUGIN_PATH=set:${_clipcoach_qt_platform_dir}"
  )
endif()
