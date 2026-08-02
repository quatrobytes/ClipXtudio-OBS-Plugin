add_executable(native-capture-library-flow-test native-capture-library-flow-test.cpp)
target_link_libraries(native-capture-library-flow-test PRIVATE clipcoach_core clipcoach_storage)
add_test(NAME integration.native-capture-library-flow COMMAND native-capture-library-flow-test)
set_tests_properties(integration.native-capture-library-flow PROPERTIES TIMEOUT 10)
