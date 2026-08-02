add_executable(default-settings-test default-settings-test.cpp)
target_link_libraries(default-settings-test PRIVATE clipcoach_core)
add_test(NAME unit.default-settings COMMAND default-settings-test)

add_executable(settings-manager-test settings-manager-test.cpp)
target_link_libraries(settings-manager-test PRIVATE clipcoach_core)
add_test(NAME unit.settings-manager COMMAND settings-manager-test)

add_executable(settings-validation-test settings-validation-test.cpp)
target_link_libraries(settings-validation-test PRIVATE clipcoach_core)
add_test(NAME unit.settings-validation COMMAND settings-validation-test)

add_executable(settings-migration-test settings-migration-test.cpp)
target_link_libraries(settings-migration-test PRIVATE clipcoach_core)
add_test(NAME unit.settings-migration COMMAND settings-migration-test)

add_executable(hotkey-manager-test hotkey-manager-test.cpp)
target_link_libraries(hotkey-manager-test PRIVATE clipcoach_core)
add_test(NAME unit.hotkey-manager COMMAND hotkey-manager-test)

add_executable(vertical-canvas-manager-test vertical-canvas-manager-test.cpp)
target_link_libraries(vertical-canvas-manager-test PRIVATE clipcoach_core)
add_test(NAME unit.vertical-canvas-manager COMMAND vertical-canvas-manager-test)

add_executable(export-manager-test export-manager-test.cpp)
target_link_libraries(export-manager-test PRIVATE clipcoach_core)
add_test(NAME unit.export-manager COMMAND export-manager-test)

add_executable(score-engine-test score-engine-test.cpp)
target_link_libraries(score-engine-test PRIVATE clipcoach_core)
add_test(NAME unit.score-engine COMMAND score-engine-test)

add_executable(trigger-engine-test trigger-engine-test.cpp)
target_link_libraries(trigger-engine-test PRIVATE clipcoach_core)
add_test(NAME unit.trigger-engine COMMAND trigger-engine-test)

add_executable(speech-engine-test speech-engine-test.cpp)
target_link_libraries(speech-engine-test PRIVATE clipcoach_core)
add_test(NAME unit.speech-engine COMMAND speech-engine-test)

add_executable(voice-trigger-service-test voice-trigger-service-test.cpp)
target_link_libraries(voice-trigger-service-test PRIVATE clipcoach_core)
add_test(NAME unit.voice-trigger-service COMMAND voice-trigger-service-test)

add_executable(voice-audio-segmenter-test voice-audio-segmenter-test.cpp)
target_link_libraries(voice-audio-segmenter-test PRIVATE clipcoach_core)
add_test(NAME unit.voice-audio-segmenter COMMAND voice-audio-segmenter-test)

add_executable(chat-pulse-test chat-pulse-test.cpp)
target_link_libraries(chat-pulse-test PRIVATE clipcoach_core)
add_test(NAME unit.chat-pulse COMMAND chat-pulse-test)

add_executable(chat-integration-test chat-integration-test.cpp)
target_link_libraries(chat-integration-test PRIVATE clipcoach_core)
add_test(NAME unit.chat-integration COMMAND chat-integration-test)

add_executable(machine-fingerprint-test machine-fingerprint-test.cpp)
target_link_libraries(machine-fingerprint-test PRIVATE clipcoach_licensing)
add_test(NAME unit.machine-fingerprint COMMAND machine-fingerprint-test)

add_executable(license-manager-test license-manager-test.cpp)
target_link_libraries(license-manager-test PRIVATE clipcoach_licensing)
add_test(NAME unit.license-manager COMMAND license-manager-test)

add_executable(feature-gate-service-test feature-gate-service-test.cpp)
target_link_libraries(feature-gate-service-test PRIVATE clipcoach_core)
add_test(NAME unit.feature-gate-service COMMAND feature-gate-service-test)

add_executable(ai-assistant-test ai-assistant-test.cpp)
target_link_libraries(ai-assistant-test PRIVATE clipcoach_core)
add_test(NAME unit.ai-assistant COMMAND ai-assistant-test)

add_executable(clip-duration-test clip-duration-test.cpp)
target_link_libraries(clip-duration-test PRIVATE clipcoach_core)
add_test(NAME unit.clip-duration COMMAND clip-duration-test)

add_executable(clip-filename-test clip-filename-test.cpp)
target_link_libraries(clip-filename-test PRIVATE clipcoach_core)
add_test(NAME unit.clip-filename COMMAND clip-filename-test)

add_executable(clip-manager-test clip-manager-test.cpp)
target_link_libraries(clip-manager-test PRIVATE clipcoach_core)
add_test(NAME unit.clip-manager COMMAND clip-manager-test)

add_executable(remote-command-executor-test remote-command-executor-test.cpp)
target_link_libraries(remote-command-executor-test PRIVATE clipcoach_core)
add_test(NAME unit.remote-command-executor COMMAND remote-command-executor-test)

add_executable(sqlite-database-test sqlite-database-test.cpp)
target_link_libraries(sqlite-database-test PRIVATE clipcoach_storage)
add_test(NAME unit.sqlite-database COMMAND sqlite-database-test)

add_executable(database-recovery-test database-recovery-test.cpp)
target_link_libraries(database-recovery-test PRIVATE clipcoach_storage)
add_test(NAME unit.database-recovery COMMAND database-recovery-test)

add_executable(repositories-test repositories-test.cpp)
target_link_libraries(repositories-test PRIVATE clipcoach_storage)
add_test(NAME unit.repositories COMMAND repositories-test)

add_executable(export-job-repository-test export-job-repository-test.cpp)
target_link_libraries(export-job-repository-test PRIVATE clipcoach_storage)
add_test(NAME unit.export-job-repository COMMAND export-job-repository-test)

add_executable(migrations-test migrations-test.cpp)
target_link_libraries(migrations-test PRIVATE clipcoach_storage SQLite::SQLite3)
add_test(NAME unit.migrations COMMAND migrations-test)

add_executable(clip-library-service-test clip-library-service-test.cpp)
target_link_libraries(clip-library-service-test PRIVATE clipcoach_storage)
add_test(NAME unit.clip-library-service COMMAND clip-library-service-test)

add_executable(clip-library-view-model-test clip-library-view-model-test.cpp)
target_link_libraries(clip-library-view-model-test PRIVATE clipcoach_core)
add_test(NAME unit.clip-library-view-model COMMAND clip-library-view-model-test)
