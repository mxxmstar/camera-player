cmake_minimum_required(VERSION 3.16)
project(ai-camera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
    # 强制替换 /Zi /ZI 为 /Z7，将调试信息嵌入 .obj 文件，避免 PDB 文件锁冲突（C1041）
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/ZI" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "Debug flags" FORCE)
endif()

# ============================================================
# Build options
# ============================================================
option(BUILD_TESTS "Build test programs" OFF)

# ============================================================
# ASIO (header-only)
# ============================================================
set(ASIO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/asio-1.36.0)

# ============================================================
# spdlog (header-only)
# ============================================================
set(SPDLOG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/spdlog-1.17.0/include)

# Define ASIO standalone (without Boost)
add_definitions(-DASIO_STANDALONE)

# ============================================================
# Main executable
# ============================================================
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.hpp
    src/*.h
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${ASIO_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
)

# Link pthreads (required by ASIO on Linux)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 mswsock)
endif()

# ============================================================
# Test programs (optional, controlled by BUILD_TESTS)
# ============================================================
if(BUILD_TESTS)
    file(GLOB_RECURSE TEST_SOURCES
        tests/*.cpp
        tests/*.hpp
        tests/*.h
    )

    if(TEST_SOURCES)
        # 测试程序复用主程序的所有源文件（包括 ws/http/rtsp 等模块实现），
        # 但排除主程序入口 src/main.cpp 以避免 main 符号冲突。
        set(TEST_SRC_SOURCES ${SOURCES})
        list(FILTER TEST_SRC_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

        add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES} ${TEST_SRC_SOURCES})

        target_include_directories(${PROJECT_NAME}_tests PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${ASIO_INCLUDE_DIR}
            ${SPDLOG_INCLUDE_DIR}
        )

        target_link_libraries(${PROJECT_NAME}_tests PRIVATE Threads::Threads)
        if(WIN32)
            target_link_libraries(${PROJECT_NAME}_tests PRIVATE ws2_32 mswsock)
        endif()
    endif()
endif()