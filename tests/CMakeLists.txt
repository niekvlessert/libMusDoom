# Test programs

add_executable(test_api test_api.c)
target_link_libraries(test_api musdoom)
add_test(NAME test_api COMMAND test_api)
