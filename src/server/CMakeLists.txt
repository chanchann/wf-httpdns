add_executable(httpdns main.cc HDFactory.cc HDService.cc HttpUtil.cc)
target_link_libraries(httpdns workflow spdlog::spdlog)
