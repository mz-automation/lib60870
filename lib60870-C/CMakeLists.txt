cmake_minimum_required(VERSION 3.0)

# automagically detect if we should cross-compile
if(DEFINED ENV{TOOLCHAIN})
	set(CMAKE_C_COMPILER	$ENV{TOOLCHAIN}gcc)
	set(CMAKE_CXX_COMPILER	$ENV{TOOLCHAIN}g++)
	set(CMAKE_AR	"$ENV{TOOLCHAIN}ar" CACHE FILEPATH "CW archiver" FORCE)
endif()

project(lib60870-C)
ENABLE_TESTING()

set(LIB_VERSION_MAJOR "2")
set(LIB_VERSION_MINOR "3")
set(LIB_VERSION_PATCH "5")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/third_party/cmake/modules/")

macro(ADD_C_FLAGS flags)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flags}")
endmacro()

# feature checks
include(CheckLibraryExists)

# check if we are on a little or a big endian
include (TestBigEndian)
test_big_endian(PLATFORM_IS_BIGENDIAN)

option(BUILD_HAL "Build the platform abstraction layer (HAL)" ON)
option(BUILD_COMMON "Build common code (shared with other libraries - e.g. libiec61850)" ON)

option(BUILD_EXAMPLES "Build the examples" ON)
option(BUILD_TESTS "Build the tests" ON)

if(BUILD_HAL)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/dependencies/mbedtls-2.28)
set(WITH_MBEDTLS 1)
message("mbedtls 2.28 found")
ELSEIF(EXISTS ${CMAKE_CURRENT_LIST_DIR}/dependencies/mbedtls-3.6)
set(WITH_MBEDTLS3 1)
message("mbedtls 3.6 found")
else()
message("NOTE: mbedtls 2.28 or 3.6 is required for TLS support!")
endif(EXISTS ${CMAKE_CURRENT_LIST_DIR}/dependencies/mbedtls-2.28)

endif(BUILD_HAL)

include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${CMAKE_CURRENT_LIST_DIR}/src/file-service
    ${CMAKE_CURRENT_LIST_DIR}/src/inc/api
    ${CMAKE_CURRENT_LIST_DIR}/src/inc/internal
    ${CMAKE_CURRENT_LIST_DIR}/src/common/inc
    ${CMAKE_CURRENT_LIST_DIR}/src/hal/inc
)

if(WITH_MBEDTLS)
include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/src/hal/tls/mbedtls
    ${CMAKE_CURRENT_LIST_DIR}/dependencies/mbedtls-2.28/include
)

file(GLOB tls_SRCS ${CMAKE_CURRENT_LIST_DIR}/dependencies/mbedtls-2.28/library/*.c)

add_definitions(-DCONFIG_CS104_SUPPORT_TLS=1)
add_definitions(-DMBEDTLS_CONFIG_FILE="mbedtls_config.h")

endif(WITH_MBEDTLS)

if(WITH_MBEDTLS3)
include_directories(
	${CMAKE_CURRENT_LIST_DIR}/src/hal/tls/mbedtls3
	${CMAKE_CURRENT_LIST_DIR}/dependencies/mbedtls-3.6/include
)

file(GLOB tls_SRCS ${CMAKE_CURRENT_LIST_DIR}/dependencies/mbedtls-3.6/library/*.c)

add_definitions(-DCONFIG_CS104_SUPPORT_TLS=1)
add_definitions(-DMBEDTLS_CONFIG_FILE="mbedtls_config.h")

endif(WITH_MBEDTLS3)


set(API_HEADERS 
	${CMAKE_CURRENT_LIST_DIR}/src/hal/inc/hal_time.h 
	${CMAKE_CURRENT_LIST_DIR}/src/hal/inc/hal_thread.h
	${CMAKE_CURRENT_LIST_DIR}/src/hal/inc/hal_socket.h
	${CMAKE_CURRENT_LIST_DIR}/src/hal/inc/hal_serial.h
	${CMAKE_CURRENT_LIST_DIR}/src/hal/inc/hal_base.h
	${CMAKE_CURRENT_LIST_DIR}/src/hal/inc/tls_config.h
	${CMAKE_CURRENT_LIST_DIR}/src/hal/inc/tls_ciphers.h
	${CMAKE_CURRENT_LIST_DIR}/src/common/inc/linked_list.h
	${CMAKE_CURRENT_LIST_DIR}/src/inc/api/cs101_master.h
	${CMAKE_CURRENT_LIST_DIR}/src/inc/api/cs101_slave.h
	${CMAKE_CURRENT_LIST_DIR}/src/inc/api/cs104_slave.h
	${CMAKE_CURRENT_LIST_DIR}/src/inc/api/iec60870_master.h
	${CMAKE_CURRENT_LIST_DIR}/src/inc/api/iec60870_slave.h
	${CMAKE_CURRENT_LIST_DIR}/src/inc/api/iec60870_common.h
	${CMAKE_CURRENT_LIST_DIR}/src/inc/api/cs101_information_objects.h
	${CMAKE_CURRENT_LIST_DIR}/src/inc/api/cs104_connection.h
	${CMAKE_CURRENT_LIST_DIR}/src/inc/api/link_layer_parameters.h
	${CMAKE_CURRENT_LIST_DIR}/src/file-service/cs101_file_service.h
)

include(CheckCCompilerFlag)

check_c_compiler_flag("-Wredundant-decls" SUPPORT_REDUNDANT_DECLS)
if (SUPPORT_REDUNDANT_DECLS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wredundant-decls")
endif(SUPPORT_REDUNDANT_DECLS)

# write the detected stuff to this file
# configure_file(config/lib60870_config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config/lib60870_config.h)

if(BUILD_EXAMPLES)
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/examples)
endif(BUILD_EXAMPLES)

if(BUILD_TESTS)
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tests)
endif(BUILD_TESTS)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/src)

INSTALL(FILES ${API_HEADERS} DESTINATION include/lib60870 COMPONENT Development)

IF(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")
INCLUDE(InstallRequiredSystemLibraries)
 
SET(CPACK_PACKAGE_DESCRIPTION "IEC 60870-5-101/104 master/slave library")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "IEC 60870-5-101/104 master/slave library")
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
