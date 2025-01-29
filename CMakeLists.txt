cmake_minimum_required(VERSION 3.10)
project(pw-sine C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

add_executable(pw-sine pw-sine.c)

target_include_directories(pw-sine PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
target_link_libraries(pw-sine PRIVATE ${PIPEWIRE_LIBRARIES} m)
target_compile_options(pw-sine PRIVATE ${PIPEWIRE_CFLAGS_OTHER})

target_compile_options(pw-sine PRIVATE -Wall -Wextra)
