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

#ifndef __SDL_A30_VIDEO_H__
#define __SDL_A30_VIDEO_H__

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

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "../SDL_sysvideo.h"

#define _THIS SDL_VideoDevice *this

#define PREFIX              "[SDL] "
#define LCD_W               640
#define LCD_H               480
#define CCU_BASE            0x01c20000
#define INIT_CPU_CORE       2
#define INIT_CPU_CLOCK      1500
#define DEINIT_CPU_CORE     2
#define DEINIT_CPU_CLOCK    648

enum _TEX_TYPE {
    TEX_SCR = 0,
    TEX_MAX
};

struct _video {
    SDL_Surface *vsurf;

    EGLConfig eglConfig;
    EGLDisplay eglDisplay;
    EGLContext eglContext;
    EGLSurface eglSurface;
    GLuint vShader;
    GLuint fShader;
    GLuint pObject;
    GLuint texID[TEX_MAX];
    GLint posLoc;
    GLint texLoc;
    GLint samLoc;
    GLint alphaLoc;

    int mem_fd;
    int fb_flip;
    void *fb_mem[2];
    uint8_t* ccu_mem;
    uint8_t* dac_mem;
    uint32_t *vol_ptr;
    uint32_t *cpu_ptr;
};

struct _cpu_clock {
    int clk;
    uint32_t reg;
};

#endif

