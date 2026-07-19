cmake_minimum_required(VERSION 3.16)
project(SimVisionSDK LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED IMPORTED_TARGET libzmq)
pkg_check_modules(OPENSSL REQUIRED IMPORTED_TARGET openssl)
find_package(nlohmann_json 3.2.0 REQUIRED)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(LIB_SOURCES
    src/common/Types.cpp
    src/common/Logger.cpp
    src/common/Parameter.cpp
    src/common/Response.cpp
    src/common/Config.cpp
    src/data/SPSCQueue.cpp
    src/data/ChannelManager.cpp
    src/data/DataPipeline.cpp
    src/data/DataPublisher.cpp
    src/datasource/SimDataSource.cpp
    src/capture/CaptureEngine.cpp
    src/api/SessionManager.cpp
    src/api/CommandHandler.cpp
    src/api/AdminServer.cpp
)

add_library(sim_vision_lib STATIC ${LIB_SOURCES})
target_link_libraries(sim_vision_lib
    PRIVATE
        PkgConfig::ZMQ
        PkgConfig::OPENSSL
        nlohmann_json::nlohmann_json
        pthread
)

add_executable(sim_vision_node src/main.cpp)
target_link_libraries(sim_vision_node PRIVATE sim_vision_lib)

enable_testing()
add_subdirectory(tests)

install(TARGETS sim_vision_node RUNTIME DESTINATION bin)
