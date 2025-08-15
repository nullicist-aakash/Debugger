file(GLOB TEST_HELPER_CPP_SOURCES "*.cpp")
file(GLOB TEST_HELPER_ASM_SOURCES "*.s")

set(TEST_HELPER_TARGETS "")

foreach(source_file IN LISTS TEST_HELPER_CPP_SOURCES)
    get_filename_component(target_name "${source_file}" NAME_WE)
    add_executable(${target_name} ${source_file})
    list(APPEND TEST_HELPER_TARGETS ${target_name})
endforeach()


foreach(source_file IN LISTS TEST_HELPER_ASM_SOURCES)
    get_filename_component(target_name "${source_file}" NAME_WE)
    add_executable(${target_name} ${source_file})
    target_compile_options(${target_name} PRIVATE -pie)
    list(APPEND TEST_HELPER_TARGETS ${target_name})
endforeach()

# Export the list to the parent scope
set(TEST_HELPER_TARGETS "${TEST_HELPER_TARGETS}" PARENT_SCOPE)
