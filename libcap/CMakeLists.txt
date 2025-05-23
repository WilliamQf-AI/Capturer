cmake_minimum_required(VERSION 3.27)

# Windows SDK >= 10.0.22621.0
if (WIN32 AND "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}" VERSION_LESS "10.0.22621.0")
    # must be 10.0.22621.0, the behavior of winrt::com_ptr in 10.0.22621.0 has been changed
    message(WARNING "Capturer requires Windows SDK 10.0.22621.0 and above to compile.")
endif()

find_package(FFmpeg 7.0 REQUIRED)

if(UNIX AND NOT APPLE)
    find_package(PulseAudio REQUIRED)
    find_package(Libv4l2 REQUIRED)
    find_package(X11 REQUIRED)
endif()

file(GLOB_RECURSE LIBCAP_SOURCES *.cpp)

add_library(libcap ${LIBCAP_SOURCES})
add_library(libcap::libcap ALIAS libcap)

if (WIN32)
    add_custom_target(LIBCAP_SHADERS)

    set(HLSL_SHADER_FILES ResizingVertexShader.hlsl ResizingPixelShader.hlsl)

    set_source_files_properties(ResizingVertexShader.hlsl   PROPERTIES ShaderType "vs")
    set_source_files_properties(ResizingPixelShader.hlsl    PROPERTIES ShaderType "ps")

    foreach (HLSL_FILE ${HLSL_SHADER_FILES})
        get_filename_component(FILE_NAME ${HLSL_FILE} NAME_WE)
        get_source_file_property(SHADER_TYPE ${HLSL_FILE} ShaderType)

        add_custom_command(TARGET LIBCAP_SHADERS PRE_BUILD
            COMMAND fxc.exe /nologo /E${SHADER_TYPE}_main /T${SHADER_TYPE}_5_0 $<IF:$<CONFIG:DEBUG>,/Od,/O1> /Zi /Fh ${PROJECT_BINARY_DIR}/${FILE_NAME}.h ${HLSL_FILE}
            COMMENT "HLSL ${HLSL_FILE}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/resources/d3d11"
            VERBATIM
        )
    endforeach ()

    add_dependencies(libcap LIBCAP_SHADERS)
endif ()

target_compile_options(libcap
    PRIVATE 
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /Zc:preprocessor /Zc:__cplusplus /wd5054>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wno-deprecated-enum-enum-conversion>
)

target_link_libraries(libcap
    PRIVATE
        glog::glog
        fmt::fmt
        probe::probe
        ffmpeg::ffmpeg
        $<$<PLATFORM_ID:Windows>:dwmapi>
        $<$<PLATFORM_ID:Windows>:strmiids>
        $<$<PLATFORM_ID:Windows>:ntdll>
        $<$<PLATFORM_ID:Windows>:Shcore> 
        $<$<PLATFORM_ID:Windows>:DXGI>
        $<$<PLATFORM_ID:Windows>:D3D11>
        $<$<PLATFORM_ID:Windows>:Mfplat>
        $<$<PLATFORM_ID:Windows>:Mf>
        $<$<PLATFORM_ID:Windows>:Mfreadwrite>
        $<$<PLATFORM_ID:Windows>:Mfuuid>
        $<$<PLATFORM_ID:Linux>:Libv4l2::Libv4l2>
        $<$<PLATFORM_ID:Linux>:X11::X11>
        $<$<PLATFORM_ID:Linux>:X11::xcb>
        $<$<PLATFORM_ID:Linux>:X11::xcb_shm>
        $<$<PLATFORM_ID:Linux>:X11::xcb_xfixes>
        $<$<PLATFORM_ID:Linux>:${PULSEAUDIO_LIBRARY}>
)

target_include_directories(libcap
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>

    PRIVATE
        ${PROJECT_SOURCE_DIR}/3rdparty
        ${PROJECT_SOURCE_DIR}/src/common
        ${PULSEAUDIO_INCLUDE_DIR}
        ${PROJECT_BINARY_DIR} # Generated header files
)

set_target_properties(libcap
    PROPERTIES
        PUBLIC_HEADER "${LIBCAP_HEADERS}"

        RUNTIME_OUTPUT_DIRECTORY "bin/$<0:>"
        LIBRARY_OUTPUT_DIRECTORY "lib/$<0:>"
        ARCHIVE_OUTPUT_DIRECTORY "lib/$<0:>"

        DEBUG_POSTFIX "d"
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(libcap PUBLIC CAPTURER_SHARED PRIVATE CAPTURER_BUILDING)
endif()
