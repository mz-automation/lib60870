cmake_minimum_required(VERSION 2.8)

# automagically detect if we should cross-compile
if(DEFINED ENV{TOOLCHAIN})
	set(CMAKE_C_COMPILER	$ENV{TOOLCHAIN}gcc)
	set(CMAKE_CXX_COMPILER	$ENV{TOOLCHAIN}g++)
	set(CMAKE_AR	"$ENV{TOOLCHAIN}ar" CACHE FILEPATH "CW archiver" FORCE)
endif()

project(lib60870)
ENABLE_TESTING()

set(LIB_VERSION_MAJOR "0")
set(LIB_VERSION_MINOR "9")
set(LIB_VERSION_PATCH "5")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/third_party/cmake/modules/")

# feature checks
include(CheckLibraryExists)
check_library_exists(rt clock_gettime "time.h" CONFIG_SYSTEM_HAS_CLOCK_GETTIME)

# check if we are on a little or a big endian
include (TestBigEndian)
test_big_endian(PLATFORM_IS_BIGENDIAN)


option(BUILD_EXAMPLES "Build the examples" ON)
option(BUILD_TESTS "Build the tests" ON)

include_directories(
    config
    src/inc/api
    src/inc/internal
    src/common/inc
    src/hal/inc
)

set(API_HEADERS 
	src/hal/inc/hal_time.h 
	src/hal/inc/hal_thread.h
	src/hal/inc/hal_filesystem.h 
	src/inc/api/iec60870_master.h
	src/inc/api/iec60870_server.h
	src/inc/api/iec60870_common.h
	src/inc/api/information_objects.h
	src/inc/api/t104_connection.h
)


# write the detected stuff to this file
# configure_file(config/lib60870_config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config/lib60870_config.h)

if(BUILD_EXAMPLES)
	add_subdirectory(examples)
endif(BUILD_EXAMPLES)

if(BUILD_TESTS)
	add_subdirectory(tests)
endif(BUILD_TESTS)

add_subdirectory(src)

INSTALL(FILES ${API_HEADERS} DESTINATION include/lib60870 COMPONENT Development)

IF(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")
INCLUDE(InstallRequiredSystemLibraries)
 
SET(CPACK_PACKAGE_DESCRIPTION "IEC 60870-5-104 client and server library")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "IEC 60870-5-104 client and server library")
SET(CPACK_PACKAGE_VENDOR "MZ Automation GmbH")
SET(CPACK_PACKAGE_CONTACT "info@mz-automation.de")
SET(CPACK_PACKAGE_VERSION_MAJOR "${LIB_VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${LIB_VERSION_MINOR}")
SET(CPACK_PACKAGE_VERSION_PATCH "${LIB_VERSION_PATCH}")
SET(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}_${CMAKE_SYSTEM_PROCESSOR}")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
 
SET(CPACK_COMPONENTS_ALL Libraries Development Applications)
#set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CMAKE_PROJECT_NAME}")
INCLUDE(CPack)
 
ENDIF(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")
