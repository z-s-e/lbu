cmake_minimum_required(VERSION 3.4)
project(lbu)

set(LIBLBU_VERSION_MAJOR 0)
set(LIBLBU_VERSION_MINOR 1)
set(LIBLBU_VERSION_PATCH 1)
set(LIBLBU_VERSION "${LIBLBU_VERSION_MAJOR}.${LIBLBU_VERSION_MINOR}.${LIBLBU_VERSION_PATCH}")

set(LIB_INSTALL_DIR lib)
set(LIB_STATIC_INSTALL_DIR lib/static)
set(CMAKECONFIG_INSTALL_DIR lib/cmake/${PROJECT_NAME})
set(INCLUDE_IMPORT_DIR include)
set(INCLUDE_INSTALL_DIR ${INCLUDE_IMPORT_DIR}/${PROJECT_NAME})
find_package(PkgConfig REQUIRED)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wmissing-declarations -Wmissing-include-dirs -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wsign-conversion -Wsign-promo -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnoexcept -Wstrict-null-sentinel -Wlogical-op")
endif()

option(LBU_BUILD_STATIC "Build lbu as a static library" OFF)


# --- TARGETS

## Core

set(lbu_core_src
    core/abstract_stream.cpp
    core/align.cpp
    core/array_ref.cpp
    core/buffer_stream.cpp
    core/byte_buffer.cpp
    core/c_function.cpp
    core/core.cpp
    core/endian.cpp
    core/eventfd.cpp
    core/fd.cpp
    core/fd_stream.cpp
    core/file.cpp
    core/io.cpp
    core/memory.cpp
    core/pipe.cpp
    core/poll.cpp
    core/ring_spsc.cpp
    core/ring_spsc_stream.cpp
    core/time.cpp
    core/xmalloc.cpp
)
set(lbu_core_hdr
    core/lbu/abstract_stream.h
    core/lbu/align.h
    core/lbu/array_ref.h
    core/lbu/buffer_stream.h
    core/lbu/byte_buffer.h
    core/lbu/c_function.h
    core/lbu/endian.h
    core/lbu/eventfd.h
    core/lbu/fd.h
    core/lbu/fd_stream.h
    core/lbu/file.h
    core/lbu/io.h
    core/lbu/memory.h
    core/lbu/pipe.h
    core/lbu/poll.h
    core/lbu/ring_spsc.h
    core/lbu/ring_spsc_stream.h
    core/lbu/time.h
    core/lbu/xmalloc.h
)

if(LBU_BUILD_STATIC)
    add_library(lbu_core STATIC ${lbu_core_src})
else()
    add_library(lbu_core SHARED ${lbu_core_src})
    set_target_properties(lbu_core PROPERTIES
                          SOVERSION ${LIBLBU_VERSION_MAJOR}
                          VERSION ${LIBLBU_VERSION})
endif()

add_custom_target(lbu_core_header SOURCES ${lbu_core_hdr})

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/core/lbu_global.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/include/lbu/lbu_global.h
)
set_property(TARGET lbu_core
             PROPERTY PUBLIC_HEADER
             ${lbu_core_hdr}
             ${CMAKE_CURRENT_BINARY_DIR}/include/lbu/lbu_global.h)

set_property(TARGET lbu_core PROPERTY CXX_STANDARD 14)
set_property(TARGET lbu_core PROPERTY CXX_STANDARD_REQUIRED 1)

target_include_directories(lbu_core PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
                           $<INSTALL_INTERFACE:${INCLUDE_IMPORT_DIR}>)


## Alsa

set(lbu_alsa_src
  alsa/alsa.cpp
)
set(lbu_alsa_hdr
  alsa/lbu/alsa/alsa.h
)

if(LBU_BUILD_STATIC)
    add_library(lbu_alsa STATIC ${lbu_alsa_src})
else()
    add_library(lbu_alsa SHARED ${lbu_alsa_src})
    set_target_properties(lbu_alsa PROPERTIES
                          SOVERSION ${LIBLBU_VERSION_MAJOR}
                          VERSION ${LIBLBU_VERSION})
endif()

add_custom_target(lbu_alsa_header SOURCES ${lbu_alsa_hdr})
set_property(TARGET lbu_alsa
             PROPERTY PUBLIC_HEADER
             ${lbu_alsa_hdr})

set_property(TARGET lbu_alsa PROPERTY CXX_STANDARD 14)
set_property(TARGET lbu_alsa PROPERTY CXX_STANDARD_REQUIRED 1)

pkg_search_module(LIB_ALSA REQUIRED alsa)
target_link_libraries(lbu_alsa PUBLIC lbu_core ${LIB_ALSA_LIBRARIES})
target_compile_options(lbu_alsa PUBLIC ${LIB_ALSA_CFLAGS})

target_include_directories(lbu_alsa PUBLIC
                           ${LIB_ALSA_INCLUDE_DIRS}
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/alsa>
                           $<INSTALL_INTERFACE:${INCLUDE_IMPORT_DIR}>)


# --- TESTS

enable_testing()
add_subdirectory(tests)

# --- INSTALL

install(TARGETS lbu_core EXPORT lbuTargets
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_STATIC_INSTALL_DIR}
        PUBLIC_HEADER DESTINATION ${INCLUDE_INSTALL_DIR})

install(TARGETS lbu_alsa EXPORT lbuTargets
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_STATIC_INSTALL_DIR}
        PUBLIC_HEADER DESTINATION ${INCLUDE_INSTALL_DIR}/alsa)


include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/lbu-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/lbu-config.cmake
  INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/lbu-config-version.cmake
  VERSION ${LIBLBU_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/lbu-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/lbu-config-version.cmake
  DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)

install(
  EXPORT lbuTargets
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  FILE lbuTargets.cmake
)
