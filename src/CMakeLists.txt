# Executables
set(PRESIGN_SRCS presign_url.cpp aws_sign.cpp url_utility.cpp)
set(SIGN_HEADER_SRCS sign_header.cpp aws_sign.cpp url_utility.cpp)
set(PAR_UPLOAD_SRCS "parallel_upload.cpp" url_utility.cpp aws_sign.cpp webclient.cpp response_parser.cpp)
set(S3_CLIENT_SRCS "s3-client.cpp" url_utility.cpp aws_sign.cpp webclient.cpp)

set(CMAKE_CXX_FLAGS "-std=c++17" ${CMAKE_CXX_FLAGS})

add_executable("s3-presign" ${PRESIGN_SRCS})
add_executable("sign-headers" ${SIGN_HEADER_SRCS})
add_executable("s3-client" ${S3_CLIENT_SRCS})
add_executable("s3-upload" ${PAR_UPLOAD_SRCS})

add_dependencies("s3-presign" ${DEPENDENCIES})
target_link_libraries("s3-presign" ${LIBRARIES})

add_dependencies("sign-headers" ${DEPENDENCIES})
target_link_libraries("sign-headers" ${LIBRARIES})

add_dependencies("s3-client" ${DEPENDENCIES})
target_link_libraries("s3-client" ${LIBRARIES})
target_link_libraries("s3-client" curl)

add_dependencies("s3-upload" ${DEPENDENCIES})
target_link_libraries("s3-upload" ${LIBRARIES})
target_link_libraries("s3-upload" curl)