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

#include "SDL_config.h"

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_a30_video.h"
#include "SDL_a30_event.h"

#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#if 1
    #define debug(...) printf("[SDL] "__VA_ARGS__)
#else
    #define debug(...) (void)0
#endif

struct _a30 a30;

static int A30_Available(void)
{
    debug("%s\n", __func__);
    return 1;
}

static void A30_DeleteDevice(SDL_VideoDevice *device)
{
    debug("%s\n", __func__);
    if(device){
        SDL_free(device);
    }
    free(a30.fb_mem[0]);
    free(a30.fb_mem[1]);
    a30.fb_mem[0] = NULL;
    a30.fb_mem[1] = NULL;
}

static int A30_HWAccelBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
    debug("%s\n", __func__);
    return 0;
}

static int A30_CheckHWBlit(_THIS, SDL_Surface *src, SDL_Surface *dst)
{
    debug("%s\n", __func__);
    src->map->hw_blit = A30_HWAccelBlit;
    return 1;
}

static SDL_Rect **A30_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
    debug("%s\n", __func__);
    return (SDL_Rect **)-1;
}

static int A30_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
    debug("%s\n", __func__);

    a30.fb_mem[0] = malloc(LCD_W * LCD_H * 4);
    a30.fb_mem[1] = malloc(LCD_W * LCD_H * 4);
    return 0;
}

static SDL_Surface *A30_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags)
{
    debug("%s\n", __func__);

	if(!SDL_ReallocFormat(current, bpp, 0, 0, 0, 0)) {
		SDL_SetError("Failed to allocate new pixel format for requested mode");
		return NULL;
	}

	current->flags = flags | SDL_DOUBLEBUF | SDL_PREALLOC;
	current->w = width;
	current->h = height;
    current->pitch = width * (bpp / 8);
    current->pixels = a30.fb_mem[0];
    return current;
}

static int A30_AllocHWSurface(_THIS, SDL_Surface *surface)
{
    debug("%s\n", __func__);
    return 0;
}

static void A30_FreeHWSurface(_THIS, SDL_Surface *surface)
{
    debug("%s\n", __func__);
}

static int A30_LockHWSurface(_THIS, SDL_Surface *surface)
{
    return 0;
}

static void A30_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
}

static int A30_FlipHWSurface(_THIS, SDL_Surface *surface)
{
    return 0;
}

static void A30_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
    debug("%s\n", __func__);
}

static int A30_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
    debug("%s\n", __func__);
    return 0;
}

static void A30_VideoQuit(_THIS)
{
    debug("%s\n", __func__);
}

static SDL_VideoDevice *A30_CreateDevice(int devindex)
{
    SDL_VideoDevice *device = NULL;

    debug("%s\n", __func__);
    device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return 0;
    }

    device->VideoInit = A30_VideoInit;
    device->ListModes = A30_ListModes;
    device->SetVideoMode = A30_SetVideoMode;
    device->CreateYUVOverlay = NULL;
    device->SetColors = A30_SetColors;
    device->UpdateRects = A30_UpdateRects;
    device->VideoQuit = A30_VideoQuit;
    device->AllocHWSurface = A30_AllocHWSurface;
    device->CheckHWBlit = A30_CheckHWBlit;
    device->FillHWRect = NULL;
    device->SetHWColorKey = NULL;
    device->SetHWAlpha = NULL;
    device->LockHWSurface = A30_LockHWSurface;
    device->UnlockHWSurface = A30_UnlockHWSurface;
    device->FlipHWSurface = A30_FlipHWSurface;
    device->FreeHWSurface = A30_FreeHWSurface;
    device->SetCaption = NULL;
    device->SetIcon = NULL;
    device->IconifyWindow = NULL;
    device->GrabInput = NULL;
    device->GetWMInfo = NULL;
    device->InitOSKeymap = A30_InitOSKeymap;
    device->PumpEvents = A30_PumpEvents;
    device->free = A30_DeleteDevice;
    device->CreateWMCursor = NULL;
    device->ShowWMCursor = NULL;
    device->CheckMouseMode = NULL;
    device->UpdateMouse = NULL;
    return device;
}

VideoBootStrap A30_bootstrap = {"a30", "SDL A30 video driver", A30_Available, A30_CreateDevice};

