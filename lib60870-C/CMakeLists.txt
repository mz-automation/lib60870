cmake_minimum_required(VERSION 3.23)

project(lib60870-C VERSION 2.3.3 LANGUAGES C
        DESCRIPTION "IEC 60870-5-101/104 master/slave library"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cmake/modules")

set(dependencyMbedtlsPath "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/mbedtls-2.28.2")

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
option(LIB60870_INSTALL "Create installation targets" ${PROJECT_IS_TOP_LEVEL})

if(PROJECT_IS_TOP_LEVEL AND LIB60870_BUILD_TESTS)
  include(CTest)
endif()
include(GNUInstallDirs)

if(LIB60870_WITH_MBEDTLS)
  if(EXISTS "${dependencyMbedtlsPath}")
    add_subdirectory("${dependencyMbedtlsPath}")
  else()
    include(FetchContent)
    FetchContent_Declare(mbedtls-extern
      GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
      GIT_TAG 89f040a5c938985c5f30728baed21e49d0846a53
    )
    FetchContent_MakeAvailable(mbedtls-extern)
    FetchContent_GetProperties(mbedtls-extern SOURCE_DIR dependencyMbedtlsPath)
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
