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
  };

  err = cwin_create_window(&window, &window_builder);
  if (err)
  {
    printf("error: %d\n", err);
    return EXIT_FAILURE;
  }

  cwin_get_raw_window(window, &raw);
  assert(raw.t == CWIN_RAW_WINDOW_WIN32);
  while (running)
  {
    while (cwin_poll_event(queue, &event))
    {
      if (event.t == CWIN_EVENT_WINDOW &&
          event.window.t == CWIN_WINDOW_EVENT_CLOSE)
      {
        running = false;
      }
    }
  }

  cwin_destroy_window(window);

  printf("Exiting with success\n");
  return EXIT_SUCCESS;
}
