cmake_minimum_required(VERSION 3.15)
if(${CMAKE_VERSION} VERSION_LESS 3.15)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(s3-url-presign 
			VERSION 1.0
            DESCRIPTION "Pre-signing of AWS URLs"
            LANGUAGES CXX)


# Variables to be filled by dependent sub-projects

	set(DEPENDENCIES "")
	set(LIBRARIES "")
 
# Separate debug-release paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/release)

# Dependencies
    
    include_directories(${CMAKE_SOURCE_DIR}/dep/hash)
    add_subdirectory(dep/hash)
    
    include_directories(${CMAKE_SOURCE_DIR}/dep/Lyra/include)

    include_directories(${CMAKE_SOURCE_DIR}/include)
	add_subdirectory(src)