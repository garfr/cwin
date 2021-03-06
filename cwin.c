#include "cwin.h"

#include <stdio.h>
#include <stdlib.h>

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
void cwin_plat_deinit_window(struct cwin_window *window);
enum cwin_error cwin_plat_init(void);
void cwin_plat_deinit(void);
enum cwin_error cwin_plat_pump_events(void);
void cwin_plat_get_raw_window(struct cwin_window *window,
                              struct cwin_raw_window *raw);

/* REGULAR PROTOTYPES */

struct cwin_event *alloc_event(struct cwin_event_queue *queue,
                               enum cwin_event_type type);
struct cwin_event *alloc_window_event(struct cwin_event_queue *queue,
                                      enum cwin_window_event_type type,
                                      struct cwin_window *window);
struct cwin_event *alloc_mouse_event(struct cwin_event_queue *queue,
                                      enum cwin_mouse_event_type type,
                                      struct cwin_window *window);

#ifdef CWIN_BACKEND_WIN32

#if defined(CWIN_BACKEND_X11) || defined(CWIN_BACKEND_WL) ||                    \
    defined(CWIN_BACKEND_MACOS)
#error Only one backend supported at a time.
#endif

#include <windows.h>

/* TYPES */

struct cwin_win32_window {
  HWND handle;
  bool is_tracked;
  enum cwin_screen_state screen_state;
  WINDOWPLACEMENT prev_placement; /* Window placement before fullscreen. */
  bool has_minimum, has_maximum;
  int min_width, min_height;
  int max_width, max_height;
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

void cwin_window_get_size_pixels(struct cwin_window *window,
                                 int *width, int *height)
{
  RECT rect;
  GetClientRect(window->plat.handle, &rect);

  if (width != NULL)
  {
    *width = rect.right;
  }
  if (height != NULL)
  {
    *height = rect.bottom;
  }
}

void cwin_window_get_size_screen_coordinates(struct cwin_window *window,
                                             int *width, int *height)
{
  RECT rect;
  GetClientRect(window->plat.handle, &rect);

  if (width != NULL)
  {
    *width = rect.right;
  }
  if (height != NULL)
  {
    *height = rect.bottom;
  }
}

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

void cwin_plat_deinit(void)
{
  UnregisterClass(CWIN_CLASS_NAME, win32.instance);
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

  DWORD style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
  style |= WS_SYSMENU | WS_MINIMIZEBOX;
  style |= WS_CAPTION;
  style |= WS_MAXIMIZEBOX | WS_THICKFRAME;

  DWORD exstyle = WS_EX_APPWINDOW;
  window->plat.is_tracked = false;
  window->plat.screen_state = CWIN_SCREEN_WINDOWED;
  window->plat.has_minimum = window->plat.has_maximum = false;
  window->plat.handle = CreateWindowEx(exstyle,
                                       CWIN_CLASS_NAME,
                                       str,
                                       style,
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

void cwin_plat_deinit_window(struct cwin_window *window)
{
  DestroyWindow(window->plat.handle);
}

void cwin_plat_get_raw_window(struct cwin_window *window,
                              struct cwin_raw_window *raw)
{
  raw->t = CWIN_RAW_WINDOW_WIN32;
  raw->win32.hwnd = window->plat.handle;
  raw->win32.hinstance = win32.instance;
}

void cwin_window_set_screen_state(struct cwin_window *window,
                                  enum cwin_screen_state state)
{
  if (state == window->plat.screen_state)
  {
    return;
  }

  DWORD style = GetWindowLong(window->plat.handle, GWL_STYLE);
  switch (state)
  {
  case CWIN_SCREEN_FULLSCREEN: {
    MONITORINFO mi = {
      sizeof(MONITORINFO)
    };
    if (GetWindowPlacement(window->plat.handle,
                           &window->plat.prev_placement) &&
        GetMonitorInfo(MonitorFromWindow(window->plat.handle,
                                         MONITOR_DEFAULTTOPRIMARY), &mi))
    {
      SetWindowLong(window->plat.handle, GWL_STYLE,
                    style & ~WS_OVERLAPPEDWINDOW);
      SetWindowPos(window->plat.handle, HWND_TOP, mi.rcMonitor.left,
                   mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                   mi.rcMonitor.bottom - mi.rcMonitor.top,
                   SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    break;
  }
  case CWIN_SCREEN_WINDOWED: {
    SetWindowLong(window->plat.handle, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(window->plat.handle, &window->plat.prev_placement);
    SetWindowPos(window->plat.handle, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                 SWP_FRAMECHANGED);
    break;
  }
  }

  window->plat.screen_state = state;
}

void cwin_window_set_maximum_size(struct cwin_window *window,
                                  int max_width, int max_height)
{
  window->plat.has_maximum = true;
  window->plat.max_width = max_width;
  window->plat.max_height = max_height;
}

void cwin_window_set_minimum_size(struct cwin_window *window,
                                  int min_width, int min_height)
{
  window->plat.has_minimum = true;
  window->plat.min_width = min_width;
  window->plat.min_height = min_height;
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
    event = alloc_window_event(queue, CWIN_WINDOW_EVENT_CLOSE, window);
    break;
  case WM_GETMINMAXINFO: {
    LPMINMAXINFO info = (LPMINMAXINFO) lparam;
    if (window->plat.has_minimum)
    {
      info->ptMinTrackSize.x = window->plat.min_width;
      info->ptMinTrackSize.y = window->plat.min_height;
    }
    if (window->plat.has_maximum)
    {
      info->ptMaxTrackSize.x = window->plat.max_width;
      info->ptMaxTrackSize.y = window->plat.max_height;
    }
    break;
  }
  case WM_SIZE:
    event = alloc_window_event(queue, CWIN_WINDOW_EVENT_RESIZE, window);
    event->window.width = LOWORD(lparam);
    event->window.height = HIWORD(lparam);
    break;
  case WM_MOUSEHOVER:
    event = alloc_window_event(queue, CWIN_WINDOW_EVENT_ENTER, window);
    break;
  case WM_MOUSELEAVE:
    alloc_window_event(queue, CWIN_WINDOW_EVENT_EXIT, window);
    window->plat.is_tracked = false;
    break;
  case WM_LBUTTONDOWN:
    event = alloc_mouse_event(queue, CWIN_MOUSE_EVENT_BUTTON, window);
    event->mouse.state = CWIN_BUTTON_DOWN;
    event->mouse.button = CWIN_MOUSE_BUTTON_LEFT;
    break;
  case WM_MBUTTONDOWN:
    event = alloc_mouse_event(queue, CWIN_MOUSE_EVENT_BUTTON, window);
    event->mouse.state = CWIN_BUTTON_DOWN;
    event->mouse.button = CWIN_MOUSE_BUTTON_MIDDLE;
    break;
  case WM_RBUTTONDOWN:
    event = alloc_mouse_event(queue, CWIN_MOUSE_EVENT_BUTTON, window);
    event->mouse.state = CWIN_BUTTON_DOWN;
    event->mouse.button = CWIN_MOUSE_BUTTON_RIGHT;
    break;
  case WM_LBUTTONUP:
    event = alloc_mouse_event(queue, CWIN_MOUSE_EVENT_BUTTON, window);
    event->mouse.state = CWIN_BUTTON_UP;
    event->mouse.button = CWIN_MOUSE_BUTTON_LEFT;
    break;
  case WM_MBUTTONUP:
    event = alloc_mouse_event(queue, CWIN_MOUSE_EVENT_BUTTON, window);
    event->mouse.state = CWIN_BUTTON_UP;
    event->mouse.button = CWIN_MOUSE_BUTTON_MIDDLE;
    break;
  case WM_RBUTTONUP:
    event = alloc_mouse_event(queue, CWIN_MOUSE_EVENT_BUTTON, window);
    event->mouse.state = CWIN_BUTTON_UP;
    event->mouse.button = CWIN_MOUSE_BUTTON_RIGHT;
    break;
  case WM_MOUSEWHEEL:
    event = alloc_mouse_event(queue, CWIN_MOUSE_EVENT_WHEEL, window);
    event->mouse.delta = GET_WHEEL_DELTA_WPARAM(wparam);
    break;
  case WM_KILLFOCUS:
    alloc_window_event(queue, CWIN_WINDOW_EVENT_UNFOCUS, window);
    break;
  case WM_SETFOCUS:
    alloc_window_event(queue, CWIN_WINDOW_EVENT_FOCUS, window);
    break;
  case WM_MOUSEMOVE:
    event = alloc_mouse_event(queue, CWIN_MOUSE_EVENT_MOVE, window);
    event->mouse.x = MAKEPOINTS(lparam).x;
    event->mouse.y = MAKEPOINTS(lparam).y;

    if (!window->plat.is_tracked)
    {
      TRACKMOUSEEVENT track_mouse_event;
      track_mouse_event.cbSize = sizeof(TRACKMOUSEEVENT);
      track_mouse_event.dwFlags = TME_LEAVE;
      track_mouse_event.hwndTrack = hwnd;

      if (TrackMouseEvent(&track_mouse_event))
      {
        window->plat.is_tracked = true;
      }

      alloc_window_event(queue, CWIN_WINDOW_EVENT_ENTER, window);
    }

    break;
  default:
    return DefWindowProc(hwnd, umsg, wparam, lparam);
  }

  return 0;
}

void cwin_mouse_capture(struct cwin_window *window)
{
  SetCapture(window->plat.handle);
}

void cwin_mouse_uncapture(struct cwin_window *window)
{
  ReleaseCapture();
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

#ifdef CWIN_VULKAN

const char *win32_vk_extensions[] = {
  "VK_KHR_surface",
  "VK_KHR_win32_surface",
};

void cwin_vk_get_required_extensions(struct cwin_window *window,
                                     const char **extensions,
                                     int *extension_count)
{
  (void) window;

  if (*extension_count == 0 || extensions == NULL)
  {
    *extension_count = sizeof(win32_vk_extensions) /
      sizeof(win32_vk_extensions[0]);
  } else
  {
    for (int i = 0; i < *extension_count; i++)
    {
      extensions[i] = win32_vk_extensions[i];
    }
  }
}

enum cwin_error cwin_vk_create_surface(struct cwin_window *window,
                                       VkInstance instance,
                                       VkSurfaceKHR *surface)
{
  VkResult err;
  VkWin32SurfaceCreateInfoKHR surface_create_info = {
    .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
    .hwnd = window->plat.handle,
    .hinstance = win32.instance,
  };

  err = vkCreateWin32SurfaceKHR(instance, &surface_create_info, NULL, surface);
  if (err)
  {
    return CWIN_ERROR_VK_INTERNAL;
  }

  return CWIN_SUCCESS;
}

#endif

#endif

/* PRIVATE FUNCTIONS */

struct cwin_event *alloc_event(struct cwin_event_queue *queue,
                               enum cwin_event_type type)
{
  if (queue->events_left + 1 > queue->events_alloc)
  {
    queue->events = CWIN_REALLOC(struct cwin_event, queue->events_alloc,
                                 queue->events_alloc * 2, queue->events);
    if (queue->events == NULL)
    {
      return NULL;
    }
    queue->events_alloc *= 2;
  }

  struct cwin_event *event = &queue->events[queue->events_left++];
  event->t = type;
  return event;
}

struct cwin_event *alloc_window_event(struct cwin_event_queue *queue,
                                      enum cwin_window_event_type type,
                                      struct cwin_window *window)
{
  struct cwin_event *event = alloc_event(queue, CWIN_EVENT_WINDOW);
  if (event == NULL)
  {
    return NULL;
  }

  event->window.window = window;
  event->window.t = type;

  return event;
}
struct cwin_event *alloc_mouse_event(struct cwin_event_queue *queue,
                                      enum cwin_mouse_event_type type,
                                      struct cwin_window *window)
{
  struct cwin_event *event = alloc_event(queue, CWIN_EVENT_MOUSE);
  if (event == NULL)
  {
    return NULL;
  }

  event->mouse.window = window;
  event->mouse.t = type;

  return event;
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
  cwin_plat_deinit_window(window);
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

void cwin_deinit()
{
  cwin_plat_deinit();
  cwin_destroy_event_queue(global_queue);
}

void cwin_get_raw_window(struct cwin_window *window,
                         struct cwin_raw_window *raw)
{
  cwin_plat_get_raw_window(window, raw);
}
