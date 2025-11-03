// LGPL-2.1 License
// (C) 2025 Steward Fu <steward.fu@gmail.com>

#include "SDL_config.h"

#if SDL_VIDEO_DRIVER_SFOS

#ifndef __SFOS_VIDEO_H__
#define __SFOS_VIDEO_H__

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

#define DEBUG       0
#define MAX_FB      2

#if defined(QX1000) || defined(QX1050)
#define LCD_W       1080
#define LCD_H       2160
#define DEV_PATH    "/dev/input/event3"
#endif

#if defined(XT897)
#define LCD_W       540
#define LCD_H       960
#define DEV_PATH    "/dev/input/event1"
#endif

#if defined(XT894)
#define LCD_W       540
#define LCD_H       960
#define DEV_PATH    "/dev/input/event3"
#endif

typedef struct {
    struct wl_shell *shell;
    struct wl_region *region;
    struct wl_display *display;
    struct wl_surface *surface;
    struct wl_registry *registry;
    struct wl_egl_window *window;
    struct wl_compositor *compositor;
    struct wl_shell_surface *shell_surface;

    struct {
        EGLConfig config;
        EGLContext context;
        EGLDisplay display;
        EGLSurface surface;

        GLuint tex;
        GLuint program;

        GLint pos;
        GLint coord;
        GLint sampler;
    } egl;
    
    struct {
        int w;
        int h;
        int bpp;
        int size;
    } info;

    struct {
        int running;
        pthread_t id[3];
    } thread;

    int init;
    int flip;
    int ready;

    uint8_t *bg;
    uint8_t *data;
    uint16_t *pixels[MAX_FB];
} wayland;

#endif

#endif

