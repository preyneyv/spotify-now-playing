#include "spotify.h"
#include "constants.h"
#include "http-server.h"
#include "jansson.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int fake_fetch_api(void) {
  ResponseBuffer *response;

  {
    CURL *curl;
    CURLcode res;

    if (!(curl = curl_easy_init())) {
      curl_easy_cleanup(curl);
      return 1;
    }
    curl_easy_setopt(curl, CURLOPT_URL,
                     "https://jsonplaceholder.typicode.com/todos/1");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

    response = response_buffer_new();
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                     response_buffer_libcurl_write_function);

    if ((res = curl_easy_perform(curl)) != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
      curl_easy_cleanup(curl);
      response_buffer_free(response);
      return 1;
    }

    curl_easy_cleanup(curl);
  }

  json_t *root;
  json_error_t error;
  root = json_loads(response->contents, 0, &error);
  response_buffer_free(response);

  if (!root) {
    fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
    return 1;
  }

  if (!json_is_object(root)) {
    fprintf(stderr, "error: not an array\n");
    return 1;
  }

  printf("title from api: %s\n",
         json_string_value(json_object_get(root, "title")));
  json_decref(root);
  printf("is completed?: %s\n",
         json_boolean_value(json_object_get(root, "completed")) ? "yes" : "no");
  json_decref(root);

  return 0;
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
    fprintf(stderr, "unable to parse response json: line %d\n%s", error.line,
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
