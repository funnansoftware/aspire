# scopegpu draws scope's picture with SDL's GPU API and shaders of its own, rather than with SDL's renderer, so the two
# can be compared. SDL has no GPU backend on the web, and scope isn't built for Android.
if(ANDROID OR EMSCRIPTEN)
    return()
endif()

find_package(SDL3 3.4 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

# The shaders are written once, in GLSL, and compiled for each of SDL's GPU backends: to SPIR-V for Vulkan with glslang,
# and from that to MSL for Metal with SPIRV-Cross. On Windows SPIRV-Cross also writes HLSL, which the Windows SDK's
# compilers turn into DXIL and DXBC for Direct3D 12. vcpkg builds glslang and SPIRV-Cross as host tools.
find_program(SCOPEGPU_GLSLANG NAMES glslang glslangValidator REQUIRED)
find_program(SCOPEGPU_SPIRV_CROSS NAMES spirv-cross REQUIRED)
set(scopegpuFormats spv msl)

if(WIN32)
    # Direct3D 12 takes DXIL where the device has shader model 6, as SDL's renderer gives it, and DXBC from any.
    find_program(SCOPEGPU_DXC NAMES dxc HINTS "$ENV{WindowsSdkVerBinPath}x64")
    find_program(SCOPEGPU_FXC NAMES fxc HINTS "$ENV{WindowsSdkVerBinPath}x64")

    if(SCOPEGPU_DXC)
        list(APPEND scopegpuFormats dxil)
    endif()

    if(SCOPEGPU_FXC)
        list(APPEND scopegpuFormats dxbc)
    endif()

    if(NOT SCOPEGPU_DXC AND NOT SCOPEGPU_FXC)
        message(FATAL_ERROR "scopegpu needs dxc or fxc from the Windows SDK: configure from a Visual Studio developer environment.")
    endif()
endif()

# The 2d shaders are scopegpu2d's: the same work as their namesakes, but drawn through SDL's renderer, behind its own
# vertex shaders and into one target at a time. Both sets share what's in shaders/common.
set(scopegpuShaders
    block.vert
    blur.frag
    blur2d.frag
    composite.frag
    composite2d.frag
    fullscreen.vert
    glow.frag
    glow2d.frag
    mark.vert
    shape.frag
    shape.vert
    text.frag
    text.vert
)
set(scopegpuShaderDirectory "${CMAKE_CURRENT_BINARY_DIR}/shaders")
# The most taps the glow's blur takes, given to the blur shaders and, through the shader module, to the painters.
set(scopegpuMaxTaps 16)
set(scopegpuShaderBlobs)
file(MAKE_DIRECTORY "${scopegpuShaderDirectory}")

foreach(shader IN LISTS scopegpuShaders)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}")
    set(compiled "${scopegpuShaderDirectory}/${shader}")

    # SDL's Vulkan backend makes a Vulkan 1.0 instance. glslang lists the files a shader includes, so changing one rebuilds
    # the shaders that include it.
    add_custom_command(OUTPUT "${compiled}.spv"
        COMMAND "${SCOPEGPU_GLSLANG}" -V --target-env vulkan1.0 -DMAX_TAPS=${scopegpuMaxTaps} --depfile "${compiled}.spv.d" -o "${compiled}.spv"
                "${source}"
        DEPENDS "${source}"
        DEPFILE "${compiled}.spv.d"
        COMMENT "Compiling ${shader} to SPIR-V"
        VERBATIM
    )
    # Each binding keeps its number as its MSL index, as SDL's Metal backend binds them.
    add_custom_command(OUTPUT "${compiled}.msl"
        COMMAND "${SCOPEGPU_SPIRV_CROSS}" "${compiled}.spv" --msl --msl-decoration-binding --output "${compiled}.msl"
        DEPENDS "${compiled}.spv"
        VERBATIM
    )
    list(APPEND scopegpuShaderBlobs "${compiled}.spv" "${compiled}.msl")

    if(WIN32)
        if(shader MATCHES "\\.vert$")
            set(stage vs)
        else()
            set(stage ps)
        endif()

        # From shader model 5.1, each descriptor set becomes a register space, as SDL's Direct3D 12 backend binds them.
        add_custom_command(OUTPUT "${compiled}.hlsl"
            COMMAND "${SCOPEGPU_SPIRV_CROSS}" "${compiled}.spv" --hlsl --shader-model 51 --output "${compiled}.hlsl"
            DEPENDS "${compiled}.spv"
            VERBATIM
        )

        if(SCOPEGPU_DXC)
            add_custom_command(OUTPUT "${compiled}.dxil"
                COMMAND "${SCOPEGPU_DXC}" -nologo -T ${stage}_6_0 -E main -Fo "${compiled}.dxil" "${compiled}.hlsl"
                DEPENDS "${compiled}.hlsl"
                VERBATIM
            )
            list(APPEND scopegpuShaderBlobs "${compiled}.dxil")
        endif()

        if(SCOPEGPU_FXC)
            add_custom_command(OUTPUT "${compiled}.dxbc"
                COMMAND "${SCOPEGPU_FXC}" /nologo /T ${stage}_5_1 /E main /Fo "${compiled}.dxbc" "${compiled}.hlsl"
                DEPENDS "${compiled}.hlsl"
                VERBATIM
            )
            list(APPEND scopegpuShaderBlobs "${compiled}.dxbc")
        endif()
    endif()
endforeach()

# The compiled shaders, carried in the executable as a module of their own.
set(scopegpuShaderCode "${CMAKE_CURRENT_BINARY_DIR}/ShaderCode.ixx")
string(REPLACE ";" "," scopegpuShaderList "${scopegpuShaders}")
string(REPLACE ";" "," scopegpuFormatList "${scopegpuFormats}")
add_custom_command(OUTPUT "${scopegpuShaderCode}"
    COMMAND "${CMAKE_COMMAND}" "-DOUTPUT=${scopegpuShaderCode}" "-DDIRECTORY=${scopegpuShaderDirectory}" "-DSHADERS=${scopegpuShaderList}"
            "-DFORMATS=${scopegpuFormatList}" "-DMAX_TAPS=${scopegpuMaxTaps}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Embed.cmake"
    DEPENDS ${scopegpuShaderBlobs} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Embed.cmake"
    COMMENT "Embedding scopegpu's shaders"
    VERBATIM
)

# Owned SDL GPU objects, and the few ways scopegpu makes them.
add_library(scopegpu-gpu STATIC)
target_sources(scopegpu-gpu PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Gpu.ixx
)
target_link_libraries(scopegpu-gpu PUBLIC SDL3::SDL3)

# The shaders, made for a device in the best format it takes.
add_library(scopegpu-shaders STATIC)
target_sources(scopegpu-shaders PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" FILES
        Shaders.ixx
        "${scopegpuShaderCode}"
)
target_link_libraries(scopegpu-shaders PUBLIC scopegpu-gpu)

# A frame of the picture laid out for the GPU: scope's triangles, and what's drawn in what order. The glow's triangles
# are worked out on a thread of their own, which older glibc needs libpthread for.
find_package(Threads REQUIRED)
add_library(scopegpu-frame STATIC)
target_sources(scopegpu-frame PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Frame.ixx
)
target_link_libraries(scopegpu-frame PUBLIC scope-glow scope-tessellation Threads::Threads)

# Rectangles packed into an atlas on shelves.
add_library(scopegpu-shelves STATIC)
target_sources(scopegpu-shelves PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Shelves.ixx
)

# Text laid out by SDL_ttf as glyphs from an atlas the GPU samples.
add_library(scopegpu-glyphs STATIC)
target_sources(scopegpu-glyphs PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Glyphs.ixx
)
target_link_libraries(scopegpu-glyphs PUBLIC scopegpu-gpu scopegpu-shelves scope-picture scope-mesh SDL3_ttf::SDL3_ttf)

# Draws the picture with SDL's GPU API.
add_library(scopegpu-painter STATIC)
target_sources(scopegpu-painter PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Painter.ixx
)
target_link_libraries(scopegpu-painter PUBLIC scopegpu-frame scopegpu-glyphs scopegpu-shaders)

add_executable(scopegpu main.cpp)
target_link_libraries(scopegpu PRIVATE scopegpu-painter SDL3_ttf::SDL3_ttf SDL3::SDL3)

# The readout font is read from next to the executable, with its licence beside it, as scope reads it.
set(SCOPEGPU_FONT_DIR "${CMAKE_SOURCE_DIR}/assets/b612")
set(SCOPEGPU_FONT_FILES
    "${SCOPEGPU_FONT_DIR}/B612Mono-Regular.ttf"
    "${SCOPEGPU_FONT_DIR}/OFL.txt"
)
add_custom_command(TARGET scopegpu POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:scopegpu>/fonts"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${SCOPEGPU_FONT_FILES} "$<TARGET_FILE_DIR:scopegpu>/fonts"
    VERBATIM
)
install(TARGETS scopegpu RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES ${SCOPEGPU_FONT_FILES} DESTINATION "${CMAKE_INSTALL_BINDIR}/fonts")

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
    add_executable(scopegpu-frame-test test/Frame.test.cpp)
    target_link_libraries(scopegpu-frame-test PRIVATE scopegpu-frame GTest::gtest GTest::gtest_main)
    gtest_discover_tests(scopegpu-frame-test)
    add_executable(scopegpu-shelves-test test/Shelves.test.cpp)
    target_link_libraries(scopegpu-shelves-test PRIVATE scopegpu-shelves GTest::gtest GTest::gtest_main)
    gtest_discover_tests(scopegpu-shelves-test)

    # Draws offscreen, so it needs a GPU device but no display. Direct3D 12 needs no video driver of its own, and
    # Vulkan's headless surfaces come with SDL's offscreen one; Metal needs a display's, so macOS runners skip it, as
    # do runners with no GPU device to make.
    add_test(NAME scopegpu-smoke COMMAND scopegpu --offscreen --paused --frames=3)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(scopegpuSmokeVideo offscreen)
    else()
        set(scopegpuSmokeVideo dummy)
    endif()
    set_tests_properties(scopegpu-smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=${scopegpuSmokeVideo}"
        SKIP_REGULAR_EXPRESSION "No GPU device:"
        TIMEOUT 20
    )
endif()
