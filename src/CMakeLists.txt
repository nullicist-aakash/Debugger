add_library(libsdb process.cpp pipe.cpp registers.cpp
        ../include/libsdb/parse.hpp)
add_library(sdb::libsdb ALIAS libsdb)

set_target_properties(
        libsdb
        PROPERTIES OUTPUT_NAME sdb
)

target_compile_features(libsdb PUBLIC cxx_std_23)

target_include_directories(
        libsdb
        PRIVATE include
        PUBLIC ${CMAKE_SOURCE_DIR}/include
)