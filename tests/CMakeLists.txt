find_package(Qt5 COMPONENTS Core Test QUIET)

if(Qt5Core_FOUND)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_INCLUDE_CURRENT_DIR ON)

  add_executable(bench_stream bench/bench_stream.cpp)
  target_link_libraries(bench_stream lbu_core Qt5::Core Qt5::Test)
  set_property(TARGET bench_stream PROPERTY CXX_STANDARD 14)

  add_executable(test_byte_buffer auto/test_byte_buffer.cpp)
  target_link_libraries(test_byte_buffer lbu_core Qt5::Core Qt5::Test)
  set_property(TARGET test_byte_buffer PROPERTY CXX_STANDARD 14)
  add_test(NAME test_byte_buffer COMMAND test_byte_buffer)
endif()


