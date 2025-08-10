add_executable(tests tests.cpp)
target_link_libraries(tests PRIVATE sdb::libsdb Catch2::Catch2WithMain)

add_subdirectory("targets")
add_dependencies(tests ${TEST_HELPER_TARGETS})
