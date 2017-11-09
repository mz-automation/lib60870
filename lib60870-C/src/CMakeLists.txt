
set (lib_common_SRCS
./common/lib_memory.c
./common/linked_list.c
./iec60870/apl/asdu.c
./iec60870/apl/bcr.c
./iec60870/apl/cpXXtime2a.c
./iec60870/apl/information_objects.c
./iec60870/t104/t104_connection.c
./iec60870/t104/t104_frame.c
./iec60870/t104/t104_slave.c
./iec60870/t104/buffer_frame.c
./iec60870/frame.c
./iec60870/lib60870_common.c
)

set (lib_linux_SRCS
./hal/socket/linux/socket_linux.c
./hal/thread/linux/thread_linux.c
./hal/time/unix/time.c
)

set (lib_windows_SRCS
./hal/socket/win32/socket_win32.c
./hal/thread/win32/thread_win32.c
./hal/time/win32/time.c
)

set (lib_bsd_SRCS
./hal/socket/bsd/socket_bsd.c
./hal/thread/bsd/thread_bsd.c
./hal/time/unix/time.c
)

IF(WIN32)

IF(MSVC)
set_source_files_properties(${lib_common_SRCS} ${lib_windows_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF()

set (library_SRCS
    ${lib_common_SRCS}
    ${lib_windows_SRCS}
)

set (CMAKE_MODULE_LINKER_FLAGS   "${CMAKE_MODULE_LINKER_FLAGS}\"/DEF:${CMAKE_CURRENT_SOURCE_DIR}/vs/lib60870.def\"") 

ELSEIF(UNIX)
IF(APPLE)
set (library_SRCS
    ${lib_common_SRCS}
    ${lib_bsd_SRCS}
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
list (APPEND library_SRCS ./tls/mbedtls/tls_mbedtls.c)

ENDIF(WITH_MBEDTLS)

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

add_library (iec60870-shared SHARED ${library_SRCS} )

set_target_properties(iec60870-shared PROPERTIES
           OUTPUT_NAME iec60870
           SOVERSION "${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH}"
)


GENERATE_EXPORT_HEADER(iec60870-shared
			BASE_NAME iec60870-shared
			EXPORT_MACRO_NAME iec60870-shared_EXPORT
			EXPORT_FILE_NAME iec60870-shared_export.h
			STATIC_DEFINE iec60870-shared_BUILT_AS_STATIC
)

add_library (iec60870 STATIC ${library_SRCS})

IF(UNIX)
  IF (CONFIG_SYSTEM_HAS_CLOCK_GETTIME)
     target_link_libraries (iec60870
         -lpthread
         -lm
         -lrt
     )
  ELSE ()
     target_link_libraries (iec60870
         -lpthread
         -lm
     )
  ENDIF (CONFIG_SYSTEM_HAS_CLOCK_GETTIME)
ENDIF(UNIX)
IF(MINGW)
  target_link_libraries(iec60870-shared ws2_32 iphlpapi)
  target_link_libraries(iec60870 ws2_32 iphlpapi)
ENDIF(MINGW)



install (TARGETS iec60870 iec60870-shared
	RUNTIME DESTINATION bin COMPONENT Applications
	ARCHIVE DESTINATION lib COMPONENT Libraries
    LIBRARY DESTINATION lib COMPONENT Libraries
)

