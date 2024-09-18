#include <chafa.h>
// #include <libsoup/soup.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <jansson.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "term-util.h"

#define PIX_WIDTH 3
#define PIX_HEIGHT 3
#define N_CHANNELS 4

int fetch_webpage(void) {
  CURL *curl;
  CURLcode res;

  printf("Trying to make a web request...\n");
  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Failed to initialize curl\n");
    curl_easy_cleanup(curl);
    return 1;
  }
  curl_easy_setopt(curl, CURLOPT_URL, "https://www.google.com/");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
  // curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dummy_write_function);
  FILE *out = fopen("test.html", "w");
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
  res = curl_easy_perform(curl);
  fclose(out);

  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));

    curl_easy_cleanup(curl);
    return 1;
  }

  char *ct;
  res = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
  if (res != CURLE_OK) {
    fprintf(stderr, "easy_get_info(CONTENT_TYPE) failed: %s\n",
            curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    return 1;
  }
  long code;
  res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  if (res != CURLE_OK) {
    fprintf(stderr, "easy_get_info(RESPONSE_CODE) failed: %s\n",
            curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    return 1;
  }
  printf("%ld: %s\n", code, ct);
  return 0;
}

/** Null-terminated resizable buffer. */
typedef struct {
  char *contents;
  size_t size;
} MemoryBuffer;

/** Free allocated memory for the given buf */
static void memory_buffer_free(MemoryBuffer *buf) {
  if (!buf)
    return;
  free(buf->contents);
  free(buf);
}

/** Allocate a new MemoryBuffer, prefilled to the given size. */
static MemoryBuffer *memory_buffer_new_with_size(size_t size) {
  MemoryBuffer *buf = malloc(sizeof(MemoryBuffer));
  if (!buf)
    return buf;
  buf->contents = malloc(size + 1); // null-terminated
  buf->size = 0;
  return buf;
}

/** Allocate a new empty MemoryBuffer */
static MemoryBuffer *memory_buffer_new() {
  return memory_buffer_new_with_size(0);
}

/**
 * Write bytes to the memory buffer
 * @param buf buffer to write to
 * @param data data to read from
 * @param size number of bytes to copy
 * @returns number of written bytes
 */
static size_t memory_buffer_write_bytes(MemoryBuffer *buf, char *data,
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

static size_t memory_buffer_libcurl_write_function(char *data, size_t size,
                                                   size_t nmemb,
                                                   MemoryBuffer *buf) {
  size_t chunk_size = size * nmemb;
  return memory_buffer_write_bytes(buf, data, chunk_size);
}

int fake_fetch_api(void) {
  MemoryBuffer *response;

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

    response = memory_buffer_new();
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                     memory_buffer_libcurl_write_function);

    if ((res = curl_easy_perform(curl)) != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
      curl_easy_cleanup(curl);
      memory_buffer_free(response);
      return 1;
    }

    curl_easy_cleanup(curl);
  }

  json_t *root;
  json_error_t error;
  root = json_loads(response->contents, 0, &error);
  memory_buffer_free(response);

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

int main(void) {
  const guint8 pixels[PIX_WIDTH * PIX_HEIGHT * N_CHANNELS] = {
      0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff,
      0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff,
      0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff};

  ChafaSymbolMap *symbol_map;
  ChafaCanvasConfig *config;
  ChafaTermInfo *term_info;
  ChafaCanvas *canvas;

  ChafaCanvasMode mode;
  ChafaPixelMode pixel_mode;

  gfloat font_ratio = 0.5;
  gint cell_width = -1, cell_height = -1;
  gint width_cells, height_cells;
  TermSize term_size = get_tty_size();

  if (term_size.width_cells > 0 && term_size.height_cells > 0 &&
      term_size.width_pixels > 0 && term_size.height_pixels > 0) {
    cell_width = term_size.width_pixels / term_size.width_cells;
    cell_height = term_size.height_pixels / term_size.height_cells;
    font_ratio = (gdouble)cell_width / (gdouble)cell_height;
  }

  width_cells = 80;
  height_cells = 20;
  chafa_calc_canvas_geometry(PIX_WIDTH, PIX_HEIGHT, &width_cells, &height_cells,
                             font_ratio, TRUE, FALSE);
  detect_terminal_mode(&term_info, &mode, &pixel_mode);

  symbol_map = chafa_symbol_map_new();
  chafa_symbol_map_add_by_tags(symbol_map, CHAFA_SYMBOL_TAG_ALL);

  config = chafa_canvas_config_new();
  chafa_canvas_config_set_symbol_map(config, symbol_map);
  chafa_canvas_config_set_canvas_mode(config, mode);
  chafa_canvas_config_set_pixel_mode(config, pixel_mode);
  chafa_canvas_config_set_geometry(config, width_cells, height_cells);
  if (cell_width > 0 && cell_height > 0)
    chafa_canvas_config_set_cell_geometry(config, cell_width, cell_height);

  canvas = chafa_canvas_new(config);

  chafa_canvas_draw_all_pixels(canvas, CHAFA_PIXEL_RGBA8_UNASSOCIATED, pixels,
                               PIX_WIDTH, PIX_HEIGHT, PIX_WIDTH * N_CHANNELS);

  GString *gs = chafa_canvas_print(canvas, term_info);
  fwrite(gs->str, sizeof(char), gs->len, stdout);
  fputc('\n', stdout);
  g_string_free(gs, TRUE);

  chafa_canvas_unref(canvas);
  chafa_term_info_unref(term_info);
  chafa_canvas_config_unref(config);
  chafa_symbol_map_unref(symbol_map);

  printf("look its a web request!\n");
  fake_fetch_api();

  return 0;
}
