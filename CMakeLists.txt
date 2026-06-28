cmake_minimum_required(VERSION 3.16)
project(camera-player LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

# ============================================================
# GLFW3 (prebuilt binaries)
# ============================================================
set(GLFW3_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/glfw-3.4.bin.WIN64/include)
if(MSVC)
    set(GLFW3_LIBRARY ${CMAKE_SOURCE_DIR}/third_party/glfw-3.4.bin.WIN64/lib-vc2022/glfw3.lib)
else()
    set(GLFW3_LIBRARY ${CMAKE_SOURCE_DIR}/third_party/glfw-3.4.bin.WIN64/lib-mingw-w64/libglfw3.a)
endif()

# ============================================================
# GLAD (generated loader)
# ============================================================
set(GLAD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/glad/include)
set(GLAD_SOURCE ${CMAKE_SOURCE_DIR}/third_party/glad/src/glad.c)

# ============================================================
# FFmpeg (prebuilt shared libraries)
# ============================================================
set(FFMPEG_ROOT ${CMAKE_SOURCE_DIR}/third_party/ffmpeg-8.1)
set(FFMPEG_INCLUDE_DIR ${FFMPEG_ROOT}/include)
set(FFMPEG_LIB_DIR ${FFMPEG_ROOT}/lib)

set(FFMPEG_LIBRARIES
    ${FFMPEG_LIB_DIR}/avcodec.lib
    ${FFMPEG_LIB_DIR}/avformat.lib
    ${FFMPEG_LIB_DIR}/avutil.lib
    ${FFMPEG_LIB_DIR}/swscale.lib
    ${FFMPEG_LIB_DIR}/swresample.lib
    ${FFMPEG_LIB_DIR}/avfilter.lib
    ${FFMPEG_LIB_DIR}/avdevice.lib
)

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

list(APPEND SOURCES ${GLAD_SOURCE})

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${ASIO_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
    ${GLFW3_INCLUDE_DIR}
    ${GLAD_INCLUDE_DIR}
    ${FFMPEG_INCLUDE_DIR}
)

# Link pthreads (required by ASIO on Linux)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 mswsock)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32 ${GLFW3_LIBRARY})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARIES})
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