set(MODULE_NAME bcmtool)

# 纯 C 静态库，无 Qt 依赖
# 关闭从父项目继承的 AUTOMOC/AUTORCC/AUTOUIC，避免无谓扫描产生 mocs_compilation
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)

file(GLOB C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB C_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

add_library(${MODULE_NAME} STATIC ${C_SOURCES} ${C_HEADERS})

target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
    target_link_libraries(${MODULE_NAME} PUBLIC ws2_32)
else()
    target_link_libraries(${MODULE_NAME} PUBLIC -pthread)
endif()
