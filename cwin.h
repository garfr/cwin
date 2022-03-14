#ifndef CWIN_H
#define CWIN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define CWIN_WINDOW_POS_UNDEFINED 0
#define CWIN_WINDOW_SIZE_UNDEFINED 0

/* The only success case is CWIN_SUCCESS, which is 0, so you can simply check
   for failure with:

   enum cwin_error err;
   err = <cwin function call>;
   if (err)
   {
       <handle error>
   }
*/
enum cwin_error {
  CWIN_SUCCESS = 0,
  CWIN_ERROR_OOM,
  CWIN_ERROR_INVALID_UTF8,

  CWIN_ERROR_WIN32_INTERNAL,
};

struct cwin_event_queue;

struct cwin_window;

struct cwin_window_builder {
  /* A UTF-8 string containing the requested name. If name_len is 0, it is
     null terminated. */
  const uint8_t *name;
  size_t name_len;
  /* If 0, they will be placed at an undefined location. */
  int x, y;
  /* If 0, the height is undefined. */
  int width, height;

  /* If NULL, the default queue is used. */
  struct cwin_event_queue *queue;
};

enum cwin_event_type {
  CWIN_EVENT_WINDOW,
};

enum cwin_window_event_type {
  CWIN_WINDOW_EVENT_CLOSE,
};

struct cwin_window_event {
  enum cwin_window_event_type t;
};

struct cwin_event {
  enum cwin_event_type t;
  union {
    struct cwin_window_event window;
  };
};

enum cwin_raw_window_type {
  CWIN_RAW_WINDOW_WIN32,
};

struct cwin_raw_window {
  enum cwin_raw_window_type t;
  union {
    struct {
      void *hwnd;
      void *hinstance;
    } win32;
  };
};

/* Initializes the library internals. */
enum cwin_error cwin_init(void);

enum cwin_error cwin_create_event_queue(struct cwin_event_queue **out);
void cwin_destroy_event_queue(struct cwin_event_queue *queue);

enum cwin_error cwin_create_window(struct cwin_window **out,
                                   struct cwin_window_builder *builder);

void cwin_destroy_window(struct cwin_window *window);

bool cwin_poll_event(struct cwin_event_queue *queue, struct cwin_event *event);

void cwin_get_raw_window(struct cwin_window *window,
                         struct cwin_raw_window *raw);

#endif