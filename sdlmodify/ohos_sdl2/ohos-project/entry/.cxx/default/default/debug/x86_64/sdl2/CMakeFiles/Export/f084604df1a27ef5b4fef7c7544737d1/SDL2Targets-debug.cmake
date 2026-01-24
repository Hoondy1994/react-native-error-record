#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SDL2::SDL2-static" for configuration "Debug"
set_property(TARGET SDL2::SDL2-static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(SDL2::SDL2-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C;CXX"
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "m;D:/601/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/lib/x86_64-linux-ohos/libace_napi.z.so;D:/601/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/lib/x86_64-linux-ohos/libhilog_ndk.z.so;D:/601/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/lib/x86_64-linux-ohos/libace_ndk.z.so;D:/601/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/lib/x86_64-linux-ohos/librawfile.z.so;D:/601/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/lib/x86_64-linux-ohos/libpixelmap_ndk.z.so;cjson;D:/601/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/lib/x86_64-linux-ohos/libohaudio.so;-Wl,--no-undefined;-lpthread"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libSDL2d.a"
  )

list(APPEND _cmake_import_check_targets SDL2::SDL2-static )
list(APPEND _cmake_import_check_files_for_SDL2::SDL2-static "${_IMPORT_PREFIX}/lib/libSDL2d.a" )

# Import target "SDL2::SDL2" for configuration "Debug"
set_property(TARGET SDL2::SDL2 APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(SDL2::SDL2 PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "m;D:/601/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/lib/x86_64-linux-ohos/libace_napi.z.so;D:/601/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/lib/x86_64-linux-ohos/libhilog_ndk.z.so;D:/601/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/lib/x86_64-linux-ohos/libace_ndk.z.so;D:/601/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/lib/x86_64-linux-ohos/librawfile.z.so;D:/601/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/lib/x86_64-linux-ohos/libpixelmap_ndk.z.so;cjson;D:/601/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/lib/x86_64-linux-ohos/libohaudio.so;-Wl,--no-undefined;-lpthread;uv"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libSDL2.so"
  IMPORTED_SONAME_DEBUG "libSDL2.so"
  )

list(APPEND _cmake_import_check_targets SDL2::SDL2 )
list(APPEND _cmake_import_check_files_for_SDL2::SDL2 "${_IMPORT_PREFIX}/lib/libSDL2.so" )

# Import target "SDL2::SDL2main" for configuration "Debug"
set_property(TARGET SDL2::SDL2main APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(SDL2::SDL2main PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libSDL2maind.a"
  )

list(APPEND _cmake_import_check_targets SDL2::SDL2main )
list(APPEND _cmake_import_check_files_for_SDL2::SDL2main "${_IMPORT_PREFIX}/lib/libSDL2maind.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
