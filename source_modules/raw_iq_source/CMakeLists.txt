cmake_minimum_required(VERSION 3.13)
project(raw_iq_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(raw_iq_source PRIVATE "src/")
