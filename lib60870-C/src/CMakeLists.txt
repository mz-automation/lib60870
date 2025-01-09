
set (lib_common_SRCS
./file-service/file_server.c
./iec60870/apl/cpXXtime2a.c
./iec60870/cs101/cs101_asdu.c
./iec60870/cs101/cs101_bcr.c
./iec60870/cs101/cs101_information_objects.c
./iec60870/cs101/cs101_master_connection.c
./iec60870/cs101/cs101_master.c
./iec60870/cs101/cs101_queue.c
./iec60870/cs101/cs101_slave.c
./iec60870/cs104/cs104_connection.c
./iec60870/cs104/cs104_frame.c
./iec60870/cs104/cs104_slave.c
./iec60870/link_layer/buffer_frame.c
./iec60870/link_layer/link_layer.c
./iec60870/link_layer/serial_transceiver_ft_1_2.c
./iec60870/frame.c
./iec60870/lib60870_common.c
)

if (BUILD_COMMON)
list (APPEND lib_common_SRCS ./common/linked_list.c)
endif (BUILD_COMMON)

if (BUILD_HAL)

set (lib_linux_SRCS
./hal/serial/linux/serial_port_linux.c
./hal/socket/linux/socket_linux.c
./hal/thread/linux/thread_linux.c
./hal/time/unix/time.c
./hal/memory/lib_memory.c
)

set (lib_windows_SRCS
./hal/serial/win32/serial_port_win32.c
./hal/socket/win32/socket_win32.c
./hal/thread/win32/thread_win32.c
./hal/time/win32/time.c
./hal/memory/lib_memory.c
)

set (lib_bsd_SRCS
./hal/serial/linux/serial_port_linux.c
./hal/socket/bsd/socket_bsd.c
./hal/thread/bsd/thread_bsd.c
./hal/time/unix/time.c
./hal/memory/lib_memory.c
)

set (lib_macos_SRCS
./hal/serial/linux/serial_port_linux.c
./hal/socket/bsd/socket_bsd.c
./hal/thread/macos/thread_macos.c
./hal/time/unix/time.c
./hal/memory/lib_memory.c
)

endif (BUILD_HAL)

IF(WIN32)

IF(MSVC)
set_source_files_properties(${lib_common_SRCS} ${lib_windows_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF()

set (library_SRCS
    ${lib_common_SRCS}
    ${lib_windows_SRCS}
)

set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS}\"/DEF:${CMAKE_CURRENT_SOURCE_DIR}/vs/lib60870.def\"") 

ELSEIF(UNIX)
IF(APPLE)
set (library_SRCS
    ${lib_common_SRCS}
    ${lib_macos_SRCS}
)
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
set (library_SRCS
    ${lib_common_SRCS}
    ${lib_bsd_SRCS}
)
ELSE()
set (library_SRCS
    ${lib_common_SRCS}
    ${lib_linux_SRCS}	
)
ENDIF(APPLE)
ENDIF(WIN32)

IF(WITH_MBEDTLS)

list (APPEND library_SRCS ${tls_SRCS})
list (APPEND library_SRCS ./hal/tls/mbedtls/tls_mbedtls.c)

add_definitions(-DLIB60870_HAS_TLS_SUPPORT=1)

ENDIF(WITH_MBEDTLS)

IF(WITH_MBEDTLS3)

list (APPEND library_SRCS ${tls_SRCS})
list (APPEND library_SRCS ./hal/tls/mbedtls3/tls_mbedtls.c)

add_definitions(-DLIB60870_HAS_TLS_SUPPORT=1)

ENDIF(WITH_MBEDTLS3)

include (GenerateExportHeader)

set(RES_FILES "")
if ( WIN32 )
	# Adding RC resource file for adding information to the archive
	set(RES_FILES "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
	message(STATUS "Generating RC file : ${RES_FILES}")
	configure_file(
			${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in
			${RES_FILES}
			@ONLY)
	if( MINGW )
		set(CMAKE_RC_COMPILER_INIT windres)
		ENABLE_LANGUAGE(RC)
		SET(CMAKE_RC_COMPILE_OBJECT
		"<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
	endif(MINGW)
	set(library_SRCS ${library_SRCS} ${RES_FILES})
endif( WIN32 )

add_library (lib60870-shared SHARED ${library_SRCS} )

set_target_properties(lib60870-shared PROPERTIES
           OUTPUT_NAME lib60870
           SOVERSION "${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH}"
	   WINDOWS_EXPORT_ALL_SYMBOLS true
)


GENERATE_EXPORT_HEADER(lib60870-shared
			BASE_NAME lib60870-shared
			EXPORT_MACRO_NAME lib60870-shared_EXPORT
			EXPORT_FILE_NAME lib60870-shared_export.h
			STATIC_DEFINE lib60870-shared_BUILT_AS_STATIC
)

add_library (lib60870 STATIC ${library_SRCS})

IF(UNIX)
    target_link_libraries (lib60870
        -lpthread
        -lm
        -lrt
    )

  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/lib60870.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/lib60870.pc @ONLY
  )
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/lib60870.pc" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/pkgconfig")
ENDIF(UNIX)
IF(MINGW)
  target_link_libraries(lib60870-shared ws2_32 iphlpapi bcrypt)
  target_link_libraries(lib60870 ws2_32 iphlpapi bcrypt)
ENDIF(MINGW)
IF(MSVC)
  target_link_libraries(lib60870-shared ws2_32 iphlpapi bcrypt)
  target_link_libraries(lib60870 ws2_32 iphlpapi bcrypt)
ENDIF(MSVC)

install (TARGETS lib60870 lib60870-shared
	RUNTIME DESTINATION bin COMPONENT Applications
	ARCHIVE DESTINATION lib COMPONENT Libraries
    LIBRARY DESTINATION lib COMPONENT Libraries
)
