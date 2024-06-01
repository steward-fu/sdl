/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

#include "SDL_config.h"
#include "SDL.h"
#include "SDL_a30_video.h"
#include "SDL_a30_event.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"

#if 1
    #define debug(...) printf("[SDL] "__VA_ARGS__)
#else
    #define debug(...) (void)0
#endif

void A30_PumpEvents(_THIS)
{
}

void A30_InitOSKeymap(_THIS)
{
}

