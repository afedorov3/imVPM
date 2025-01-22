cmake_minimum_required(VERSION 3.15)
project(imVocalPitchMonitor DESCRIPTION "VocalPitchMonitor port to PC" LANGUAGES CXX C)

if(NOT DEFINED OPUS_SUPPORT)
  set(OPUS_SUPPORT "yes")
endif()

# fonts
set(UI_FONT "DejaVuSans.ttf")
set(UI_FONT_MONO "DejaVuSansMono.ttf")
set(FA_REGULAR "Font Awesome 6 Free-Regular-400.otf")
set(FA_SOLID "Font Awesome 6 Free-Solid-900.otf")

if(NOT DEFINED IMVPM_BACKEND)
  if(WIN32)
    set(IMVPM_BACKEND "win32")
  else()
    set(IMVPM_BACKEND "sdl2")
  endif()
endif()

if(NOT DEFINED IMVPM_GRAPHICS)
  if(WIN32)
    set(IMVPM_GRAPHICS "dx11")
  else()
    set(IMVPM_GRAPHICS "vulkan")
  endif()
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_program(PATCH patch)
if(PATCH STREQUAL "PATCH-NOTFOUND")
  message(FATAL_ERROR "patch utility not available")
endif()

option(BUILD_TESTING "" OFF)
option(BUILD_EXAMPLE "" OFF)
option(BUILD_TESTS "" OFF)

include_directories(src)
if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_compile_definitions(_DEBUG ANALYZER_DEBUG)
endif()

if(NOT WIN32)
  find_package(PkgConfig REQUIRED)
endif()

if(VENDORED_BUILD)
###### local dependencies

  set(IMGUI_SRC ${CMAKE_SOURCE_DIR}/external/imgui)
  set(MINIAUDIO_SRC ${CMAKE_SOURCE_DIR}/external/miniaudio)
  set(LIBOGG_SRC ${CMAKE_SOURCE_DIR}/external/libogg)
  set(LIBOGG_BUILD ${CMAKE_BINARY_DIR}/external/libogg)
  set(OPUS_SRC ${CMAKE_SOURCE_DIR}/external/opus)
  set(OPUS_BUILD ${CMAKE_BINARY_DIR}/external/opus)
  set(OPUSFILE_SRC ${CMAKE_SOURCE_DIR}/external/opusfile)
  set(OPUSFILE_BUILD ${CMAKE_BINARY_DIR}/opusfile)
  set(FFT4G_SRC ${CMAKE_SOURCE_DIR}/external/fft4g)
  set(SIMPLEINI_SRC ${CMAKE_SOURCE_DIR}/external/simpleini)
  set(PFD_SRC ${CMAKE_SOURCE_DIR}/external/pfd)
  set(POPL_SRC ${CMAKE_SOURCE_DIR}/external/popl)

  if(NOT EXISTS ${IMGUI_SRC})
    message(FATAL_ERROR "ImGui sources not available (${IMGUI_SRC})")
  endif()
  if(NOT EXISTS ${MINIAUDIO_SRC})
    message(FATAL_ERROR "miniaudio sources not available (${MINIAUDIO_SRC})")
  endif()
  if(NOT EXISTS ${FFT4G_SRC})
    message(FATAL_ERROR "fft4g sources not available (${FFT4G_SRC})")
  endif()
  if(NOT EXISTS ${SIMPLEINI_SRC})
    message(FATAL_ERROR "simpleini sources not available (${SIMPLEINI_SRC})")
  endif()
  if(NOT EXISTS ${PFD_SRC})
    message(FATAL_ERROR "portable-file-dialogs sources not available (${PFD_SRC})")
  endif()
  if(NOT EXISTS ${POPL_SRC})
    message(FATAL_ERROR "popl sources not available (${POPL_SRC})")
  endif()

  if(OPUS_SUPPORT)

    if(WIN32)
      if(NOT EXISTS ${LIBOGG_SRC})
        message(FATAL_ERROR "libogg sources not available (${LIBOGG_SRC})")
      endif()
      if(NOT EXISTS ${OPUS_SRC})
        message(FATAL_ERROR "opus sources not available (${OPUS_SRC})")
      endif()
      if(NOT EXISTS ${OPUSFILE_SRC})
        message(FATAL_ERROR "opusfile sources not available (${OPUSFILE_SRC})")
      endif()

      # provide our CMake configuration for opusfile
      execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/assets/external/opusfile/CMakeLists.txt ${OPUSFILE_SRC}/CMakeLists.txt
      )

      set(OP_DISABLE_HTTP "yes")
      set(OP_DISABLE_EXAMPLES "yes")
      set(OP_DISABLE_DOCS "yes")

      add_subdirectory(${LIBOGG_SRC} EXCLUDE_FROM_ALL)
      add_subdirectory(${OPUS_SRC} EXCLUDE_FROM_ALL)
      add_subdirectory(${OPUSFILE_SRC} EXCLUDE_FROM_ALL)

    endif()

  endif()

###### /local dependencies
else()
###### FetchContent

  if(WIN32)
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E echo ===== MAKE SURE you have git core.autocrlf set to false and core.eol set to lf =====
    )
  endif()

  set(IMGUI_SRC ${CMAKE_BINARY_DIR}/_deps/imgui-src)
  set(MINIAUDIO_SRC ${CMAKE_BINARY_DIR}/_deps/miniaudio-src)
  set(LIBOGG_SRC ${CMAKE_BINARY_DIR}/_deps/libogg-src)
  set(LIBOGG_BUILD ${CMAKE_BINARY_DIR}/_deps/libogg-build)
  set(OPUS_SRC ${CMAKE_BINARY_DIR}/_deps/opus-src)
  set(OPUS_BUILD ${CMAKE_BINARY_DIR}/_deps/opus-build)
  set(OPUSFILE_SRC ${CMAKE_BINARY_DIR}/_deps/opusfile-src)
  set(OPUSFILE_BUILD ${CMAKE_BINARY_DIR}/_deps/opusfile-build)
  set(FFT4G_SRC ${CMAKE_BINARY_DIR}/_deps/fft4g-src)
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
    GIT_TAG         80c9cd1
    DOWNLOAD_EXTRACT_TIMESTAMP yes
  )

  FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY  https://github.com/mackron/miniaudio.git
    GIT_TAG         4a5b74b
    DOWNLOAD_EXTRACT_TIMESTAMP yes
  )

  FetchContent_Declare(
    fft4g
    GIT_REPOSITORY  https://github.com/YSRKEN/Ooura-FFT-Library-by-Other-Language.git
    GIT_TAG         4a2dccf
    DOWNLOAD_EXTRACT_TIMESTAMP yes
  )

  FetchContent_Declare(
    simpleini
    GIT_REPOSITORY  https://github.com/brofield/simpleini.git
    GIT_TAG         6048871
    DOWNLOAD_EXTRACT_TIMESTAMP yes
  )

  FetchContent_Declare(
    pfd
    GIT_REPOSITORY  https://github.com/samhocevar/portable-file-dialogs.git
    GIT_TAG         7f852d8
    DOWNLOAD_EXTRACT_TIMESTAMP yes
  )

  FetchContent_Declare(
    popl
    GIT_REPOSITORY  https://github.com/badaix/popl.git
    GIT_TAG         bda5f43
  )

  FetchContent_MakeAvailable(imgui miniaudio fft4g simpleini pfd popl)

  if(OPUS_SUPPORT)

    if(WIN32)

      set(OP_DISABLE_HTTP "yes")
      set(OP_DISABLE_EXAMPLES "yes")
      set(OP_DISABLE_DOCS "yes")

      FetchContent_Declare(
        libogg
        GIT_REPOSITORY  https://github.com/xiph/ogg.git
        GIT_TAG         db5c7a4
        DOWNLOAD_EXTRACT_TIMESTAMP yes
      )
      FetchContent_Declare(
        opus
        GIT_REPOSITORY  https://github.com/xiph/opus.git
        GIT_TAG         7db2693
        DOWNLOAD_EXTRACT_TIMESTAMP yes
      )
      FetchContent_Declare(
        opusfile
        GIT_REPOSITORY  https://github.com/xiph/opusfile.git
        GIT_TAG         9d71834
        DOWNLOAD_EXTRACT_TIMESTAMP yes
      )

      FetchContent_MakeAvailable(libogg opus opusfile)

    endif()

  endif()

###### /FetchContent
endif()



set(FONTS_SRC ${CMAKE_SOURCE_DIR}/external/fonts)

###
### prepare the sources
###

if(IMVPM_GRAPHICS STREQUAL "dx11")
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
  COMMAND ${PATCH} -p0 -i ${CMAKE_SOURCE_DIR}/assets/external/fft4g/fft4g-size.patch
  WORKING_DIRECTORY ${FFT4G_SRC}
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
if(BUILD_TEST_APP)
  add_executable(pitchtest src/testapp.cpp ${FFT4G_SRC}/C++/fft4g.cpp)
  set_target_properties(pitchtest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "bin")
  target_include_directories(pitchtest PRIVATE ${FFT4G_SRC}/C++ ${MINIAUDIO_SRC})
  target_compile_definitions(pitchtest PRIVATE ANALYZER_DEBUG)
endif()

###
### imVocalPitchMonitor
###
add_executable(imvpm src/imvpm.cpp src/AudioHandler.cpp ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp ${MINIAUDIO_SRC}/extras/stb_vorbis.c src/imgui/backends/${IMVPM_BACKEND}_${IMVPM_GRAPHICS}.cpp ${FFT4G_SRC}/C++/fft4g.cpp ${IMGUI_SRC}/backends/imgui_impl_${IMVPM_GRAPHICS}.cpp ${IMGUI_SRC}/backends/imgui_impl_${IMVPM_BACKEND}.cpp ${IMGUI_SRC}/imgui.cpp ${IMGUI_SRC}/imgui_widgets.cpp ${IMGUI_SRC}/imgui_tables.cpp ${IMGUI_SRC}/imgui_draw.cpp)
set_target_properties(imvpm PROPERTIES RUNTIME_OUTPUT_DIRECTORY "bin")
target_include_directories(imvpm PRIVATE src/imgui ${FFT4G_SRC}/C++ ${MINIAUDIO_SRC} ${IMGUI_SRC} ${IMGUI_SRC}/backends ${FONTS_SRC} ${PFD_SRC} ${SIMPLEINI_SRC} ${POPL_SRC}/include)
if(OPUS_SUPPORT)
  target_compile_definitions(imvpm PRIVATE HAVE_OPUS)
  if(WIN32)
    target_include_directories(imvpm PRIVATE ${OPUSFILE_SRC}/include)
    target_link_libraries(imvpm PRIVATE opusfile)
  else()
    pkg_search_module(OPUSFILE REQUIRED opusfile)
    target_include_directories(imvpm PRIVATE ${OPUSFILE_INCLUDE_DIRS})
    target_link_libraries(imvpm PRIVATE ${OPUSFILE_LIBRARIES})
  endif()
endif()

###
### compiler options
###
if(WIN32)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(imvpm PRIVATE "-municode")
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # forward debug flag to RC
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -D_DEBUG")
  else()
    # set subsystem type while having good old main()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
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
if(IMVPM_BACKEND STREQUAL "win32")
  target_link_libraries(imvpm PRIVATE Dwmapi)
elseif(IMVPM_BACKEND STREQUAL "sdl2")
  pkg_search_module(SDL2 REQUIRED sdl2)
  include_directories(${SDL2_INCLUDE_DIRS})
  target_link_libraries(imvpm PRIVATE ${SDL2_LIBRARIES})
endif()

if(IMVPM_GRAPHICS STREQUAL "vulkan")
  pkg_search_module(VULKAN REQUIRED vulkan)
  include_directories(${VULKAN_INCLUDE_DIRS})
  target_link_libraries(imvpm PRIVATE ${VULKAN_LIBRARIES})
elseif(IMVPM_GRAPHICS STREQUAL "opengl3")
  target_link_libraries(imvpm PRIVATE GL)
elseif(IMVPM_GRAPHICS STREQUAL "dx11")
  target_link_libraries(imvpm PRIVATE d3d11 d3dcompiler)
endif()
