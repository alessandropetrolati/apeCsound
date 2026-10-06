#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Csound::CsoundLib64" for configuration "Release"
set_property(TARGET Csound::CsoundLib64 APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Csound::CsoundLib64 PROPERTIES
  IMPORTED_LOCATION_RELEASE "/Users/alessandropetrolati/Library/Frameworks/CsoundLib64.framework/Versions/7.0/CsoundLib64"
  IMPORTED_SONAME_RELEASE "@rpath/CsoundLib64.framework/Versions/7.0/CsoundLib64"
  )

list(APPEND _cmake_import_check_targets Csound::CsoundLib64 )
list(APPEND _cmake_import_check_files_for_Csound::CsoundLib64 "/Users/alessandropetrolati/Library/Frameworks/CsoundLib64.framework/Versions/7.0/CsoundLib64" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
