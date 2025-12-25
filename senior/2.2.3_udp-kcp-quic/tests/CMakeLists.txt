add_executable(test_kcp test.cpp)
target_link_libraries(test_kcp PRIVATE kcp::kcp)
target_compile_features(test_kcp PRIVATE cxx_std_11)
target_include_directories(test_kcp PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_test(NAME test_kcp COMMAND test_kcp)
