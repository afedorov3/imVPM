cmake_minimum_required(VERSION 3.27)
project(imVocalPitchMonitor DESCRIPTION "VocalPitchMonitor port to PC" LANGUAGES CXX C)

# build tools required: CMake, git, make/ninja, C++11 compiler (Clang/GCC/MSVC are tested),
#                       patch (on windows you'll get it with git),
#                       Windows SDK and MSVC libs for windows build (the only supported option yet)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E echo ===== MAKE SURE you have git core.autocrlf set to false and core.eol set to lf =====
)

if (NOT DEFINED OPUS_SUPPORT)
  set(OPUS_SUPPORT "yes")
endif()

# fonts
set(UI_FONT "DejaVuSans.ttf")
set(UI_FONT_MONO "DejaVuSansMono.ttf")
set(FA_REGULAR "Font Awesome 6 Free-Regular-400.otf")
set(FA_SOLID "Font Awesome 6 Free-Solid-900.otf")

if (NOT DEFINED PM_BACKEND)
  #set(PM_BACKEND "sdl2")
  set(PM_BACKEND "win32")
endif()

if (NOT DEFINED PM_GRAPHICS)
  #set(PM_GRAPHICS "opengl3")
  set(PM_GRAPHICS "dx11")
endif()

find_program(PATCH patch)
if (${PATCH} STREQUAL "PATCH-NOTFOUND")
  message(FATAL_ERROR "patch utility not available")
endif()

option(BUILD_TESTING "" OFF)
option(BUILD_EXAMPLE "" OFF)
option(BUILD_TESTS "" OFF)

include_directories(src)



###### FetchContent

set(IMGUI_SRC ${CMAKE_BINARY_DIR}/_deps/imgui-src)
set(MINIAUDIO_SRC ${CMAKE_BINARY_DIR}/_deps/miniaudio-src)
set(LIBOGG_SRC ${CMAKE_BINARY_DIR}/_deps/libogg-src)
set(LIBOGG_BUILD ${CMAKE_BINARY_DIR}/_deps/libogg-build)
set(OPUS_SRC ${CMAKE_BINARY_DIR}/_deps/opus-src)
set(OPUS_BUILD ${CMAKE_BINARY_DIR}/_deps/opus-build)
set(OPUSFILE_SRC ${CMAKE_BINARY_DIR}/_deps/opusfile-src)
set(OPUSFILE_BUILD ${CMAKE_BINARY_DIR}/_deps/opusfile-build)
set(FFT4G_SRC ${CMAKE_BINARY_DIR}/_deps/fft4g-src/C++)
set(SIMPLEINI_SRC ${CMAKE_BINARY_DIR}/_deps/simpleini-src)
set(PFD_SRC ${CMAKE_BINARY_DIR}/_deps/pfd-src)
set(POPL_SRC ${CMAKE_BINARY_DIR}/_deps/popl-src)

include(FetchContent)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E echo Downloading external dependencies...
)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY  https://github.com/ocornut/imgui.git
  GIT_TAG         master
  DOWNLOAD_EXTRACT_TIMESTAMP yes
)

FetchContent_Declare(
  miniaudio
  GIT_REPOSITORY  https://github.com/mackron/miniaudio.git
  GIT_TAG         master
  DOWNLOAD_EXTRACT_TIMESTAMP yes
)

FetchContent_Declare(
  fft4g
  GIT_REPOSITORY  https://github.com/YSRKEN/Ooura-FFT-Library-by-Other-Language.git
  GIT_TAG         master
  DOWNLOAD_EXTRACT_TIMESTAMP yes
)

FetchContent_Declare(
  simpleini
  GIT_REPOSITORY  https://github.com/brofield/simpleini.git
  GIT_TAG         master
  DOWNLOAD_EXTRACT_TIMESTAMP yes
)

FetchContent_Declare(
  pfd
  GIT_REPOSITORY  https://github.com/samhocevar/portable-file-dialogs.git
  GIT_TAG         main
  DOWNLOAD_EXTRACT_TIMESTAMP yes
)

FetchContent_Declare(
  popl
  GIT_REPOSITORY  https://github.com/badaix/popl.git
  GIT_TAG         master
)

FetchContent_MakeAvailable(imgui miniaudio fft4g simpleini pfd popl)

if (OPUS_SUPPORT)
  set(OP_DISABLE_HTTP "yes")
  set(OP_DISABLE_EXAMPLES "yes")
  set(OP_DISABLE_DOCS "yes")

  FetchContent_Declare(
    libogg
    GIT_REPOSITORY  https://github.com/xiph/ogg.git
    GIT_TAG         master
    DOWNLOAD_EXTRACT_TIMESTAMP yes
  )
  FetchContent_Declare(
    opus
    GIT_REPOSITORY  https://github.com/xiph/opus.git
    GIT_TAG         main
    DOWNLOAD_EXTRACT_TIMESTAMP yes
  )
  set(OGG_LIBRARY ${LIBOGG_BUILD}/ogg)
  set(OGG_INCLUDE_DIR ${LIBOGG_SRC}/include)
  set(OPUS_LIBRARY ${OPUS_BUILD}/opus)
  set(OPUS_INCLUDE_DIR ${OPUS_SRC}/include)
  FetchContent_Declare(
    opusfile
    GIT_REPOSITORY  https://github.com/xiph/opusfile.git
    GIT_TAG         master
    DOWNLOAD_EXTRACT_TIMESTAMP yes
  )

  FetchContent_MakeAvailable(libogg opus opusfile)
endif()

###### /FetchContent



set(FONTS_SRC ${CMAKE_SOURCE_DIR}/external/fonts)

###
### prepare sources
###
if (OPUS_SUPPORT)
  # if opusfile lacks CMake configuration, provide ours and configure
  if(NOT EXISTS ${OPUSFILE_SRC}/CMakeLists.txt)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/assets/external/opusfile/CMakeLists.txt ${OPUSFILE_SRC}/CMakeLists.txt
    )
    add_subdirectory(${OPUSFILE_SRC})
  endif()
endif()

if(${PM_GRAPHICS} STREQUAL "dx11")
  execute_process(
    COMMAND ${PATCH} -p0 -i ${CMAKE_SOURCE_DIR}/assets/external/imgui/ImGui_ImplDX11_ReCreateFontsTexture.patch
    WORKING_DIRECTORY ${IMGUI_SRC}
  )
endif()

execute_process(
  COMMAND ${PATCH} -p0 -i ${CMAKE_SOURCE_DIR}/assets/external/miniaudio/ma_default_vfs_seek__win32.patch
  WORKING_DIRECTORY ${MINIAUDIO_SRC}
)

execute_process(
  COMMAND ${PATCH} -p0 -i ${CMAKE_SOURCE_DIR}/assets/external/portable-file-dialogs/portable-file-dialogs-fixes.patch
  WORKING_DIRECTORY ${PFD_SRC}
)

###
### font compressing tool
###
add_executable(binary_to_compressed_c EXCLUDE_FROM_ALL ${IMGUI_SRC}/misc/fonts/binary_to_compressed_c.cpp)
set_target_properties(binary_to_compressed_c
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

###
### fonts data
###
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp
    COMMAND ${CMAKE_COMMAND} -E echo "#include \"fonts.h\"" > ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} -nostatic -u8 "${FONTS_SRC}/${UI_FONT}" Font >> ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} -nostatic -u8 "${FONTS_SRC}/${UI_FONT_MONO}" FontMono >> ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} -nostatic -u8 "${FONTS_SRC}/${FA_REGULAR}" FARegular >> ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} -nostatic -u8 "${FONTS_SRC}/${FA_SOLID}" FASolid >> ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp
    VERBATIM
    DEPENDS binary_to_compressed_c "${FONTS_SRC}/${UI_FONT}" "${FONTS_SRC}/${UI_FONT_MONO}" "${FONTS_SRC}/${FA_REGULAR}" "${FONTS_SRC}/${FA_SOLID}"
)

###
### test app
###
if (BUILD_TEST_APP)
  add_executable(pitchtest src/testapp.cpp ${FFT4G_SRC}/fft4g.cpp)
  set_target_properties(pitchtest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "bin")
  target_include_directories(pitchtest PRIVATE ${FFT4G_SRC} ${MINIAUDIO_SRC})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DANALYZER_DEBUG")
endif()

###
### imVocalPitchMonitor
###
add_executable(imvpm src/imvpm.cpp src/AudioHandler.cpp ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp ${MINIAUDIO_SRC}/extras/stb_vorbis.c src/imgui/backends/${PM_BACKEND}_${PM_GRAPHICS}.cpp ${FFT4G_SRC}/fft4g.cpp ${IMGUI_SRC}/backends/imgui_impl_${PM_GRAPHICS}.cpp ${IMGUI_SRC}/backends/imgui_impl_${PM_BACKEND}.cpp ${IMGUI_SRC}/imgui.cpp ${IMGUI_SRC}/imgui_widgets.cpp ${IMGUI_SRC}/imgui_tables.cpp ${IMGUI_SRC}/imgui_draw.cpp)
set_target_properties(imvpm PROPERTIES RUNTIME_OUTPUT_DIRECTORY "bin")
target_include_directories(imvpm PRIVATE src/imgui ${FFT4G_SRC} ${MINIAUDIO_SRC} ${IMGUI_SRC} ${IMGUI_SRC}/backends ${FONTS_SRC} ${PFD_SRC} ${SIMPLEINI_SRC} ${POPL_SRC}/include)
if (OPUS_SUPPORT)
  target_compile_definitions(imvpm PRIVATE HAVE_OPUS)
  target_include_directories(imvpm PRIVATE ${OPUSFILE_SRC}/include)
  link_directories(${LIBOGG_BUILD} ${OPUS_BUILD} ${OPUSFILE_BUILD})
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
