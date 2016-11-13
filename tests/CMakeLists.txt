find_package(Qt5 COMPONENTS Core Test QUIET)

if(Qt5Core_FOUND)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_INCLUDE_CURRENT_DIR ON)

  add_executable(bench_stream bench/bench_stream.cpp)
  target_link_libraries(bench_stream lbu_core Qt5::Core Qt5::Test)
  set_property(TARGET bench_stream PROPERTY CXX_STANDARD 14)
endif()


