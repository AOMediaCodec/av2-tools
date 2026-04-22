# GitVersion.cmake — Derive build version from git, keep PROJECT_VERSION untouched.
#
# Sets:
#   AV2_OBU_PACKAGE_VERSION  — stable package version (= PROJECT_VERSION)
#   AV2_OBU_BUILD_VERSION    — git-derived provenance string, or PROJECT_VERSION as fallback

set(AV2_OBU_PACKAGE_VERSION "${PROJECT_VERSION}")
set(AV2_OBU_BUILD_VERSION "${PROJECT_VERSION}")  # default fallback

# --- Attempt to get version from git describe ---
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  find_program(GIT_EXECUTABLE git)
  if(GIT_EXECUTABLE)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE _git_describe
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE _git_result
    )
    if(_git_result EQUAL 0 AND _git_describe)
      # Strip leading 'v' if present (v1.2.3 -> 1.2.3)
      string(REGEX REPLACE "^v" "" AV2_OBU_BUILD_VERSION "${_git_describe}")
    endif()
  endif()
endif()

message(STATUS "av2_obu package version: ${AV2_OBU_PACKAGE_VERSION}")
message(STATUS "av2_obu build version:   ${AV2_OBU_BUILD_VERSION}")
