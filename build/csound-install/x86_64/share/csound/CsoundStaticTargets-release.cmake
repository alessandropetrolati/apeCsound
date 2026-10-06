#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Csound::CsoundLib64-static" for configuration "Release"
set_property(TARGET Csound::CsoundLib64-static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Csound::CsoundLib64-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C;CXX"
  IMPORTED_LOCATION_RELEASE "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-install/x86_64/lib/libCsoundLib64.a"
  )

list(APPEND _cmake_import_check_targets Csound::CsoundLib64-static )
list(APPEND _cmake_import_check_files_for_Csound::CsoundLib64-static "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-install/x86_64/lib/libCsoundLib64.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
