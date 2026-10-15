cmake_minimum_required(VERSION 3.21)
project(MinecraftPE)

option(TCPE_DEDICATED_SERVER "Build the headless native TCPE server only" OFF)
if(TCPE_DEDICATED_SERVER)
    include(cmake/NativeServer.cmake)
    return()
endif()

include(cmake/CPM.cmake)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "-Wno-c++11-narrowing -Wno-narrowing -Wno-invalid-source-encoding -Wno-reserved-user-defined-literal")
endif()

option(TCPE_GLES3 "Use genuine GLES3 shader compatibility renderer" ON)
option(TCPE_USE_SYSTEM_LIBS "Use installed Linux platform libraries" ${UNIX})
if(TCPE_USE_SYSTEM_LIBS)
    find_package(ZLIB REQUIRED)
    find_package(PNG REQUIRED)
    find_package(OpenAL REQUIRED)
    find_package(glfw3 REQUIRED)
    set(TCPE_PLATFORM_LIBRARIES ZLIB::ZLIB PNG::PNG OpenAL::OpenAL glfw)
else()
CPMAddPackage("gh:madler/zlib@1.3.2")
CPMAddPackage(
    NAME "libpng"
    GIT_REPOSITORY "https://github.com/pnggroup/libpng.git"
    GIT_TAG "v1.6.55"
    OPTIONS
        "ZLIB_ROOT ${zlib_SOURCE_DIR}"
        "ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR}"
        "PNG_TOOLS OFF"
        "PNG_TESTS OFF"
        "BUILD_SHARED_LIBS ON"
)

CPMAddPackage(
    NAME "openal"
    GIT_REPOSITORY "https://github.com/kcat/openal-soft.git"
    GIT_TAG "1.25.1"
    OPTIONS
        "ALSOFT_EXAMPLES OFF"
        "ALSOFT_TESTS OFF"
        "ALSOFT_UTILS OFF"
        "BUILD_SHARED_LIBS ON"
)

CPMAddPackage(
    NAME "glfw"
    GIT_REPOSITORY "https://github.com/glfw/glfw.git"
    GIT_TAG "3.4"
    OPTIONS
        "GLFW_BUILD_EXAMPLES OFF"
        "GLFW_BUILD_TESTS OFF"
        "GLFW_BUILD_DOCS OFF"
        "BUILD_SHARED_LIBS ON"
)

    set(TCPE_PLATFORM_LIBRARIES zlib png_shared alsoft.common OpenAL::OpenAL glfw)
endif()

file(GLOB SOURCES 
    "src/client/*.cpp"

    "src/client/gamemode/*.cpp"
    "src/client/gui/*.cpp"
    "src/client/gui/components/*.cpp"
    "src/client/gui/screens/*.cpp"
    "src/client/gui/screens/crafting/*.cpp"
    "src/client/gui/screens/touch/*.cpp"

    "src/client/model/*.cpp"
    "src/client/model/geom/*.cpp"

    "src/client/particle/*.cpp"

    "src/client/player/*.cpp"
    "src/client/player/input/*.cpp"
    "src/client/player/input/touchscreen/*.cpp"

    "src/client/renderer/*.cpp"
    "src/client/renderer/culling/*.cpp"
    "src/client/renderer/entity/*.cpp"
    "src/client/renderer/ptexture/*.cpp"
    "src/client/renderer/tileentity/*.cpp"

    "src/client/sound/*.cpp"
    
    "src/locale/*.cpp"
    
    "src/nbt/*.cpp"

    "src/network/*.cpp"
    "src/network/command/*.cpp"

    "src/platform/*.cpp"
    "src/platform/audio/SoundSystemAL.cpp"
    "src/platform/input/*.cpp"

    "src/raknet/**.cpp"
    
    "src/server/**.cpp"
    
    "src/util/**.cpp"

    "src/world/*.cpp"
    "src/world/phys/*.cpp"
    "src/world/entity/*.cpp"
    "src/world/entity/ai/control/*.cpp"
    "src/world/entity/animal/*.cpp"
    "src/world/entity/item/*.cpp"
    "src/world/entity/monster/*.cpp"
    "src/world/entity/player/*.cpp"
    "src/world/entity/projectile/*.cpp"

    "src/world/food/*.cpp"
    "src/world/inventory/*.cpp"

    "src/world/item/*.cpp"
    "src/world/item/crafting/*.cpp"

    "src/world/level/*.cpp"
    "src/world/level/biome/*.cpp"
    "src/world/level/chunk/*.cpp"
    "src/world/level/dimension/*.cpp"
    "src/world/level/levelgen/*.cpp"
    "src/world/level/levelgen/feature/*.cpp"
    "src/world/level/levelgen/synth/*.cpp"
    "src/world/level/material/*.cpp"
    "src/world/level/pathfinder/*.cpp"
    "src/world/level/storage/*.cpp"
    "src/world/level/tile/*.cpp"
    "src/world/level/tile/entity/*.cpp"

    "src/SharedConstants.cpp"
    "src/main.cpp"
    "src/NinecraftApp.cpp"
)

if(NOT DEFINED PLATFORM)
    set(PLATFORM "PLATFORM_GLFW")
endif()


if(PLATFORM STREQUAL "PLATFORM_WIN32")
    list(APPEND SOURCES "src/AppPlatform_win32.cpp")
endif()

if(PLATFORM STREQUAL "PLATFORM_GLFW")
    list(APPEND SOURCES "src/AppPlatform_glfw.cpp")
endif()

# Explicitly list files added after the initial glob so they are always included
list(APPEND SOURCES
    "src/client/gui/screens/ConsoleScreen.cpp"
    "src/client/gui/screens/UsernameScreen.cpp"
    "src/client/gui/screens/CreditsScreen.cpp"
)

if(NOT TCPE_GLES3)
    list(APPEND SOURCES "glad/src/glad.c")
endif()
add_executable(${PROJECT_NAME} ${SOURCES})
if(TCPE_GLES3)
    find_library(TCPE_GLES3_LIBRARY GLESv2 REQUIRED)
    target_compile_definitions(${PROJECT_NAME} PUBLIC TCPE_GLES3 GLFW_INCLUDE_NONE)
    target_link_libraries(${PROJECT_NAME} ${TCPE_GLES3_LIBRARY})
endif()

if(WIN32)
    set(EXTRA_LIBS "ws2_32")
    target_compile_definitions(${PROJECT_NAME} PUBLIC "_CRT_SECURE_NO_WARNINGS")
endif()

if(PLATFORM STREQUAL "PLATFORM_WIN32" OR PLATFORM STREQUAL "PLATFORM_GLFW")
    target_compile_definitions(${PROJECT_NAME} PUBLIC "PLATFORM_DESKTOP")
endif()

target_include_directories(${PROJECT_NAME} PUBLIC 
    "${CMAKE_SOURCE_DIR}/glad/include/"
    "${CMAKE_SOURCE_DIR}/src"
    "${openal_SOURCE_DIR}/include"
    "${glfw_SOURCE_DIR}/include"
    "lib/include"
)

target_compile_definitions(${PROJECT_NAME} PUBLIC "OPENGL_ES" "NO_EGL" ${PLATFORM})
target_link_libraries(${PROJECT_NAME} ${TCPE_PLATFORM_LIBRARIES} ${EXTRA_LIBS})

if (NOT UNIX)
add_custom_command(
    TARGET ${PROJECT_NAME} 
    POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}> 
        COMMAND_EXPAND_LISTS
)
endif()

add_custom_command(
    TARGET ${PROJECT_NAME} 
    POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data" $<TARGET_FILE_DIR:${PROJECT_NAME}>/data
)
# Direct native Java protocol support; no separate proxy process.
set(TCPE_JAVA_NETWORK_LIBRARY "" CACHE FILEPATH "Path to libtcpe_java_network shared library")
if(NOT TCPE_JAVA_NETWORK_LIBRARY)
    message(FATAL_ERROR "Set TCPE_JAVA_NETWORK_LIBRARY after building rust-network")
endif()
target_include_directories(MinecraftPE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(MinecraftPE "${TCPE_JAVA_NETWORK_LIBRARY}")

file(GLOB JAVA_ADAPTER_SOURCES CONFIGURE_DEPENDS "src/network/java/*.cpp")
target_sources(MinecraftPE PRIVATE ${JAVA_ADAPTER_SOURCES})
