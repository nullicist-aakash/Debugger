add_library(libsdb
    process.cpp pipe.cpp registers.cpp breakpoint_site.cpp disassembler.cpp watchpoint.cpp
    syscalls.cpp elf.cpp contract_handler.cpp types.cpp target.cpp
)
add_library(sdb::libsdb ALIAS libsdb)

target_link_libraries(libsdb PRIVATE Zydis::Zydis)

set_target_properties(
    libsdb
    PROPERTIES OUTPUT_NAME sdb
)

target_compile_features(libsdb PUBLIC cxx_std_26)

target_include_directories(
    libsdb
    PRIVATE include
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)
