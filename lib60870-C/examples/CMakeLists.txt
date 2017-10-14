add_subdirectory(simple_client)
add_subdirectory(simple_server)
add_subdirectory(multi_client_server)

if (WITH_MBEDTLS)
add_subdirectory(tls_client)
add_subdirectory(tls_server)
endif (WITH_MBEDTLS)

