# Example programs

add_executable(musdoom_player musdoom_player.c)
target_link_libraries(musdoom_player musdoom)

# Simple test player
add_executable(simple_test simple_test.c)
target_link_libraries(simple_test musdoom)
