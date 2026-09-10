# Set minimum CMake version
cmake_minimum_required(VERSION "3.21")

# Configure project and languages
project(libjoybus-mgmt LANGUAGES C)

# Wire protocol definitions only, link this from firmware and host apps
add_library(joybus_mgmt INTERFACE)
target_include_directories(joybus_mgmt INTERFACE include)

# libjoybus management target, link this from device firmware
add_library(joybus_mgmt_target INTERFACE)
target_sources(joybus_mgmt_target INTERFACE src/target.c)
target_link_libraries(joybus_mgmt_target INTERFACE joybus_mgmt joybus)

# N64 (libdragon) host client interface, link this from N64 homebrew
add_library(joybus_mgmt_n64 INTERFACE)
target_sources(joybus_mgmt_n64 INTERFACE src/host/host.c src/host/n64.c)
target_link_libraries(joybus_mgmt_n64 INTERFACE joybus_mgmt)
