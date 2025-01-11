cmake_minimum_required(VERSION 3.13)
project(imVocalPitchMonitor DESCRIPTION "VocalPitchMonitor port to PC" LANGUAGES CXX C)

# TODO: dep tests
set(HAVE_OPUS "yes")

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

include_directories(src)

###
### font compressing tool
###
add_executable(binary_to_compressed_c EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/external/imgui/misc/fonts/binary_to_compressed_c.cpp)
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
add_executable(imvpm src/imvpm.cpp src/AudioHandler.cpp ${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp external/stb_vorbis.c src/imgui/backends/${PM_BACKEND}_${PM_GRAPHICS}.cpp external/fft4g.cpp external/imgui/backends/imgui_impl_${PM_GRAPHICS}.cpp external/imgui/backends/imgui_impl_${PM_BACKEND}.cpp external/imgui/imgui.cpp external/imgui/imgui_widgets.cpp external/imgui/imgui_tables.cpp external/imgui/imgui_draw.cpp)
set_target_properties(imvpm PROPERTIES RUNTIME_OUTPUT_DIRECTORY "bin")
target_include_directories(imvpm PRIVATE src/imgui external external/imgui external/imgui/backends external/fonts external/portable-file-dialogs)
if (HAVE_OPUS)
  target_compile_definitions(imvpm PRIVATE HAVE_OPUS)
  target_include_directories(imvpm PRIVATE external/libogg/include external/opus/include external/opusfile/include)
  target_link_libraries(imvpm ${CMAKE_SOURCE_DIR}/external/libogg/build/ogg.lib ${CMAKE_SOURCE_DIR}/external/opus/build/opus.lib ${CMAKE_SOURCE_DIR}/external/opusfile/build/opusfile.lib)
endif()

if (WIN32)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # forward debug flag to RC
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -D_DEBUG")
  else()
    # set subsystem type while having good old main()
    target_link_options(imvpm PRIVATE "LINKER:/subsystem:windows,/entry:wmainCRTStartup")
  endif()

  # add windows resource file
  target_sources(imvpm PRIVATE assets/res/imvpm.rc)
endif()

###
### dependencies
###
if(${PM_BACKEND} STREQUAL "sdl2")
  include_directories(/usr/include/SDL2)
  target_link_libraries(imvpm SDL2)
endif()

if(${PM_GRAPHICS} STREQUAL "opengl3")
  target_link_libraries(imvpm OpenGL)
elseif(${PM_GRAPHICS} STREQUAL "dx11")
  target_link_libraries(imvpm d3d11 d3dcompiler)
endif()
