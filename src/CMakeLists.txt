add_executable(httpdns server.cc)
target_link_libraries(httpdns workflow spdlog::spdlog)
