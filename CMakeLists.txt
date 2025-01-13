cmake_minimum_required(VERSION 3.15)
project(imVocalPitchMonitor DESCRIPTION "VocalPitchMonitor port to PC" LANGUAGES CXX C)

# build tools required: CMake, git, make/ninja, C++ compiler (Clang/GCC/MSVC are tested),
#                       patch (on windows you'll get it with git),
#                       Windows SDK and MSVC libs for windows build (the only supported option yet)

if (NOT DEFINED OPUS_SUPPORT)
  set(OPUS_SUPPORT "yes")
endif()

# fonts
set(UI_FONT "external/fonts/DejaVuSans.ttf")
set(UI_FONT_MONO "external/fonts/DejaVuSansMono.ttf")
set(FA_REGULAR "external/fonts/Font Awesome 6 Free-Regular-400.otf")
set(FA_SOLID "external/fonts/Font Awesome 6 Free-Solid-900.otf")

if (NOT DEFINED PM_BACKEND)
  #set(PM_BACKEND "sdl2")
  set(PM_BACKEND "win32")
endif()

if (NOT DEFINED PM_GRAPHICS)
  #set(PM_GRAPHICS "opengl3")
  set(PM_GRAPHICS "dx11")
endif()

find_program(PATCH patch)
find_program(SED sed)
if (${PATCH} STREQUAL "PATCH-NOTFOUND")
  message(FATAL_ERROR "patch utility not available")
endif()
if (${SED} STREQUAL "SED-NOTFOUND")
  message(FATAL_ERROR "sed utility not available")
endif()

include_directories(src)

include(FetchContent)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY  https://github.com/ocornut/imgui.git
  GIT_TAG         v1.91.6
  DOWNLOAD_EXTRACT_TIMESTAMP yes
)

FetchContent_MakeAvailable(imgui)

if(${PM_GRAPHICS} STREQUAL "dx11")
  execute_process(
    COMMAND ${PATCH} -p0 -i ${CMAKE_SOURCE_DIR}/assets/external/imgui/ImGui_ImplDX11_ReCreateFontsTexture.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/_deps/imgui-src
  )
endif()

if (OPUS_SUPPORT)
  option(BUILD_TESTING "" OFF)

  FetchContent_Declare(
    libogg
    GIT_REPOSITORY  https://github.com/xiph/ogg.git
    GIT_TAG         v1.3.5
    DOWNLOAD_EXTRACT_TIMESTAMP yes
  )
  FetchContent_Declare(
    opus
    GIT_REPOSITORY  https://github.com/xiph/opus.git
    GIT_TAG         v1.5.2
    DOWNLOAD_EXTRACT_TIMESTAMP yes
  )
  FetchContent_Declare(
    opusfile
    GIT_REPOSITORY  https://github.com/xiph/opusfile.git
    GIT_TAG         v0.12
    DOWNLOAD_EXTRACT_TIMESTAMP yes
  )

  FetchContent_MakeAvailable(libogg opus opusfile)

  include_directories(${CMAKE_BINARY_DIR}/_deps/libogg-src/include ${CMAKE_BINARY_DIR}/_deps/opus-src/include ${CMAKE_BINARY_DIR}/_deps/opusfile-src/include)

  # opusfile lacks CMake configuration, provide ours and configure
  if(NOT EXISTS ${CMAKE_BINARY_DIR}/_deps/opusfile-src/CMakeLists.txt)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/assets/external/opusfile/CMakeLists.txt ${CMAKE_BINARY_DIR}/_deps/opusfile-src
    )
    add_subdirectory(${CMAKE_BINARY_DIR}/_deps/opusfile-src)
  endif()

endif()

###
### font compressing tool
###
add_executable(binary_to_compressed_c EXCLUDE_FROM_ALL ${CMAKE_BINARY_DIR}/_deps/imgui-src/misc/fonts/binary_to_compressed_c.cpp)
set_target_properties(binary_to_compressed_c
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

###
### fonts data
###
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp
    COMMAND ${CMAKE_COMMAND} -E echo "#include \"fonts.h\"" > ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} -nostatic -u8 "${CMAKE_SOURCE_DIR}/${UI_FONT}" Font >> ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} -nostatic -u8 "${CMAKE_SOURCE_DIR}/${UI_FONT_MONO}" FontMono >> ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} -nostatic -u8 "${CMAKE_SOURCE_DIR}/${FA_REGULAR}" FARegular >> ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} -nostatic -u8 "${CMAKE_SOURCE_DIR}/${FA_SOLID}" FASolid >> ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp
    VERBATIM
    DEPENDS binary_to_compressed_c "${UI_FONT}" "${UI_FONT_MONO}" "${FA_REGULAR}" "${FA_SOLID}"
)

###
### test app
###
if (BUILD_TEST_APP)
  add_executable(pitchtest src/testapp.cpp external/fft4g.cpp)
  set_target_properties(pitchtest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "bin")
  target_include_directories(pitchtest PRIVATE external)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DANALYZER_DEBUG")
endif()

###
### imVocalPitchMonitor
###
add_executable(imvpm src/imvpm.cpp src/AudioHandler.cpp ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp external/stb_vorbis.c src/imgui/backends/${PM_BACKEND}_${PM_GRAPHICS}.cpp external/fft4g.cpp ${CMAKE_BINARY_DIR}/_deps/imgui-src/backends/imgui_impl_${PM_GRAPHICS}.cpp ${CMAKE_BINARY_DIR}/_deps/imgui-src/backends/imgui_impl_${PM_BACKEND}.cpp ${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui.cpp ${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui_widgets.cpp ${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui_tables.cpp ${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui_draw.cpp)
set_target_properties(imvpm PROPERTIES RUNTIME_OUTPUT_DIRECTORY "bin")
target_include_directories(imvpm PRIVATE src/imgui external ${CMAKE_BINARY_DIR}/_deps/imgui-src ${CMAKE_BINARY_DIR}/_deps/imgui-src/backends external/fonts external/portable-file-dialogs external/simpleini)
if (OPUS_SUPPORT)
  target_compile_definitions(imvpm PRIVATE HAVE_OPUS)
  link_directories(${CMAKE_BINARY_DIR}/_deps/libogg-build ${CMAKE_BINARY_DIR}/_deps/opus-build ${CMAKE_BINARY_DIR}/_deps/opusfile-build)
  target_link_libraries(imvpm opusfile ogg opus)
endif()

###
### compiler options
###
if (WIN32)
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(imvpm PRIVATE "-municode")
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # forward debug flag to RC
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -D_DEBUG")
  else()
    # set subsystem type while having good old main()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_link_options(imvpm PRIVATE "-Wl,-subsystem,windows")
    else()
      target_link_options(imvpm PRIVATE "LINKER:/subsystem:windows,/entry:wmainCRTStartup")
    endif()
  endif()

  # add windows resource file
  target_sources(imvpm PRIVATE assets/res/imvpm.rc)
endif()

###
### GUI dependencies
###
if(${PM_BACKEND} STREQUAL "win32")
  target_link_libraries(imvpm Dwmapi)
elseif(${PM_BACKEND} STREQUAL "sdl2")
  include_directories(/usr/include/SDL2)
  target_link_libraries(imvpm SDL2)
endif()

if(${PM_GRAPHICS} STREQUAL "opengl3")
  target_link_libraries(imvpm OpenGL)
elseif(${PM_GRAPHICS} STREQUAL "dx11")
  target_link_libraries(imvpm d3d11 d3dcompiler)
endif()
