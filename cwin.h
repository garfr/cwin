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
  CWIN_ERROR_VK_INTERNAL,
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
  CWIN_EVENT_MOUSE,
};

enum cwin_window_event_type {
  CWIN_WINDOW_EVENT_RESIZE,
  CWIN_WINDOW_EVENT_CLOSE,
  CWIN_WINDOW_EVENT_FOCUS,
  CWIN_WINDOW_EVENT_UNFOCUS,
  CWIN_WINDOW_EVENT_ENTER,
  CWIN_WINDOW_EVENT_EXIT,
};

struct cwin_window_event {
  enum cwin_window_event_type t;
  struct cwin_window *window;
  int width, height;
};

enum cwin_mouse_event_type {
  CWIN_MOUSE_EVENT_MOVE,
  CWIN_MOUSE_EVENT_BUTTON,
  CWIN_MOUSE_EVENT_WHEEL,
};

enum cwin_button_state {
  CWIN_BUTTON_DOWN,
  CWIN_BUTTON_UP,
};

enum cwin_mouse_button {
  CWIN_MOUSE_BUTTON_LEFT,
  CWIN_MOUSE_BUTTON_MIDDLE,
  CWIN_MOUSE_BUTTON_RIGHT,
};

struct cwin_mouse_event {
  enum cwin_mouse_event_type t;
  struct cwin_window *window; /* The window with mouse focus. */
  union {
    struct {
      int x, y;
    };
    struct {
      enum cwin_button_state state;
      enum cwin_mouse_button button;
    };
    struct {
      int delta;
    };
  };
};

struct cwin_event {
  enum cwin_event_type t;
  union {
    struct cwin_window_event window;
    struct cwin_mouse_event mouse;
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

enum cwin_screen_state {
  CWIN_SCREEN_FULLSCREEN,
  CWIN_SCREEN_DESKTOP,
  CWIN_SCREEN_WINDOWED,
};

/* Initializes the library internals. */
enum cwin_error cwin_init(void);
void cwin_deinit(void);

enum cwin_error cwin_create_event_queue(struct cwin_event_queue **out);
void cwin_destroy_event_queue(struct cwin_event_queue *queue);

enum cwin_error cwin_create_window(struct cwin_window **out,
                                   struct cwin_window_builder *builder);

void cwin_window_get_size_screen_coordinates(struct cwin_window *window,
                                             int *width, int *height);
void cwin_window_get_size_pixels(struct cwin_window *window,
                                 int *width, int *height);

void cwin_mouse_capture(struct cwin_window *window);
void cwin_mouse_uncapture(struct cwin_window *window);

void cwin_destroy_window(struct cwin_window *window);

bool cwin_poll_event(struct cwin_event_queue *queue, struct cwin_event *event);

void cwin_get_raw_window(struct cwin_window *window,
                         struct cwin_raw_window *raw);

void cwin_window_set_screen_state(struct cwin_window *window,
                                  enum cwin_screen_state state);

void cwin_window_set_maximum_size(struct cwin_window *window,
                                  int max_width, int max_height);
void cwin_window_set_minimum_size(struct cwin_window *window,
                                  int min_width, int min_height);


#ifdef CWIN_VULKAN

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

void cwin_vk_get_required_extensions(struct cwin_window *window,
                                     const char **extensions,
                                     int *extension_count);

enum cwin_error cwin_vk_create_surface(struct cwin_window *window,
                                       VkInstance instance,
                                       VkSurfaceKHR *surface);

#endif

#endif
