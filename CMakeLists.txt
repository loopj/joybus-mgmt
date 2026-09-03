# Set minimum CMake version
cmake_minimum_required(VERSION "3.21")

# Configure project and languages
project(libjoybus-mgmt LANGUAGES C)

# Wire protocol definitions only, with no dependencies.
# Link this from homebrew and host tools.
add_library(joybus_mgmt INTERFACE)
target_include_directories(joybus_mgmt INTERFACE include)

# Generic management target, dispatching commands to device-supplied groups.
# Link this from device firmware.
add_library(joybus_mgmt_target INTERFACE)
target_sources(joybus_mgmt_target INTERFACE src/target.c)
target_link_libraries(joybus_mgmt_target INTERFACE joybus_mgmt joybus)
