#include <curl/curl.h>
#include <curl/easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "http-server.h"
#include "jansson.h"
#include "nanojpeg.c"
#include "spotify.h"
#include "src/jansson.h"

/** Allocate a new empty MemoryBuffer */
ResponseBuffer *response_buffer_new() {
  return response_buffer_new_with_size(0);
}

/** Allocate a new MemoryBuffer, prefilled to the given size. */
ResponseBuffer *response_buffer_new_with_size(size_t size) {
  ResponseBuffer *buf = malloc(sizeof(ResponseBuffer));
  if (!buf)
    return buf;
  buf->contents = malloc(size + 1); // null-terminated
  buf->size = 0;
  return buf;
}

/**
 * Write bytes to the memory buffer
 * @param buf buffer to write to
 * @param data data to read from
 * @param size number of bytes to copy
 * @returns number of written bytes
 */
size_t response_buffer_write_bytes(ResponseBuffer *buf, char *data,
                                   size_t size) {
  char *new_contents = realloc(buf->contents, buf->size + size + 1);
  if (!new_contents)
    return 0; // out of memory (yikes)

  buf->contents = new_contents;
  memcpy(&(buf->contents[buf->size]), data, size);
  buf->size += size;
  buf->contents[buf->size] = 0;
  return size;
}

size_t response_buffer_set_bytes(ResponseBuffer *buf, char *data, size_t size) {
  buf->size = 0;
  return response_buffer_write_bytes(buf, data, size);
}

size_t response_buffer_libcurl_write_function(char *data, size_t size,
                                              size_t nmemb,
                                              ResponseBuffer *buf) {
  size_t chunk_size = size * nmemb;
  return response_buffer_write_bytes(buf, data, chunk_size);
}

/** Free allocated memory for the given buf */
void response_buffer_free(ResponseBuffer *buf) {
  if (!buf)
    return;
  free(buf->contents);
  free(buf);
}

struct spotify_auth_cb_params {
  char res[512];
  char success;
};

int spotify_auth_http_callback(HttpRequest *req, HttpResponse *res, void *out) {
  struct spotify_auth_cb_params *params = out;
  if (strcmp(req->path, "/") == 0 && req->method == HTTP_METHOD_GET) {
    const char *code = http_request_query_get(req, "code");
    if (code) {
      strcpy(params->res, code);
      params->success = 1;

      res->code = 200;
      sprintf(res->body, "Success! You can close this tab now.");
    } else {
      const char *error = http_request_query_get(req, "error");
      strcpy(params->res, error);
      params->success = 0;

      res->code = 401;
      sprintf(res->body, "One of us goofed, and it wasnt me.<br><pre>%s</pre>",
              error);
    }

    return 1;
  }
  return 0;
}

SpotifyAuth *spotify_auth_new_from_oauth(void) {
  // Make OAuth2 authorize endpoints
  printf("Open this URL in your browser:\n");
  printf(SNP_SPOTIFY_AUTH_AUTHORIZE_ENDPOINT
         "?client_id=" SNP_SPOTIFY_AUTH_CLIENT_ID
         "&redirect_uri=" SNP_SPOTIFY_AUTH_REDIRECT_URI
         "&scope=user-read-currently-playing%%20user-read-playback-state"
         "&code_challenge_method=S256"
         "&code_challenge="
         // TODO: compute a random string, then SHA256 -> Base64
         "_-BU_nrgy23GXDr5th1SCfQ5hR20PQulmXM33xVGaOs"
         "&response_type=code"
         "\n\n");

  printf("Waiting for code...\n");
  struct spotify_auth_cb_params result;
  if (http_server_run_until(SNP_SPOTIFY_AUTH_PORT, spotify_auth_http_callback,
                            &result) != 0) {
    perror("http_server_run_until");
    return NULL;
  }

  if (!result.success) {
    fprintf(stderr, "unable to retrieve authorization code: %s\n", result.res);
    return NULL;
  }
  char *authorization_code = result.res;

  char request_body[512];
  sprintf(request_body,
          "grant_type=authorization_code"
          "&code=%s"
          "&redirect_uri=" SNP_SPOTIFY_AUTH_REDIRECT_URI
          "&client_id=" SNP_SPOTIFY_AUTH_CLIENT_ID "&code_verifier="
          // TODO: use computed random string from above
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          authorization_code);

  CURL *curl = curl_easy_init();
  CURLcode res;

  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt(curl, CURLOPT_URL, SNP_SPOTIFY_AUTH_TOKEN_ENDPOINT);

  struct curl_slist *headers = curl_slist_append(
      NULL, "Content-Type: application/x-www-form-urlencoded");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);

  ResponseBuffer *response = response_buffer_new();
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                   response_buffer_libcurl_write_function);

  if ((res = curl_easy_perform(curl)) != CURLE_OK) {
    fprintf(stderr, "spotify auth network request failed: %s\n",
            curl_easy_strerror(res));
    goto cleanup_curl;
  }
  long code;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  if (code != 200) {
    fprintf(stderr, "server responded with code: %ld\n", code);
    fprintf(stderr, "%s\n", response->contents);
    goto cleanup_curl;
  }
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);

  json_error_t error;
  json_t *root = json_loads(response->contents, 0, &error);
  response_buffer_free(response);

  if (!root) {
    fprintf(stderr, "unable to parse response json: line %d\n%s\n", error.line,
            error.text);
    return NULL;
  }

  SpotifyAuth *auth = malloc(sizeof(SpotifyAuth));
  strcpy(auth->access_token,
         json_string_value(json_object_get(root, "access_token")));
  strcpy(auth->refresh_token,
         json_string_value(json_object_get(root, "refresh_token")));
  auth->expires_at =
      time(NULL) + json_integer_value(json_object_get(root, "expires_in"));

  json_decref(root);
  return auth;

cleanup_curl:
  response_buffer_free(response);
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);
  return NULL;
}

void spotify_auth_free(SpotifyAuth *auth) { free(auth); }

void spotify_auth_refresh_if_required(SpotifyAuth *auth) {
  if (!auth)
    return;
  // TODO: implement
}

json_t *spotify_api_get(const char *endpoint, SpotifyAuth *auth) {
  if (!auth) {
    fprintf(stderr, "no auth session found\n");
    exit(1);
  }
  spotify_auth_refresh_if_required(auth);

  CURL *curl = curl_easy_init();
  CURLcode res;

  curl_easy_setopt(curl, CURLOPT_URL, endpoint);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

  char authorization_header[512];
  sprintf(authorization_header, "Authorization: Bearer %s", auth->access_token);
  struct curl_slist *headers = curl_slist_append(NULL, authorization_header);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  ResponseBuffer *response = response_buffer_new();
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                   response_buffer_libcurl_write_function);

  if ((res = curl_easy_perform(curl)) != CURLE_OK) {
    fprintf(stderr, "spotify api network request failed: %s\n",
            curl_easy_strerror(res));
    goto cleanup_curl;
  }
  long code;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  if (code > 299) {
    fprintf(stderr, "server responded with code: %ld\n%s\n", code,
            response->contents);
    goto cleanup_curl;
  }
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (response->size == 0) {
    return NULL;
  }

  json_error_t error;
  json_t *root = json_loads(response->contents, 0, &error);
  response_buffer_free(response);
  if (!root) {
    fprintf(stderr, "unable to parse response json: line %d\n%s\n", error.line,
            error.text);
    return NULL;
  }
  return root;

cleanup_curl:
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  exit(1);
}

SpotifyAlbumCover *spotify_album_cover_from_jpeg(ResponseBuffer *buf) {
  if (!buf)
    return NULL;
  SpotifyAlbumCover *ret = NULL;

  njInit();
  if (njDecode(buf->contents, buf->size)) {
    fprintf(stderr, "error decoding jpeg\n");
    goto cleanup;
  }

  ret = malloc(sizeof(*ret));
  ret->width = njGetWidth();
  ret->height = njGetHeight();
  response_buffer_set_bytes(buf, (char *)njGetImage(), njGetImageSize());
  njDone();
  ret->pixels = (unsigned char *)buf->contents;

  free(buf);

cleanup:
  return ret;
}

void spotify_album_cover_free(SpotifyAlbumCover *album) {
  if (!album)
    return;
  free(album->pixels);
  free(album);
}

ResponseBuffer *response_buffer_new_from_url(const char *url) {
  ResponseBuffer *ret = NULL;
  CURL *curl = curl_easy_init();
  CURLcode res;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

  ResponseBuffer *response = response_buffer_new();
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                   response_buffer_libcurl_write_function);

  if ((res = curl_easy_perform(curl)) != CURLE_OK) {
    fprintf(stderr, "network request failed: %s\n", curl_easy_strerror(res));
    goto cleanup;
  }

  long code;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  if (code > 299) {
    fprintf(stderr, "server responded with code: %ld\n%s\n", code,
            response->contents);
    goto cleanup;
  }

  ret = response;

cleanup:
  curl_easy_cleanup(curl);
  return ret;
}

SpotifyCurrentlyPlaying *spotify_currently_playing_get(SpotifyAuth *auth) {
  SpotifyCurrentlyPlaying *ret = malloc(sizeof(*ret));
  json_t *root = spotify_api_get(SNP_SPOTIFY_API_CURRENTLY_PLAYING, auth);
  ret->__root = root;
  if (!root) {
    printf("Nothing is playing...\n");
    return ret;
  }

  const char *track_type =
      json_string_value(json_object_get(root, "currently_playing_type"));
  if (strcmp(track_type, "track") != 0) {
    printf("Item being played is not a track.\n");
    return ret;
  }

  json_t *item = json_object_get(root, "item");
  json_t *album = json_object_get(item, "album");

  const char *album_url = json_string_value(json_object_get(
      json_array_get(json_object_get(album, "images"), 0), "url"));
  ret->album_name = json_string_value(json_object_get(album, "name"));
  ret->track_name = json_string_value(json_object_get(item, "name"));

  size_t artist_i;
  json_t *artist;
  json_array_foreach(json_object_get(item, "artists"), artist_i, artist) {
    if (artist_i > 2)
      break;
    ret->artists[artist_i] = json_string_value(json_object_get(artist, "name"));
  }

  ResponseBuffer *img_buf = response_buffer_new_from_url(album_url);
  ret->album_cover = spotify_album_cover_from_jpeg(img_buf);

  return ret;
}

void spotify_currently_playing_free(SpotifyCurrentlyPlaying *playing) {
  if (!playing)
    return;
  json_decref(playing->__root);
  spotify_album_cover_free(playing->album_cover);
  free(playing);
}
