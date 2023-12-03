project(handler)
add_executable(sample 
AHandler.cpp
ALooper.cpp
ALooperRoster.cpp
AMessage.cpp
Errors.cpp
main.cpp
Thread.cpp)
target_link_libraries(sample fmt)