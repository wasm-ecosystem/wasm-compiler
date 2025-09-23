
set(qnxArm64Options "-V8.3.0,gcc_ntoaarch64le -march=armv8.1-a")
add_definitions(-D_QNX_SOURCE)
set(CMAKE_SYSTEM_NAME QNX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${qnxArm64Options}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${qnxArm64Options}")