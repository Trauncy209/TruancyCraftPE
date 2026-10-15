set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
file(GLOB_RECURSE SERVER_CORE CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nbt/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/raknet/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/server/*.cpp")
file(GLOB SERVER_UTIL "src/util/*.cpp" "src/locale/*.cpp")
list(FILTER SERVER_UTIL EXCLUDE REGEX "PerfRenderer.cpp$")
file(GLOB SERVER_PLATFORM "src/platform/*.cpp" "src/platform/input/*.cpp")
add_executable(tcpe-server ${SERVER_CORE} ${SERVER_PLATFORM} ${SERVER_UTIL}
    src/main_dedicated.cpp src/NinecraftApp.cpp src/SharedConstants.cpp
    src/client/Minecraft.cpp src/client/Options.cpp src/client/Option.cpp src/client/OptionStrings.cpp src/client/OptionsFile.cpp
    src/client/gamemode/GameMode.cpp src/client/gamemode/CreativeMode.cpp src/client/gamemode/SurvivalMode.cpp
    src/network/RakNetInstance.cpp src/network/ServerSideNetworkHandler.cpp src/network/Packet.cpp src/network/NetEventCallback.cpp
    src/network/command/CommandServer.cpp
)
target_compile_definitions(tcpe-server PRIVATE STANDALONE_SERVER)
target_include_directories(tcpe-server PRIVATE src glad/include ../include)
target_compile_options(tcpe-server PRIVATE -Wno-c++11-narrowing -Wno-narrowing -ffunction-sections -fdata-sections)
target_link_options(tcpe-server PRIVATE -Wl,--gc-sections)
target_link_libraries(tcpe-server PRIVATE ZLIB::ZLIB Threads::Threads)

