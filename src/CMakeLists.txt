# Executables
set(PRESIGN_SRCS presign_url.cpp aws_sign.cpp url_utility.cpp)
set(CMAKE_CXX_FLAGS "-std=c++17" ${CMAKE_CXX_FLAGS})
add_executable("s3-presign" ${PRESIGN_SRCS})
add_dependencies("s3-presign" ${DEPENDENCIES})
target_link_libraries("s3-presign" ${LIBRARIES})