add_library(lib60870)
add_library(lib60870::lib60870 ALIAS lib60870)
set_target_properties(lib60870 PROPERTIES
  OUTPUT_NAME lib60870
  SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_sources(lib60870
  PRIVATE
    file-service/file_server.c
    iec60870/apl/cpXXtime2a.c
    iec60870/cs101/cs101_asdu.c
    iec60870/cs101/cs101_bcr.c
    iec60870/cs101/cs101_information_objects.c
    iec60870/cs101/cs101_master_connection.c
    iec60870/cs101/cs101_master.c
    iec60870/cs101/cs101_queue.c
    iec60870/cs101/cs101_slave.c
    iec60870/cs104/cs104_connection.c
    iec60870/cs104/cs104_frame.c
    iec60870/cs104/cs104_slave.c
    iec60870/link_layer/buffer_frame.c
    iec60870/link_layer/link_layer.c
    iec60870/link_layer/serial_transceiver_ft_1_2.c
    iec60870/frame.c
    iec60870/lib60870_common.c
)

target_include_directories(lib60870
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/file-service>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/api>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/lib60870>"
  PRIVATE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/config>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/internal>"
)

if(LIB60870_BUILD_COMMON)
  target_sources(lib60870
    PRIVATE
      common/linked_list.c
  )

  target_include_directories(lib60870
    PUBLIC
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/common/inc>"
  )
endif()

if(LIB60870_BUILD_HAL)
  include(CheckLibraryExists)
  check_library_exists(rt clock_gettime "time.h" CONFIG_SYSTEM_HAS_CLOCK_GETTIME)

  find_package(Threads REQUIRED)

  target_compile_definitions(lib60870
    PRIVATE
      $<$<STREQUAL:${CMAKE_C_BYTE_ORDER},BIG_ENDIAN>:PLATFORM_IS_BIGENDIAN=1>
  )

  target_include_directories(lib60870
    PUBLIC
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hal/inc>"
  )

  target_link_libraries(lib60870
    PRIVATE
      Threads::Threads
  )

  if(WIN32)
    configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

    target_sources(lib60870
      PRIVATE
        hal/serial/win32/serial_port_win32.c
        hal/socket/win32/socket_win32.c
        hal/thread/win32/thread_win32.c
        hal/time/win32/time.c
        hal/memory/lib_memory.c
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
    )

    target_link_libraries(lib60870
      PRIVATE
        ws2_32
        iphlpapi
    )
  elseif(UNIX)
    if(APPLE)
      target_sources(lib60870
        PRIVATE
          hal/serial/linux/serial_port_linux.c
          hal/socket/bsd/socket_bsd.c
          hal/thread/macos/thread_macos.c
          hal/time/unix/time.c
          hal/memory/lib_memory.c
      )
    elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
      target_sources(lib60870
        PRIVATE
          hal/serial/linux/serial_port_linux.c
          hal/socket/bsd/socket_bsd.c
          hal/thread/bsd/thread_bsd.c
          hal/time/unix/time.c
          hal/memory/lib_memory.c
      )
    else()
      target_sources(lib60870
        PRIVATE
          hal/serial/linux/serial_port_linux.c
          hal/socket/linux/socket_linux.c
          hal/thread/linux/thread_linux.c
          hal/time/unix/time.c
          hal/memory/lib_memory.c
      )
    endif()

    target_link_libraries(lib60870
      PRIVATE
        $<$<BOOL:${CONFIG_SYSTEM_HAS_CLOCK_GETTIME}>:rt>
        m
    )
  endif()
endif()

if(LIB60870_WITH_MBEDTLS)
  target_sources(lib60870
    PRIVATE
      hal/tls/mbedtls/tls_mbedtls.c
  )
  target_compile_definitions(lib60870
    PUBLIC
      LIB60870_HAS_TLS_SUPPORT=1
    PRIVATE
      CONFIG_CS104_SUPPORT_TLS=1
      MBEDTLS_CONFIG_FILE="mbedtls_config.h"
  )

  target_include_directories(lib60870
    PRIVATE
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hal/tls/mbedtls>"
      # TODO: The currently used mbedtls version does not carry this usage requirement
      "$<BUILD_INTERFACE:${dependencyMbedtlsPath}/include>"
  )

  target_link_libraries(lib60870
    PRIVATE
      mbedtls
      mbedx509
      mbedcrypto
  )
endif()

install(TARGETS lib60870
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Applications
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Libraries
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Libraries
)
if(UNIX)
  configure_file(lib60870.pc.in "${CMAKE_CURRENT_BINARY_DIR}/lib60870.pc" @ONLY)
  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/lib60870.pc" DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig COMPONENT Libraries
  )
endif()
