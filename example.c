#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "cwin.h"

int main()
{
  enum cwin_error err;
  struct cwin_window *window;
  struct cwin_event_queue *queue;
  struct cwin_event event;
  struct cwin_raw_window raw;

  bool running = true;
  err = cwin_init();
  if (err)
  {
    printf("error: %d\n", err);
    return EXIT_FAILURE;
  }

  err = cwin_create_event_queue(&queue);
  if (err)
  {
    printf("error: %d\n", err);
    return EXIT_FAILURE;
  }

  struct cwin_window_builder window_builder = {
    .name = "cwin example, 素晴らしい",
    .queue = queue,
    .width = 200,
    .height = 200,
  };

  err = cwin_create_window(&window, &window_builder);
  if (err)
  {
    printf("error: %d\n", err);
    return EXIT_FAILURE;
  }

  cwin_get_raw_window(window, &raw);
  assert(raw.t == CWIN_RAW_WINDOW_WIN32);

  int pwidth, pheight, scwidth, scheight;
  cwin_window_get_size_pixels(window, &pwidth, &pheight);
  printf("Pixel size: (%d, %d)\n", pwidth, pheight);

  cwin_window_get_size_screen_coordinates(window, &scwidth, &scheight);
  printf("Screen coordinate size: (%d, %d)\n", scwidth, scheight);

  cwin_window_set_minimum_size(window, 100, 100);
  cwin_window_set_maximum_size(window, 300, 300);

  while (running)
  {
    while (cwin_poll_event(queue, &event))
    {
      switch (event.t)
      {
      case CWIN_EVENT_WINDOW:
        switch (event.window.t)
        {
        case CWIN_WINDOW_EVENT_CLOSE:
          running = false;
          break;
        case CWIN_WINDOW_EVENT_RESIZE:
          printf("Window resize: (%d, %d)\n", event.window.width,
                 event.window.height);
          break;
        case CWIN_WINDOW_EVENT_ENTER:
          printf("Mouse enter\n");
          break;
        case CWIN_WINDOW_EVENT_EXIT:
          printf("Mouse exit\n");
          break;
        case CWIN_WINDOW_EVENT_FOCUS:
          printf("Mouse focus\n");
          break;
        case CWIN_WINDOW_EVENT_UNFOCUS:
          printf("Mouse unfocus\n");
          break;
        }
        break;
      case CWIN_EVENT_MOUSE:
        switch (event.mouse.t)
        {
        case CWIN_MOUSE_EVENT_MOVE:
          printf("Mouse move: (%d, %d)\n", event.mouse.x, event.mouse.y);
          break;
        case CWIN_MOUSE_EVENT_WHEEL:
          printf("Mouse wheel: %d\n", event.mouse.delta);
          break;
        case CWIN_MOUSE_EVENT_BUTTON:
          if (event.mouse.state == CWIN_BUTTON_DOWN &&
              event.mouse.button == CWIN_MOUSE_BUTTON_LEFT)
          {
            cwin_window_set_screen_state(window, CWIN_SCREEN_FULLSCREEN);
          }
          if (event.mouse.state == CWIN_BUTTON_DOWN &&
              event.mouse.button == CWIN_MOUSE_BUTTON_RIGHT)
          {
            cwin_window_set_screen_state(window, CWIN_SCREEN_WINDOWED);
          }
          printf("Mouse %s: %d\n",
                 event.mouse.state == CWIN_BUTTON_DOWN ? "down" : "up",
                 event.mouse.button);
          break;
        }

        break;
    }
    }
  }

  cwin_destroy_window(window);

  cwin_destroy_event_queue(queue);

  cwin_deinit();
  printf("Exiting with success\n");
  return EXIT_SUCCESS;
}
