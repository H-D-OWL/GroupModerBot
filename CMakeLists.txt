cmake_minimum_required(VERSION 3.21)
project(GroupModerBot LANGUAGES CXX)

if(NOT DEFINED ENV{VCPKG_ROOT_CUSTOM})
    message(FATAL_ERROR "VCPKG_ROOT_CUSTOM is not set")
else()
    # Outputting the value VCPKG_ROOT_CUSTOM to output
    message(STATUS "VCPKG_ROOT_CUSTOM: $ENV{VCPKG_ROOT_CUSTOM}")
endif()

# Setting the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------
# SEARCH FOR DEPENDENCIES
# ---------------------------------------------------------

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(SQLiteCpp REQUIRED)
find_package(TgBot REQUIRED)

# ---------------------------------------------------------
# TARGET ASSEMBLY
# ---------------------------------------------------------

set(SOURCE_FILES 
    GroupModerBot/GroupModerBot.cpp
    GroupModerBot/BotController.cpp 
    GroupModerBot/BotDatabase.cpp 
    GroupModerBot/Logging.cpp
)

add_executable(GroupModerBot ${SOURCE_FILES})

# ---------------------------------------------------------
# COMPILATION AND OPTIMIZATION
# ---------------------------------------------------------

if(MSVC)
    target_compile_options(GroupModerBot PRIVATE /W4 /permissive- /WX)
else()
    target_compile_options(GroupModerBot PRIVATE -Wall -Wextra -pedantic)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
    set_target_properties(GroupModerBot PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

# ---------------------------------------------------------
# LINKING
# ---------------------------------------------------------

target_link_libraries(GroupModerBot PRIVATE 
    SQLiteCpp
    TgBot::TgBot
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Threads::Threads
)

if(MSVC)    
    # The built-in module looks for vcruntime140.dll, msvcp140.dll, etc.
    include(InstallRequiredSystemLibraries)

    # We take the list of found DLLs and copy them to the folder next to the .exe after assembly
    foreach(lib ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
        add_custom_command(TARGET GroupModerBot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${lib}"
            "$<TARGET_FILE_DIR:GroupModerBot>"
        )
    endforeach()
endif()