cmake_minimum_required(VERSION 3.12)
project(lbu)

set(LIBLBU_VERSION_MAJOR 0)
set(LIBLBU_VERSION_MINOR 1)
set(LIBLBU_VERSION_PATCH 12)
set(LIBLBU_VERSION "${LIBLBU_VERSION_MAJOR}.${LIBLBU_VERSION_MINOR}.${LIBLBU_VERSION_PATCH}")

option(LBU_BUILD_STATIC "Build lbu as a static library" OFF)
option(LBU_BUILD_ALSA "Build lbu alsa library" ON)
option(LBU_BUILD_TESTS "Build lbu tests" ON)

include(GNUInstallDirs)
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
set(LIB_STATIC_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
set(CMAKECONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    CACHE STRING "Path to lbu cmake files")
set(INCLUDE_IMPORT_DIR ${CMAKE_INSTALL_INCLUDEDIR})
set(INCLUDE_INSTALL_DIR ${INCLUDE_IMPORT_DIR}/${PROJECT_NAME})

find_package(PkgConfig REQUIRED)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wmissing-declarations -Wmissing-include-dirs -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wsign-conversion -Wsign-promo -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused -Wno-type-limits")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnoexcept -Wstrict-null-sentinel -Wlogical-op")
endif()

# --- TARGETS

## Core

set(lbu_core_src
    core/abstract_stream.cpp
    core/array_ref.cpp
    core/ascii.cpp
    core/bit_ops.cpp
    core/byte_buffer_stream.cpp
    core/byte_buffer.cpp
    core/core.cpp
    core/dynamic_memory.cpp
    core/endian.cpp
    core/eventfd.cpp
    core/fd.cpp
    core/fd_stream.cpp
    core/file.cpp
    core/io.cpp
    core/math.cpp
    core/memory.cpp
    core/pipe.cpp
    core/poll.cpp
    core/ring_spsc.cpp
    core/ring_spsc_stream.cpp
    core/unexpected.cpp
)
set(lbu_core_hdr
    core/lbu/abstract_stream.h
    core/lbu/array_ref.h
    core/lbu/ascii.h
    core/lbu/bit_ops.h
    core/lbu/byte_buffer_stream.h
    core/lbu/byte_buffer.h
    core/lbu/dynamic_memory.h
    core/lbu/endian.h
    core/lbu/eventfd.h
    core/lbu/fd.h
    core/lbu/fd_stream.h
    core/lbu/file.h
    core/lbu/io.h
    core/lbu/math.h
    core/lbu/memory.h
    core/lbu/pipe.h
    core/lbu/poll.h
    core/lbu/ring_spsc.h
    core/lbu/ring_spsc_stream.h
    core/lbu/unexpected.h
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

target_compile_features(lbu_core PUBLIC cxx_std_17)

target_include_directories(lbu_core PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
                           $<INSTALL_INTERFACE:${INCLUDE_IMPORT_DIR}>)

install(TARGETS lbu_core EXPORT lbuTargets
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
                COMPONENT lbu_Runtime
                NAMELINK_COMPONENT lbu_Development
        ARCHIVE DESTINATION ${LIB_STATIC_INSTALL_DIR}
                COMPONENT lbu_Development
        PUBLIC_HEADER DESTINATION ${INCLUDE_INSTALL_DIR}
                COMPONENT lbu_Development
       )


## Alsa

set(lbu_alsa_src
  alsa/alsa.cpp
)
set(lbu_alsa_hdr
  alsa/lbu/alsa/alsa.h
)

if(LBU_BUILD_ALSA)
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

    pkg_check_modules(LIB_ALSA REQUIRED IMPORTED_TARGET alsa)
    target_link_libraries(lbu_alsa PUBLIC lbu_core PkgConfig::LIB_ALSA)

    target_include_directories(lbu_alsa PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/alsa>
                            $<INSTALL_INTERFACE:${INCLUDE_IMPORT_DIR}>)
    
    install(TARGETS lbu_alsa EXPORT lbuTargets
            LIBRARY DESTINATION ${LIB_INSTALL_DIR}
                    COMPONENT lbu_Runtime
                    NAMELINK_COMPONENT lbu_Development
            ARCHIVE DESTINATION ${LIB_STATIC_INSTALL_DIR}
                    COMPONENT lbu_Development
            PUBLIC_HEADER DESTINATION ${INCLUDE_INSTALL_DIR}/alsa
                    COMPONENT lbu_Development
           )
endif()

# --- TESTS

if(LBU_BUILD_TESTS)
    find_package(Qt5 COMPONENTS Core Test REQUIRED)
    enable_testing()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_INCLUDE_CURRENT_DIR ON)

    add_executable(bench_stream tests/bench/bench_stream.cpp)
    target_link_libraries(bench_stream lbu_core Qt5::Core Qt5::Test)
    target_compile_definitions(bench_stream PRIVATE BENCH_RING_ADDITIONAL)

    add_executable(test_byte_buffer tests/auto/test_byte_buffer.cpp)
    target_link_libraries(test_byte_buffer lbu_core Qt5::Core Qt5::Test)
    add_test(NAME test_byte_buffer COMMAND test_byte_buffer)
endif()


# --- CMAKE INSTALL

include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/lbu-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/lbu-config.cmake
  INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/lbu-config-version.cmake
  VERSION ${LIBLBU_VERSION}
  COMPATIBILITY ExactVersion # TODO change to SameMajorVersion once stable
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/lbu-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/lbu-config-version.cmake
  DESTINATION ${CMAKECONFIG_INSTALL_DIR}
  COMPONENT lbu_Development
)

install(
  EXPORT lbuTargets
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  FILE lbuTargets.cmake
  COMPONENT lbu_Development
)
