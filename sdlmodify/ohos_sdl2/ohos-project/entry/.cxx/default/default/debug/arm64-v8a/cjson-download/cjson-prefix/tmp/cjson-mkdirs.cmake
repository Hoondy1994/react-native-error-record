# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/0730/ohos_sdl2/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/cjson-source"
  "D:/0730/ohos_sdl2/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/cjson"
  "D:/0730/ohos_sdl2/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/cjson-download/cjson-prefix"
  "D:/0730/ohos_sdl2/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/cjson-download/cjson-prefix/tmp"
  "D:/0730/ohos_sdl2/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/cjson-download/cjson-prefix/src/cjson-stamp"
  "D:/0730/ohos_sdl2/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/cjson-download/cjson-prefix/src"
  "D:/0730/ohos_sdl2/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/cjson-download/cjson-prefix/src/cjson-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/0730/ohos_sdl2/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/cjson-download/cjson-prefix/src/cjson-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/0730/ohos_sdl2/ohos_sdl2/ohos-project/entry/.cxx/default/default/debug/arm64-v8a/cjson-download/cjson-prefix/src/cjson-stamp${cfgdir}") # cfgdir has leading slash
endif()
