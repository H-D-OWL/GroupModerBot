cmake_minimum_required(VERSION 3.21)
project(GroupModerBot LANGUAGES CXX)

if(NOT DEFINED ENV{VCPKG_ROOT_CUSTOM})
    message(STATUS "VCPKG_ROOT_CUSTOM is not set")
else()
    # Outputting the value VCPKG_ROOT_CUSTOM to output
    message(STATUS "VCPKG_ROOT_CUSTOM: $ENV{VCPKG_ROOT_CUSTOM}")
endif()

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

# Setting the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------
# SEARCH FOR DEPENDENCIES
# ---------------------------------------------------------

find_package(Boost COMPONENTS system REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(SQLiteCpp REQUIRED)
find_package(TgBot REQUIRED)
find_package(Taskflow REQUIRED)

# ---------------------------------------------------------
# TARGET ASSEMBLY
# ---------------------------------------------------------

set(SOURCE_FILES 
    "${PROJECT_NAME}/GroupModerBot.cpp"
    "${PROJECT_NAME}/BotController.cpp" 
    "${PROJECT_NAME}/BotDatabase.cpp"
    "${PROJECT_NAME}/Logging.cpp"
    "${PROJECT_NAME}/ConfigManager.cpp"
)

add_executable("${PROJECT_NAME}" ${SOURCE_FILES} )

# ---------------------------------------------------------
# COMPILATION AND OPTIMIZATION
# ---------------------------------------------------------

if(MSVC)
    target_compile_options("${PROJECT_NAME}" PRIVATE 
        /W4 /permissive- /WX
        $<$<CONFIG:Release>:/O2 /Ob2 /Oi /Ot /Oy /GT /DNDEBUG>
    )
else()
    target_compile_options("${PROJECT_NAME}" PRIVATE 
        -Wall -Wextra -pedantic -Werror
        $<$<CONFIG:Release>:-O3 -msse4.2 -fno-plt -DNDEBUG>
    )
endif()

# Global IPO (LTO) activation
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)

if(ipo_supported)
    set_property(DIRECTORY PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

# ---------------------------------------------------------
# LINKING
# ---------------------------------------------------------

target_link_libraries("${PROJECT_NAME}" PRIVATE 
    Taskflow::Taskflow
    SQLiteCpp
    TgBot::TgBot
    Boost::system
    CURL::libcurl
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

if(MSVC)    
    # The built-in module looks for vcruntime140.dll, msvcp140.dll, etc.
    include(InstallRequiredSystemLibraries)

    # We take the list of found DLLs and copy them to the folder next to the .exe after assembly
    foreach(lib ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
        add_custom_command(TARGET "${PROJECT_NAME}" POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${lib}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        )
    endforeach()
endif()

# ---------------------------------------------------------
# CONFIGURATION FILE COPY (OPTIONAL)
# ---------------------------------------------------------

set(CONFIG_FILE_NAME "DataForBot.txt")
set(CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/${CONFIG_FILE_NAME}")
set(CONFIG_BUILD_FILE "${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE_NAME}")

if(EXISTS ${CONFIG_FILE})
    add_custom_command(
        OUTPUT "${CONFIG_BUILD_FILE}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CONFIG_FILE}" "${CONFIG_BUILD_FILE}"
    )

    target_sources("${PROJECT_NAME}" PRIVATE "${CONFIG_BUILD_FILE}")

    message(STATUS "${CONFIG_FILE_NAME} found. It will be copied to the build directory.")
else()
    message(STATUS "${CONFIG_FILE_NAME} not found. Skipping copy step.")
endif()
