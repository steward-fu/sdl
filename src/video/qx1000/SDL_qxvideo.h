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

#ifndef __SDL_qxvideo_H__
#define __SDL_qxvideo_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <syscall.h>
#include <sys/mman.h>
#include <linux/input.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "../SDL_sysvideo.h"

#define _THIS SDL_VideoDevice *this

#define MAX_FB  2
#define LCD_W   1080
#define LCD_H   2160

struct _wayland {
    struct wl_shell* shell;
    struct wl_region* region;
    struct wl_display* display;
    struct wl_surface* surface;
    struct wl_registry* registry;
    struct wl_compositor* compositor;
    struct wl_shell_surface* shell_surface;

    struct _egl {
        EGLConfig config;
        EGLContext context;
        EGLDisplay display;
        EGLSurface surface;

        GLuint vShader;
        GLuint fShader;
        GLuint pObject;

        GLuint textureId;
        GLint positionLoc;
        GLint texCoordLoc;
        GLint samplerLoc;
        struct wl_egl_window* window;
    } egl;
    
    struct _org {
        int w;
        int h;
        int bpp;
        int size;
    } info;

    int init;
    int ready;
    int flip;
    uint8_t* bg;
    uint8_t* data;
    uint16_t* pixels[MAX_FB];
};

#endif

