/* mz_crypt.h -- Cryptographic functions
   Version 2.6.0, October 8, 2018
   part of the MiniZip project

   Copyright (C) 2010-2018 Nathan Moinvaziri
     https://github.com/nmoinvaz/minizip

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef MZ_CRYPT_H
#define MZ_CRYPT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/

int32_t  mz_crypt_rand(uint8_t *buf, int32_t size);

void     mz_crypt_sha_reset(void *handle);
int32_t  mz_crypt_sha_begin(void *handle);
int32_t  mz_crypt_sha_update(void *handle, const void *buf, int32_t size);
int32_t  mz_crypt_sha_end(void *handle, uint8_t *digest, int32_t digest_size);
void     mz_crypt_sha_set_algorithm(void *handle, uint16_t algorithm);
void*    mz_crypt_sha_create(void **handle);
void     mz_crypt_sha_delete(void **handle);

void     mz_crypt_aes_reset(void *handle);
int32_t  mz_crypt_aes_encrypt(void *handle, uint8_t *buf, int32_t size, int32_t final);
int32_t  mz_crypt_aes_decrypt(void *handle, uint8_t *buf, int32_t size, int32_t final);
int32_t  mz_crypt_aes_set_key(void *handle, const void *key, int32_t key_length);
void     mz_crypt_aes_set_mode(void *handle, int32_t mode);
void     mz_crypt_aes_set_algorithm(void *handle, uint16_t algorithm);
void*    mz_crypt_aes_create(void **handle);
void     mz_crypt_aes_delete(void **handle);

void     mz_crypt_hmac_reset(void *handle);
int32_t  mz_crypt_hmac_begin(void *handle);
int32_t  mz_crypt_hmac_update(void *handle, const void *buf, int32_t size);
int32_t  mz_crypt_hmac_end(void *handle, uint8_t *digest, int32_t digest_size);
int32_t  mz_crypt_hmac_copy(void *src_handle, void *target_handle);
int32_t  mz_crypt_hmac_set_key(void *handle, const void *key, int32_t key_length);
void     mz_crypt_hmac_set_algorithm(void *handle, uint16_t algorithm);
void*    mz_crypt_hmac_create(void **handle);
void     mz_crypt_hmac_delete(void **handle);

int32_t  mz_crypt_sign(uint8_t *message, int32_t message_size, const char *cert_path, const char *cert_pwd,
    const char *timestamp_url, uint8_t **signature, int32_t *signature_size);
int32_t  mz_crypt_sign_verify(uint8_t *message, int32_t message_size, uint8_t *signature, int32_t signature_size);

/***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
