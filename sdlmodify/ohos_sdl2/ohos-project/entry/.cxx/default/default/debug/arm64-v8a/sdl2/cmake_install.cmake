# Install script for directory: D:/1224/ohos_sdl2

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/entry")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "D:/601/DevEco Studio/sdk/default/openharmony/native/llvm/bin/llvm-objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/1224/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/sdl2/libSDL2d.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "D:/1224/ohos_sdl2/ohos-project/entry/build/default/intermediates/cmake/default/obj/arm64-v8a/libSDL2.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libSDL2.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libSDL2.so")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "D:/601/DevEco Studio/sdk/default/openharmony/native/llvm/bin/llvm-strip.exe" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libSDL2.so")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/1224/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/sdl2/libSDL2maind.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2/SDL2Targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2/SDL2Targets.cmake"
         "D:/1224/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/sdl2/CMakeFiles/Export/f084604df1a27ef5b4fef7c7544737d1/SDL2Targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2/SDL2Targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2/SDL2Targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2" TYPE FILE FILES "D:/1224/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/sdl2/CMakeFiles/Export/f084604df1a27ef5b4fef7c7544737d1/SDL2Targets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2" TYPE FILE FILES "D:/1224/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/sdl2/CMakeFiles/Export/f084604df1a27ef5b4fef7c7544737d1/SDL2Targets-debug.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Devel" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2" TYPE FILE FILES
    "D:/1224/ohos_sdl2/SDL2Config.cmake"
    "D:/1224/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/SDL2ConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/SDL2" TYPE FILE FILES
    "D:/1224/ohos_sdl2/include/SDL.h"
    "D:/1224/ohos_sdl2/include/SDL_assert.h"
    "D:/1224/ohos_sdl2/include/SDL_atomic.h"
    "D:/1224/ohos_sdl2/include/SDL_audio.h"
    "D:/1224/ohos_sdl2/include/SDL_bits.h"
    "D:/1224/ohos_sdl2/include/SDL_blendmode.h"
    "D:/1224/ohos_sdl2/include/SDL_clipboard.h"
    "D:/1224/ohos_sdl2/include/SDL_config_android.h"
    "D:/1224/ohos_sdl2/include/SDL_config_iphoneos.h"
    "D:/1224/ohos_sdl2/include/SDL_config_macosx.h"
    "D:/1224/ohos_sdl2/include/SDL_config_minimal.h"
    "D:/1224/ohos_sdl2/include/SDL_config_ohos.h"
    "D:/1224/ohos_sdl2/include/SDL_config_os2.h"
    "D:/1224/ohos_sdl2/include/SDL_config_pandora.h"
    "D:/1224/ohos_sdl2/include/SDL_config_psp.h"
    "D:/1224/ohos_sdl2/include/SDL_config_windows.h"
    "D:/1224/ohos_sdl2/include/SDL_config_winrt.h"
    "D:/1224/ohos_sdl2/include/SDL_config_wiz.h"
    "D:/1224/ohos_sdl2/include/SDL_copying.h"
    "D:/1224/ohos_sdl2/include/SDL_cpuinfo.h"
    "D:/1224/ohos_sdl2/include/SDL_egl.h"
    "D:/1224/ohos_sdl2/include/SDL_endian.h"
    "D:/1224/ohos_sdl2/include/SDL_error.h"
    "D:/1224/ohos_sdl2/include/SDL_events.h"
    "D:/1224/ohos_sdl2/include/SDL_filesystem.h"
    "D:/1224/ohos_sdl2/include/SDL_gamecontroller.h"
    "D:/1224/ohos_sdl2/include/SDL_gesture.h"
    "D:/1224/ohos_sdl2/include/SDL_haptic.h"
    "D:/1224/ohos_sdl2/include/SDL_hints.h"
    "D:/1224/ohos_sdl2/include/SDL_joystick.h"
    "D:/1224/ohos_sdl2/include/SDL_keyboard.h"
    "D:/1224/ohos_sdl2/include/SDL_keycode.h"
    "D:/1224/ohos_sdl2/include/SDL_loadso.h"
    "D:/1224/ohos_sdl2/include/SDL_log.h"
    "D:/1224/ohos_sdl2/include/SDL_main.h"
    "D:/1224/ohos_sdl2/include/SDL_messagebox.h"
    "D:/1224/ohos_sdl2/include/SDL_metal.h"
    "D:/1224/ohos_sdl2/include/SDL_mouse.h"
    "D:/1224/ohos_sdl2/include/SDL_mutex.h"
    "D:/1224/ohos_sdl2/include/SDL_name.h"
    "D:/1224/ohos_sdl2/include/SDL_opengl.h"
    "D:/1224/ohos_sdl2/include/SDL_opengl_glext.h"
    "D:/1224/ohos_sdl2/include/SDL_opengles.h"
    "D:/1224/ohos_sdl2/include/SDL_opengles2.h"
    "D:/1224/ohos_sdl2/include/SDL_opengles2_gl2.h"
    "D:/1224/ohos_sdl2/include/SDL_opengles2_gl2ext.h"
    "D:/1224/ohos_sdl2/include/SDL_opengles2_gl2platform.h"
    "D:/1224/ohos_sdl2/include/SDL_opengles2_khrplatform.h"
    "D:/1224/ohos_sdl2/include/SDL_pixels.h"
    "D:/1224/ohos_sdl2/include/SDL_platform.h"
    "D:/1224/ohos_sdl2/include/SDL_power.h"
    "D:/1224/ohos_sdl2/include/SDL_quit.h"
    "D:/1224/ohos_sdl2/include/SDL_rect.h"
    "D:/1224/ohos_sdl2/include/SDL_render.h"
    "D:/1224/ohos_sdl2/include/SDL_revision.h"
    "D:/1224/ohos_sdl2/include/SDL_rwops.h"
    "D:/1224/ohos_sdl2/include/SDL_scancode.h"
    "D:/1224/ohos_sdl2/include/SDL_sensor.h"
    "D:/1224/ohos_sdl2/include/SDL_shape.h"
    "D:/1224/ohos_sdl2/include/SDL_stdinc.h"
    "D:/1224/ohos_sdl2/include/SDL_surface.h"
    "D:/1224/ohos_sdl2/include/SDL_system.h"
    "D:/1224/ohos_sdl2/include/SDL_syswm.h"
    "D:/1224/ohos_sdl2/include/SDL_test.h"
    "D:/1224/ohos_sdl2/include/SDL_test_assert.h"
    "D:/1224/ohos_sdl2/include/SDL_test_common.h"
    "D:/1224/ohos_sdl2/include/SDL_test_compare.h"
    "D:/1224/ohos_sdl2/include/SDL_test_crc32.h"
    "D:/1224/ohos_sdl2/include/SDL_test_font.h"
    "D:/1224/ohos_sdl2/include/SDL_test_fuzzer.h"
    "D:/1224/ohos_sdl2/include/SDL_test_harness.h"
    "D:/1224/ohos_sdl2/include/SDL_test_images.h"
    "D:/1224/ohos_sdl2/include/SDL_test_log.h"
    "D:/1224/ohos_sdl2/include/SDL_test_md5.h"
    "D:/1224/ohos_sdl2/include/SDL_test_memory.h"
    "D:/1224/ohos_sdl2/include/SDL_test_random.h"
    "D:/1224/ohos_sdl2/include/SDL_thread.h"
    "D:/1224/ohos_sdl2/include/SDL_timer.h"
    "D:/1224/ohos_sdl2/include/SDL_touch.h"
    "D:/1224/ohos_sdl2/include/SDL_types.h"
    "D:/1224/ohos_sdl2/include/SDL_version.h"
    "D:/1224/ohos_sdl2/include/SDL_video.h"
    "D:/1224/ohos_sdl2/include/SDL_vulkan.h"
    "D:/1224/ohos_sdl2/include/begin_code.h"
    "D:/1224/ohos_sdl2/include/close_code.h"
    "D:/1224/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/sdl2/include/SDL_config.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "D:/1224/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/sdl2/sdl2.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "D:/1224/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/sdl2/sdl2-config")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/Program Files (x86)/entry/share/aclocal/sdl2.m4")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/Program Files (x86)/entry/share/aclocal" TYPE FILE FILES "D:/1224/ohos_sdl2/sdl2.m4")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("D:/1224/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/cjson/cmake_install.cmake")

endif()

