add_executable(sdb sdb.cpp)

target_link_libraries(sdb PRIVATE sdb::libsdb PkgConfig::libedit)
target_link_libraries(sdb PRIVATE sdb::libsdb PkgConfig::libedit fmt::fmt)