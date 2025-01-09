cmake_minimum_required(VERSION 3.13)
project(PitchMonitor VERSION "0.0.1" DESCRIPTION "Pitch monitor application" LANGUAGES CXX C)

set(HAVE_OPUS "yes")
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


add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX}
    COMMAND ${CMAKE_CXX_COMPILER} -O3 -o ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} ${CMAKE_SOURCE_DIR}/external/imgui/misc/fonts/binary_to_compressed_c.cpp
    DEPENDS ${CMAKE_SOURCE_DIR}/external/imgui/misc/fonts/binary_to_compressed_c.cpp
)
add_custom_target(binary_to_compressed_c DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fonts.h
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} -u8 "${CMAKE_SOURCE_DIR}/${UI_FONT}" Font > ${CMAKE_CURRENT_BINARY_DIR}/fonts.h
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} -u8 "${CMAKE_SOURCE_DIR}/${UI_FONT_MONO}" FontMono >> ${CMAKE_CURRENT_BINARY_DIR}/fonts.h
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} -u8 "${CMAKE_SOURCE_DIR}/${FA_REGULAR}" FARegular >> ${CMAKE_CURRENT_BINARY_DIR}/fonts.h
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c${CMAKE_EXECUTABLE_SUFFIX} -u8 "${CMAKE_SOURCE_DIR}/${FA_SOLID}" FASolid >> ${CMAKE_CURRENT_BINARY_DIR}/fonts.h
    VERBATIM
    DEPENDS binary_to_compressed_c "${UI_FONT}" "${UI_FONT_MONO}" "${FA_REGULAR}" "${FA_SOLID}"
)
add_custom_target(fonts_h DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fonts.h)

if (PM_BUILD_TEST_APP)
  add_executable(pitchtest src/testapp.cpp external/fft4g.cpp)
  set_target_properties(pitchtest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "bin")
  target_include_directories(pitchtest PUBLIC src external)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DANALYZER_DEBUG")
endif()

add_executable(imvpm src/imvpm.cpp src/AudioHandler.cpp external/stb_vorbis.c src/imgui/backends/${PM_BACKEND}_${PM_GRAPHICS}.cpp external/fft4g.cpp external/imgui/backends/imgui_impl_${PM_GRAPHICS}.cpp external/imgui/backends/imgui_impl_${PM_BACKEND}.cpp external/imgui/imgui.cpp external/imgui/imgui_widgets.cpp external/imgui/imgui_tables.cpp external/imgui/imgui_draw.cpp)
add_dependencies(imvpm fonts_h)
set_target_properties(imvpm PROPERTIES RUNTIME_OUTPUT_DIRECTORY "bin")
target_include_directories(imvpm PUBLIC src src/imgui ${CMAKE_CURRENT_BINARY_DIR} external external/imgui external/imgui/backends external/fonts external/portable-file-dialogs)
if (HAVE_OPUS)
  target_compile_definitions(imvpm PUBLIC HAVE_OPUS)
  target_include_directories(imvpm PUBLIC external/libogg/include external/opus/include external/opusfile/include)
  target_link_libraries(imvpm ${CMAKE_SOURCE_DIR}/external/libogg/build/ogg.lib ${CMAKE_SOURCE_DIR}/external/opus/build/opus.lib ${CMAKE_SOURCE_DIR}/external/opusfile/build/opusfile.lib)
endif()
set_property(TARGET imvpm APPEND
    PROPERTY ADDITIONAL_CLEAN_FILES ${CMAKE_CURRENT_BINARY_DIR}/fonts.h)

if(${PM_BACKEND} STREQUAL "sdl2")
  include_directories(/usr/include/SDL2)
  target_link_libraries(imvpm SDL2)
elseif(${PM_BACKEND} STREQUAL "win32")
#  target_link_options(imvpm PUBLIC "LINKER:/subsystem:windows,/entry:mainCRTStartup")
endif()

if(${PM_GRAPHICS} STREQUAL "opengl3")
  target_link_libraries(imvpm OpenGL)
elseif(${PM_GRAPHICS} STREQUAL "dx11")
  target_link_libraries(imvpm d3d11 d3dcompiler)
endif()
