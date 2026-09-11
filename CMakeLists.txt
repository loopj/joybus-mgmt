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

# libdragon host client interface, link this from N64 homebrew
add_library(joybus_mgmt_host_libdragon INTERFACE)
target_sources(joybus_mgmt_host_libdragon INTERFACE src/host/host.c src/host/libdragon.c)
target_link_libraries(joybus_mgmt_host_libdragon INTERFACE joybus_mgmt)

# libogc host client interface, link this from GameCube and Wii homebrew
add_library(joybus_mgmt_host_libogc INTERFACE)
target_sources(joybus_mgmt_host_libogc INTERFACE src/host/host.c src/host/libogc.c)
target_link_libraries(joybus_mgmt_host_libogc INTERFACE joybus_mgmt)
