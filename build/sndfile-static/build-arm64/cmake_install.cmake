# Install script for directory: /Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-install/arm64-deps/sndfile")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
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

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/build-arm64/libsndfile.a")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libsndfile.a" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libsndfile.a")
    execute_process(COMMAND "/usr/bin/ranlib" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libsndfile.a")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/include/sndfile.h"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/include/sndfile.hh"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SndFile/SndFileTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SndFile/SndFileTargets.cmake"
         "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/build-arm64/CMakeFiles/Export/5c71f72976042dd672d3a20ad1898c82/SndFileTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SndFile/SndFileTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SndFile/SndFileTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SndFile" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/build-arm64/CMakeFiles/Export/5c71f72976042dd672d3a20ad1898c82/SndFileTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SndFile" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/build-arm64/CMakeFiles/Export/5c71f72976042dd672d3a20ad1898c82/SndFileTargets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SndFile" TYPE FILE RENAME "SndFileConfig.cmake" FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/build-arm64/SndFileConfig2.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SndFile" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/build-arm64/SndFileConfigVersion.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/libsndfile" TYPE FILE FILES
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/index.md"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/libsndfile.jpg"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/libsndfile.css"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/print.css"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/api.md"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/command.md"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/bugs.md"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/formats.md"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/sndfile_info.md"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/new_file_type_howto.md"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/win32.md"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/FAQ.md"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/lists.md"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/embedded_files.md"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/octave.md"
    "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/src/docs/tutorial.md"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/build-arm64/sndfile.pc")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/build-arm64/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/sndfile-static/build-arm64/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
