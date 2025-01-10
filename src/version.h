#pragma once

#define VER_VERSION_MAJOR 0
#define VER_VERSION_MINOR 1
#define VER_VERSION_PATCH 0



#define VER_STRINGIFY(...) #__VA_ARGS__
#define VER_TOSTRING(x) VER_STRINGIFY(x)

#if defined(_DEBUG) || defined(DEBUG)
#define VER_OPTIONS " (debug)"
#else
#define VER_OPTIONS
#endif

#define VER_PRODUCT_NAME          "imVocalPitchMonitor"
#define VER_FILE_NAME             "imvpm.exe"
#define VER_VERSION_FILEVERSION   VER_VERSION_MAJOR , VER_VERSION_MINOR , VER_VERSION_PATCH , 0
#define VER_VERSION_DISPLAY       VER_TOSTRING(VER_VERSION_MAJOR) "." VER_TOSTRING(VER_VERSION_MINOR) "." VER_TOSTRING(VER_VERSION_PATCH) VER_OPTIONS
#define VER_FILE_DESCRIPTION      "VocalPitchMonitor port to PC"
#define VER_DATE_AUTHOR           "2025 AF"
