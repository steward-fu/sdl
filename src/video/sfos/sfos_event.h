// LGPL-2.1 License
// (C) 2025 Steward Fu <steward.fu@gmail.com>

#include "SDL_config.h"

#if SDL_VIDEO_DRIVER_SFOS

#ifndef __SFOS_EVENT_H__
#define __SFOS_EVENT_H__

#include "sfos_video.h"

void sfos_pump_events(_THIS);
void sfos_init_os_keymap(_THIS);

#endif

#endif

