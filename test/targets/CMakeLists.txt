file(GLOB TEST_HELPER_SOURCES "*.cpp")

set(TEST_HELPER_TARGETS "")

foreach(source_file IN LISTS TEST_HELPER_SOURCES)
    get_filename_component(target_name "${source_file}" NAME_WE)
    add_executable(${target_name} ${source_file})
    list(APPEND TEST_HELPER_TARGETS ${target_name})
endforeach()

# Export the list to the parent scope
set(TEST_HELPER_TARGETS "${TEST_HELPER_TARGETS}" PARENT_SCOPE)
