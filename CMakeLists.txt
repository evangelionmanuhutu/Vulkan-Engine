cmake_minimum_required(VERSION 3.20)
project(Vulkan)

set(CMAKE_CXX_STANDARD 20)
set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(TP_DIR ${ROOT_DIR}/third_party)

if (WIN32)
    if(NOT DEFINED ENV{VULKAN_SDK})
        message(FATAL_ERROR
            ">> VULKAN_SDK environment variable not defined. Please install the Vulkan SDK and set the environment variable.")
    endif()
    
    set(VULKAN_INCLUDE_DIR $ENV{VULKAN_SDK}/Include)
    set(VULKAN_LIBRARY_DIR $ENV{VULKAN_SDK}/Lib)

    message(">> Platform: Win32")
    message(">> Vulkan include dir: ${VULKAN_INCLUDE_DIR}")
    message(">> Vulkan lib dir: ${VULKAN_LIBRARY_DIR}")

    # Enable Hot Reload for MSVC compilers if supported.
    if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT 
        $<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>
    )
    endif()
elseif (UNIX AND NOT APPLE)
    # Check if we're running in WSL
    execute_process(COMMAND uname -r OUTPUT_VARIABLE WSL_KERNEL_NAME)
    string(FIND "${WSL_KERNEL_NAME}" "Microsoft" WSL_FOUND)
    
    if (WSL_FOUND EQUAL -1)
        message(">> Platform: WSL")
    else()
        message(">> Platform: LINUX")
    endif()
endif()

include(third_party/cmakefiles/imgui.cmake)
include(third_party/cmakefiles/stb.cmake)

set(BUILD_SHARED_LIBS)
set(SDL_EXAMPLES OFF)
set(SDL_TESTS OFF)

add_subdirectory(third_party/sdl3)
add_subdirectory(src/)
