add_executable(library-performance-test library-performance-test.cpp)
target_link_libraries(library-performance-test PRIVATE clipcoach_core)
add_test(NAME performance.library-10000 COMMAND library-performance-test)
set_tests_properties(performance.library-10000 PROPERTIES LABELS performance TIMEOUT 10)
