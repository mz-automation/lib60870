cmake_minimum_required(VERSION 3.23)

project(lib60870-C VERSION 2.3.1 LANGUAGES C
        DESCRIPTION "IEC 60870-5-101/104 master/slave library"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cmake/modules")

set(dependencyMbedtlsPath "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/mbedtls-2.16.12")

include(CMakeDependentOption)
option(LIB60870_BUILD_HAL "Build the platform abstraction layer (HAL)" ON)
option(LIB60870_BUILD_COMMON "Build common code (shared with other libraries - e.g. libiec61850)" ON)
option(LIB60870_BUILD_EXAMPLES "Build the examples" ON)
option(LIB60870_BUILD_TESTS "Build the tests" ON)
if(EXISTS "${dependencyMbedtlsPath}")
  set(defaultWithMbedtls ON)
else()
  set(defaultWithMbedtls OFF)
endif()
cmake_dependent_option(LIB60870_WITH_MBEDTLS "Use mbedtls for TLS support" ${defaultWithMbedtls} "LIB60870_BUILD_HAL" OFF)
unset(defaultWithMbedtls)

if(PROJECT_IS_TOP_LEVEL AND LIB60870_BUILD_TESTS)
  include(CTest)
endif()
include(GNUInstallDirs)

if(LIB60870_WITH_MBEDTLS)
  if(EXISTS "${dependencyMbedtlsPath}")
    add_subdirectory("${dependencyMbedtlsPath}")
  else()
    include(FetchContent)
    FetchContent_Declare(mbedtls
      GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
      GIT_TAG cf4667126010c665341f9e50ef691b7ef8294188
    )
    FetchContent_MakeAvailable(mbedtls)
    # Update the path to be able to add the currently missing include path usage requirement
    FetchContent_GetProperties(mbedtls SOURCE_DIR dependencyMbedtlsPath)
  endif()
endif()

add_subdirectory(src)
if(LIB60870_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
if(BUILD_TESTING AND LIB60870_BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(PROJECT_IS_TOP_LEVEL)
  include(InstallRequiredSystemLibraries)

  set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
  set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_DESCRIPTION}")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
  set(CPACK_PACKAGE_VENDOR "MZ Automation GmbH")
  set(CPACK_PACKAGE_CONTACT "info@mz-automation.de")
  set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
  set(CPACK_VERBATIM_VARIABLES YES)
  set(CPACK_COMPONENTS_ALL Libraries Development Applications)
  include(CPack)
endif()
