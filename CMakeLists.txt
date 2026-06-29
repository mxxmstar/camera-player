cmake_minimum_required(VERSION 3.16)
project(camera-player LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>)
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

# ============================================================
# Qt6 (prebuilt shared libraries)
# ============================================================
set(QT6_ROOT ${CMAKE_SOURCE_DIR}/third_party/qt6)
set(QT6_INCLUDE_DIR ${QT6_ROOT}/include)
set(QT6_LIB_DIR ${QT6_ROOT}/lib)

# Enable Qt MOC for Q_OBJECT macro
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Set Qt6_DIR to help find_package locate Qt6
set(Qt6_DIR ${QT6_ROOT}/lib/cmake/Qt6)
set(Qt6Core_DIR ${QT6_ROOT}/lib/cmake/Qt6Core)
set(Qt6Gui_DIR ${QT6_ROOT}/lib/cmake/Qt6Gui)
set(Qt6Widgets_DIR ${QT6_ROOT}/lib/cmake/Qt6Widgets)
set(Qt6Network_DIR ${QT6_ROOT}/lib/cmake/Qt6Network)
set(Qt6OpenGL_DIR ${QT6_ROOT}/lib/cmake/Qt6OpenGL)

find_package(Qt6 COMPONENTS Core Gui Widgets Network OpenGL REQUIRED)

set(QT6_LIBRARIES
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::OpenGL
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
    include/*.h
    include/*.hpp
)

# Qt resource file
set(RESOURCE_FILES
    resource/resources.qrc
    resource/style.qss
)

list(APPEND SOURCES ${GLAD_SOURCE})

add_executable(${PROJECT_NAME} ${SOURCES} ${RESOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
    ${ASIO_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
    ${GLFW3_INCLUDE_DIR}
    ${GLAD_INCLUDE_DIR}
    ${FFMPEG_INCLUDE_DIR}
    ${QT6_INCLUDE_DIR}
)

# Link pthreads (required by ASIO on Linux)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 mswsock)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32 ${GLFW3_LIBRARY})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${QT6_LIBRARIES})

    # ============================================================
    # Copy DLLs to output directory after build (for VS2022/2026)
    # ============================================================
    set(FFMPEG_DLLS
        ${FFMPEG_ROOT}/bin/avcodec-62.dll
        ${FFMPEG_ROOT}/bin/avformat-62.dll
        ${FFMPEG_ROOT}/bin/avutil-60.dll
        ${FFMPEG_ROOT}/bin/swscale-9.dll
        ${FFMPEG_ROOT}/bin/swresample-6.dll
        ${FFMPEG_ROOT}/bin/avfilter-11.dll
        ${FFMPEG_ROOT}/bin/avdevice-62.dll
    )

    set(QT6_DLLS
        ${QT6_ROOT}/bin/Qt6Core.dll
        ${QT6_ROOT}/bin/Qt6Cored.dll
        ${QT6_ROOT}/bin/Qt6Gui.dll
        ${QT6_ROOT}/bin/Qt6Guid.dll
        ${QT6_ROOT}/bin/Qt6Widgets.dll
        ${QT6_ROOT}/bin/Qt6Widgetsd.dll
        ${QT6_ROOT}/bin/Qt6Network.dll
        ${QT6_ROOT}/bin/Qt6Networkd.dll
        ${QT6_ROOT}/bin/Qt6OpenGL.dll
        ${QT6_ROOT}/bin/Qt6OpenGLd.dll
    )

    foreach(dll ${FFMPEG_DLLS})
        if(EXISTS ${dll})
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${dll}
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
            )
        endif()
    endforeach()

    foreach(dll ${QT6_DLLS})
        if(EXISTS ${dll})
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${dll}
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
            )
        endif()
    endforeach()

    # Copy Qt plugins (platforms)
    if(EXISTS ${QT6_ROOT}/plugins/platforms)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${QT6_ROOT}/plugins/platforms
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms
        )
    endif()
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