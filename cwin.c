#include "cwin.h"

#include <stdio.h>

#define CWIN_NEW(type) ((type *) calloc(1, sizeof(type)))
#define CWIN_ARR(type, len) ((type *) calloc((len), sizeof(type)))
#define CWIN_FREE(type, ptr) free(ptr)
#define CWIN_FREE_ARR(type, len, ptr) free(ptr)
#define CWIN_REALLOC(type, old, new, ptr) ((type *) realloc(ptr, new *          \
                                                            sizeof(type)))

#define INIT_EVENT_QUEUE_EVENTS 32

#if !defined(CWIN_BACKEND_WIN32) && !defined(CWIN_BACKEND_WL) &&                \
    !defined(CWIN_BACKEND_MACOS)
#error One CWin backend must be defined
#endif

struct cwin_event_queue {
  struct cwin_event *events;
  size_t events_alloc, events_left;
};

struct cwin_event_queue *global_queue;

#define CWIN_WINDOW_TYPE(__platform_type)                                       \
  struct cwin_window {                                                          \
    __platform_type plat;                                                       \
    struct cwin_event_queue *queue;                                             \
  }

/* PLATFORM PROTOTYPES */
enum cwin_error cwin_plat_init_window(struct cwin_window *window,
                                      struct cwin_window_builder *builder);
enum cwin_error cwin_plat_init(void);
enum cwin_error cwin_plat_pump_events(void);
void cwin_plat_get_raw_window(struct cwin_window *window,
                              struct cwin_raw_window *raw);

/* REGULAR PROTOTYPES */

struct cwin_event *alloc_event(struct cwin_event_queue *queue);

#ifdef CWIN_BACKEND_WIN32

#if defined(CWIN_BACKEND_X11) || defined(CWIN_BACKEND_WL) ||                    \
    defined(CWIN_BACKEND_MACOS)
#error Only one backend supported at a time.
#endif

#include <windows.h>

/* TYPES */

struct cwin_win32_window {
  HWND handle;
};

CWIN_WINDOW_TYPE(struct cwin_win32_window);

struct {
  HINSTANCE instance;
  ATOM window_class;
} win32;

/* CONSTANTS */

const wchar_t CWIN_CLASS_NAME[] = L"CWin Window";

/* PROTOTYPES */

LRESULT CALLBACK cwin_win32_window_proc(HWND hwnd, UINT umsg, WPARAM wparam,
                                        LPARAM lparam);
enum cwin_error utf8_to_wide_string(WCHAR **str_out, int *len_out,
                                    const uint8_t *src, size_t src_len);

/* PLATFORM FUNCTIONS */

enum cwin_error cwin_plat_pump_events(void)
{
  MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return CWIN_SUCCESS;
}

enum cwin_error cwin_plat_init(void)
{
  win32.instance = GetModuleHandle(NULL);

  WNDCLASS wc = {
    .lpfnWndProc = cwin_win32_window_proc,
    .hInstance = win32.instance,
    .lpszClassName = CWIN_CLASS_NAME,
  };

  win32.window_class = RegisterClass(&wc);
  if (win32.window_class == 0)
  {
    return CWIN_ERROR_WIN32_INTERNAL;
  }

  return CWIN_SUCCESS;
}

enum cwin_error cwin_plat_init_window(struct cwin_window *window,
                                      struct cwin_window_builder *builder)
{
  enum cwin_error err;
  if (builder->x == CWIN_WINDOW_POS_UNDEFINED)
  {
    builder->x = CW_USEDEFAULT;
  }
  if (builder->y == CWIN_WINDOW_POS_UNDEFINED)
  {
    builder->y = CW_USEDEFAULT;
  }
  if (builder->width == CWIN_WINDOW_SIZE_UNDEFINED)
  {
    builder->width = CW_USEDEFAULT;
  }
  if (builder->height == CWIN_WINDOW_SIZE_UNDEFINED)
  {
    builder->height = CW_USEDEFAULT;
  }

  int size;
  WCHAR *str;
  err = utf8_to_wide_string(&str, &size, builder->name, builder->name_len);
  if (err)
  {
    return err;
  }

  window->plat.handle = CreateWindowEx(0, CWIN_CLASS_NAME,
                                       str,
                                       WS_OVERLAPPEDWINDOW,
                                       builder->x, builder->y,
                                       builder->width, builder->height,
                                       NULL,
                                       NULL,
                                       win32.instance,
                                       NULL);

  CWIN_FREE_ARR(WCHAR, size, str);

  if (window->plat.handle == NULL)
  {
    CWIN_FREE(struct cwin_window, window);
    return CWIN_ERROR_WIN32_INTERNAL;
  }

  ShowWindow(window->plat.handle, SW_NORMAL);
  SetWindowLongPtrA(window->plat.handle, GWLP_USERDATA, (LONG_PTR) window);

  return CWIN_SUCCESS;
}

void cwin_plat_get_raw_window(struct cwin_window *window,
                              struct cwin_raw_window *raw)
{
  raw->t = CWIN_RAW_WINDOW_WIN32;
  raw->win32.hwnd = window->plat.handle;
  raw->win32.hinstance = win32.instance;
}

LRESULT CALLBACK cwin_win32_window_proc(HWND hwnd, UINT umsg, WPARAM wparam,
                                        LPARAM lparam)
{
  struct cwin_event *event;
  struct cwin_window *window =
    (struct cwin_window *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

  /*
   * Creating a window calls the window procedure, so we haven't set the
   * user data yet.
   */
  if (window == NULL)
  {
    return DefWindowProc(hwnd, umsg, wparam, lparam);
  }
  struct cwin_event_queue *queue = window->queue;

  switch (umsg)
  {
  case WM_CLOSE:
    event = alloc_event(queue);
    event->t = CWIN_EVENT_WINDOW;
    event->window.t = CWIN_WINDOW_EVENT_CLOSE;
    break;
  default:
    return DefWindowProc(hwnd, umsg, wparam, lparam);
  }

  return 0;
}

enum cwin_error utf8_to_wide_string(WCHAR **str_out, int *len_out,
                                    const uint8_t *src, size_t src_len)
{
  int real_src_len = src_len == 0 ? -1 : src_len;
  int len = MultiByteToWideChar(CP_UTF8, 0, src, real_src_len,
                                 NULL, 0);
  if (len == 0)
  {
    return CWIN_ERROR_INVALID_UTF8;
  }

  WCHAR *str = CWIN_ARR(WCHAR, len);
  if (str == NULL)
  {
    return CWIN_ERROR_OOM;
  }

  if (MultiByteToWideChar(CP_UTF8, 0, src, real_src_len, str, len) == 0)
  {
    CWIN_FREE_ARR(WCHAR, len, str);
    return CWIN_ERROR_INVALID_UTF8;
  }

  *len_out = len;
  *str_out = str;

  return CWIN_SUCCESS;
}

#endif

/* PRIVATE FUNCTIONS */

struct cwin_event *alloc_event(struct cwin_event_queue *queue)
{
  if (queue->events_left + 1 > queue->events_alloc)
  {
    queue->events = CWIN_REALLOC(struct cwin_event, queue->events_alloc,
                                 queue->events_alloc * 2, queue->events);
    queue->events_alloc *= 2;
  }

  return &queue->events[queue->events_left++];
}

/* PUBLIC FUNCTIONS */

enum cwin_error cwin_create_event_queue(struct cwin_event_queue **out)
{
  struct cwin_event_queue *queue = CWIN_NEW(struct cwin_event_queue);
  if (queue == NULL)
  {
    return CWIN_ERROR_OOM;
  }

  queue->events_alloc = INIT_EVENT_QUEUE_EVENTS;
  queue->events_left = 0;

  queue->events = CWIN_ARR(struct cwin_event, queue->events_alloc);
  if (queue->events == NULL)
  {
    CWIN_FREE(struct cwin_event_queue, queue);
    return CWIN_ERROR_OOM;
  }

  *out = queue;
  return CWIN_SUCCESS;
}

void cwin_destroy_event_queue(struct cwin_event_queue *queue)
{
  CWIN_FREE_ARR(struct cwin_event, queue->events_alloc, queue->events);
  CWIN_FREE(struct cwin_event_queue, queue);
}

enum cwin_error cwin_create_window(struct cwin_window **out,
                                   struct cwin_window_builder *builder)
{
  enum cwin_error err;
  struct cwin_window *window = CWIN_NEW(struct cwin_window);
  if (window == NULL)
  {
    return CWIN_ERROR_OOM;
  }

  window->queue = builder->queue;
  if (window->queue == NULL)
  {
    window->queue = global_queue;
  }

  err = cwin_plat_init_window(window, builder);
  if (err)
  {
    return err;
  }

  *out = window;
  return CWIN_SUCCESS;
}

void cwin_destroy_window(struct cwin_window *window)
{
  CWIN_FREE(struct cwin_window, window);
}

bool cwin_poll_event(struct cwin_event_queue *queue, struct cwin_event *event)
{
  if (queue == NULL)
  {
    queue = global_queue;
  }

  if (queue->events_left == 0)
  {
    cwin_plat_pump_events();
  }

  if (queue->events_left == 0)
  {
    return false;
  }

  *event = queue->events[--queue->events_left];
  return true;
}

enum cwin_error cwin_init()
{
  enum cwin_error err;

  err = cwin_plat_init();
  if (err)
  {
    return err;
  }

  err = cwin_create_event_queue(&global_queue);
  if (err)
  {
    return err;
  }

  return CWIN_SUCCESS;
}

void cwin_get_raw_window(struct cwin_window *window,
                         struct cwin_raw_window *raw)
{
  cwin_plat_get_raw_window(window, raw);
}
