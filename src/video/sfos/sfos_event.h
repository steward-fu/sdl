// LGPL-2.1 License
// (C) 2025 Steward Fu <steward.fu@gmail.com>

#include "SDL_config.h"

#if SDL_VIDEO_DRIVER_SFOS

#ifndef __SFOS_EVENT_H__
#define __SFOS_EVENT_H__

#include "sfos_video.h"

void SFOS_PumpEvents(_THIS);
void SFOS_InitOSKeymap(_THIS);

#endif

#endif

