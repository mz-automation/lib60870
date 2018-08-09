/*
 * tls_api.h
 *
 * TLS Configuration API for protocol stacks using TCP/IP
 *
 * Copyright 2017-2018 MZ Automation GmbH
 *
 * Abstraction layer for configuration of different TLS implementations
 *
 */

#ifndef SRC_TLS_CONFIG_H_
#define SRC_TLS_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * \file tls_config.h
 * \brief TLS API functions
 */

/*! \addtogroup hal Platform (Hardware/OS) abstraction layer
   *
   *  @{
   */

/**
 * @defgroup TLS_CONFIG_API TLS configuration
 *
 * @{
 */

typedef struct sTLSConfiguration* TLSConfiguration;

TLSConfiguration
TLSConfiguration_create(void);

/* will be called by stack automatically */
void
TLSConfiguration_setClientMode(TLSConfiguration self);

void
TLSConfiguration_setChainValidation(TLSConfiguration self, bool value);

void
TLSConfiguration_setAllowOnlyKnownCertificates(TLSConfiguration self, bool value);

bool
TLSConfiguration_setOwnCertificate(TLSConfiguration self, uint8_t* certificate, int certLen);

bool
TLSConfiguration_setOwnCertificateFromFile(TLSConfiguration self, const char* filename);

bool
TLSConfiguration_setOwnKey(TLSConfiguration self, uint8_t* key, int keyLen, const char* keyPassword);

bool
TLSConfiguration_setOwnKeyFromFile(TLSConfiguration self, const char* filename, const char* keyPassword);

bool
TLSConfiguration_addAllowedCertificate(TLSConfiguration self, uint8_t* certifcate, int certLen);

bool
TLSConfiguration_addAllowedCertificateFromFile(TLSConfiguration self, const char* filename);

bool
TLSConfiguration_addCACertificate(TLSConfiguration self, uint8_t* certifcate, int certLen);

bool
TLSConfiguration_addCACertificateFromFile(TLSConfiguration self, const char* filename);

void
TLSConfiguration_destroy(TLSConfiguration self);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* SRC_TLS_CONFIG_H_ */
