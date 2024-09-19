#ifndef __SNP_SPOTIFY_H__
#define __SNP_SPOTIFY_H__

#include <curl/curl.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "constants.h"

/** Null-terminated resizable buffer. */
typedef struct {
  char *contents;
  size_t size;
} ResponseBuffer;

/** Allocate a new empty MemoryBuffer */
ResponseBuffer *response_buffer_new();

/** Allocate a new MemoryBuffer, prefilled to the given size. */
ResponseBuffer *response_buffer_new_with_size(size_t size);

/**
 * Write bytes to the memory buffer
 * @param buf buffer to write to
 * @param data data to read from
 * @param size number of bytes to copy
 * @returns number of written bytes
 */
size_t response_buffer_write_bytes(ResponseBuffer *buf, char *data,
                                   size_t size);

size_t response_buffer_libcurl_write_function(char *data, size_t size,
                                              size_t nmemb,
                                              ResponseBuffer *buf);

/** Free allocated memory for the given buf */
void response_buffer_free(ResponseBuffer *buf);

typedef struct {
  char access_token[256];
  char refresh_token[160];
  time_t expires_at;
} SpotifyAuth;

SpotifyAuth *spotify_auth_new_from_oauth(void);
void spotify_auth_free(SpotifyAuth *auth);

#endif /* __SNP_SPOTIFY_H__ */
