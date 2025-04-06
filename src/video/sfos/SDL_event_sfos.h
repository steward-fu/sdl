// LGPL-2.1 License
// (C) 2025 Steward Fu <steward.fu@gmail.com>

#include "SDL_config.h"

#if SDL_VIDEO_DRIVER_SFOS

#ifndef __SDL_EVENT_SFOS_H__
#define __SDL_EVENT_SFOS_H__

#include "SDL_video_sfos.h"

void SFOS_PumpEvents(_THIS);
void SFOS_InitOSKeymap(_THIS);

#endif

#endif

