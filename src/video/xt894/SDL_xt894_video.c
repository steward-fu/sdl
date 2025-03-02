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
#include "SDL_xt894_video.h"
#include "SDL_xt894_event.h"

#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#if 0
    #define debug(...) printf("[SDL] "__VA_ARGS__)
#else
    #define debug(...) (void)0
#endif

extern uint8_t mykey[KEY_MAX][2];

static struct _wayland wl = {0};
static SDL_Surface *vsurf = NULL;

int thread_run = 0;
static pthread_t thread_id[3] = {0};

EGLint egl_attribs[] = {
    EGL_SURFACE_TYPE,
    EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE,
    EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE,
    5,
    EGL_GREEN_SIZE,
    6,
    EGL_BLUE_SIZE,
    5,
    EGL_NONE
};

EGLint ctx_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 
    2, 
    EGL_NONE
};

GLfloat egl_bg_vertices[] = {
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 
     1.0f,  1.0f, 0.0f, 1.0f, 0.0f
};

GLfloat egl_fb_vertices[] = {
    -0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 
    -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
     0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 
     0.5f,  0.5f, 0.0f, 1.0f, 0.0f
};

GLushort egl_indices[] = {0, 1, 2, 0, 2, 3};

const char *vShaderSrc =
    "attribute vec4 a_position;    \n"
    "attribute vec2 a_texCoord;    \n"
    "varying vec2 v_texCoord;      \n"
    "void main()                   \n"
    "{                             \n"
    "    gl_Position = a_position; \n"
    "    v_texCoord = a_texCoord;  \n"
    "}                             \n";

const char *fShaderSrc =
    "#ifdef GL_ES                                              \n"
    "precision mediump float;                                  \n"
    "#endif                                                    \n"
    "varying vec2 v_texCoord;                                  \n"
    "uniform float angle;                                      \n"
    "uniform float aspect;                                     \n"
    "uniform sampler2D s_texture;                              \n"
    "const vec2 HALF = vec2(0.5);                              \n"
    "void main()                                               \n"
    "{                                                         \n"
    "    float aSin = sin(angle);                              \n"
    "    float aCos = cos(angle);                              \n"
    "    vec2 tc = v_texCoord;                                 \n"
    "    mat2 rotMat = mat2(aCos, -aSin, aSin, aCos);          \n"
    "    mat2 scaleMat = mat2(aspect, 0.0, 0.0, 1.0);          \n"
    "    mat2 scaleMatInv = mat2(1.0 / aspect, 0.0, 0.0, 1.0); \n"
    "    tc -= HALF.xy;                                        \n"
    "    tc = scaleMatInv * rotMat * scaleMat * tc;            \n"
    "    tc += HALF.xy;                                        \n"
    "    vec3 tex = texture2D(s_texture, tc).rgb;              \n"
    "    gl_FragColor = vec4(tex, 1.0);                        \n"
    "}                                                         \n";

static void cb_remove(void *dat, struct wl_registry *reg, uint32_t id);
static void cb_handle(void *dat, struct wl_registry *reg, uint32_t id, const char *intf, uint32_t ver);

static struct wl_registry_listener cb_global = {
    .global = cb_handle,
    .global_remove = cb_remove
};

static void* wl_thread(void* pParam)
{
    debug("%s++\n", __func__);
    while (thread_run) {
        if (wl.init && wl.ready) {
            wl_display_dispatch(wl.display);
        }
        usleep(100);
    }
    debug("%s--\n", __func__);
    return NULL;
}

static void* keypad_thread(void* pParam)
{
    int fd = -1;
    struct input_event ev = {0};
    const char *path = "/dev/input/event3";

    debug("%s++\n", __func__);
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        debug("%s, failed to open %s\n", __func__, path);
        return NULL;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    while (thread_run) {
        if (read(fd, &ev, sizeof(struct input_event)) > 0) {
            if (ev.type == EV_KEY) {
                mykey[ev.code][ev.value] = 1;
                debug("%s, code:%d, value:%d\n", __func__, ev.code, ev.value);
            }
        }
        usleep(100);
    }
    close(fd);
    debug("%s--\n", __func__);
    return NULL;
}

static void cb_ping(void* dat, struct wl_shell_surface* shell_surf, uint32_t serial)
{
    wl_shell_surface_pong(shell_surf, serial);
}

static void cb_configure(void* dat, struct wl_shell_surface* shell_surf, uint32_t edges, int32_t w, int32_t h)
{
}

static void cb_popup_done(void* dat, struct wl_shell_surface* shell_surf)
{
}

static const struct wl_shell_surface_listener cb_shell_surf = {
    cb_ping,
    cb_configure,
    cb_popup_done
};

static void cb_handle(void* dat, struct wl_registry* reg, uint32_t id, const char* intf, uint32_t ver)
{
    if (strcmp(intf, "wl_compositor") == 0) {
        wl.compositor = wl_registry_bind(reg, id, &wl_compositor_interface, 1);
    }
    else if (strcmp(intf, "wl_shell") == 0) {
        wl.shell = wl_registry_bind(reg, id, &wl_shell_interface, 1);
    }
}

static void cb_remove(void* dat, struct wl_registry* reg, uint32_t id)
{
}

void egl_free(void)
{
    wl.init = 0;
    wl.ready = 0;
    eglDestroySurface(wl.egl.display, wl.egl.surface);
    eglDestroyContext(wl.egl.display, wl.egl.context);
    wl_egl_window_destroy(wl.egl.window);
    eglTerminate(wl.egl.display);
    //glDeleteShader(wl.egl.vShader);
    //glDeleteShader(wl.egl.fShader);
    //glDeleteProgram(wl.egl.pObject);
}

void wl_free(void)
{
    wl.init = 0;
    wl.ready = 0;
    wl_shell_surface_destroy(wl.shell_surface);
    wl_shell_destroy(wl.shell);
    wl_surface_destroy(wl.surface);
    wl_compositor_destroy(wl.compositor);
    wl_registry_destroy(wl.registry);
    wl_display_disconnect(wl.display);
    free(wl.data);
}

void wl_create(void)
{
    wl.display = wl_display_connect(NULL);
    wl.registry = wl_display_get_registry(wl.display);

    wl_registry_add_listener(wl.registry, &cb_global, NULL);
    wl_display_dispatch(wl.display);
    wl_display_roundtrip(wl.display);

    wl.surface = wl_compositor_create_surface(wl.compositor);
    wl.shell_surface = wl_shell_get_shell_surface(wl.shell, wl.surface);
    wl_shell_surface_set_toplevel(wl.shell_surface);
    wl_shell_surface_add_listener(wl.shell_surface, &cb_shell_surf, NULL);
    
    wl.region = wl_compositor_create_region(wl.compositor);
    wl_region_add(wl.region, 0, 0, LCD_W, LCD_H);
    wl_surface_set_opaque_region(wl.surface, wl.region);
    wl.egl.window = wl_egl_window_create(wl.surface, LCD_W, LCD_H);
    debug("%s, wl.display       %p\n", __func__, wl.display);
    debug("%s, wl.registry      %p\n", __func__, wl.registry);
    debug("%s, wl.surface       %p\n", __func__, wl.surface);
    debug("%s, wl.shell         %p\n", __func__, wl.shell);
    debug("%s, wl.shell_surface %p\n", __func__, wl.shell_surface);
    debug("%s, wl.region        %p\n", __func__, wl.region);
}

void egl_create(void)
{
    EGLConfig cfg = 0;
    EGLint major = 0, minor = 0, cnt = 0;

    wl.egl.display = eglGetDisplay((EGLNativeDisplayType)wl.display);
    eglInitialize(wl.egl.display, &major, &minor);
    eglGetConfigs(wl.egl.display, NULL, 0, &cnt);
    eglChooseConfig(wl.egl.display, egl_attribs, &cfg, 1, &cnt);
    wl.egl.surface = eglCreateWindowSurface(wl.egl.display, cfg, wl.egl.window, NULL);
    wl.egl.context = eglCreateContext(wl.egl.display, cfg, EGL_NO_CONTEXT, ctx_attribs);
    eglMakeCurrent(wl.egl.display, wl.egl.surface, wl.egl.surface, wl.egl.context);
    debug("%s, egl_display %p\n", __func__, wl.egl.display);
    debug("%s, egl_window  %p\n", __func__, wl.egl.window);
    debug("%s, egl_surface %p\n", __func__, wl.egl.surface);
    debug("%s, egl_context %p\n", __func__, wl.egl.context);

    wl.egl.vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(wl.egl.vShader, 1, &vShaderSrc, NULL);
    glCompileShader(wl.egl.vShader);

    GLint compiled = 0;
    glGetShaderiv(wl.egl.vShader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint len = 0;
        glGetShaderiv(wl.egl.vShader, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            char* info = malloc(sizeof(char) * len);
            glGetShaderInfoLog(wl.egl.vShader, len, NULL, info);
            debug("%s, failed to compile vShader: %s\n", __func__, info);
            free(info);
        }
    }

    wl.egl.fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(wl.egl.fShader, 1, &fShaderSrc, NULL);
    glCompileShader(wl.egl.fShader);
    
    glGetShaderiv(wl.egl.fShader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint len = 0;
        glGetShaderiv(wl.egl.fShader, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            char* info = malloc(sizeof(char) * len);
            glGetShaderInfoLog(wl.egl.fShader, len, NULL, info);
            debug("%s, failed to compile fShader: %s\n", __func__, info);
            free(info);
        }
    }

    wl.egl.pObject = glCreateProgram();
    glAttachShader(wl.egl.pObject, wl.egl.vShader);
    glAttachShader(wl.egl.pObject, wl.egl.fShader);
    glLinkProgram(wl.egl.pObject);
    glUseProgram(wl.egl.pObject);

    wl.egl.positionLoc = glGetAttribLocation(wl.egl.pObject, "a_position");
    wl.egl.texCoordLoc = glGetAttribLocation(wl.egl.pObject, "a_texCoord");
    wl.egl.samplerLoc = glGetUniformLocation(wl.egl.pObject, "s_texture");
    glUniform1f(glGetUniformLocation(wl.egl.pObject, "angle"), 90 * (3.1415 * 2.0) / 360.0);
    glUniform1f(glGetUniformLocation(wl.egl.pObject, "aspect"), 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGenTextures(1, &wl.egl.textureId);
    glBindTexture(GL_TEXTURE_2D, wl.egl.textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glViewport(0, 0, LCD_W, LCD_H);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glVertexAttribPointer(wl.egl.positionLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), egl_bg_vertices);
    glVertexAttribPointer(wl.egl.texCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &egl_bg_vertices[3]);
    glEnableVertexAttribArray(wl.egl.positionLoc);
    glEnableVertexAttribArray(wl.egl.texCoordLoc);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(wl.egl.samplerLoc, 0);

    debug("%s, textureId   0x%x\n", __func__, wl.egl.textureId);
    debug("%s, samplerLoc  0x%x\n", __func__, wl.egl.samplerLoc);
    debug("%s, positionLoc 0x%x\n", __func__, wl.egl.positionLoc);
    debug("%s, texCoordLoc 0x%x\n", __func__, wl.egl.texCoordLoc);
}

static void* draw_thread(void* pParam)
{
    int preflip = -1;

    debug("%s++\n", __func__);
    wl_create();
    egl_create();

    wl.init = 1;
    while (thread_run) {
        if (wl.ready && (preflip != wl.flip)) {
            preflip = wl.flip;

            glVertexAttribPointer(wl.egl.positionLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), egl_bg_vertices);
            glVertexAttribPointer(wl.egl.texCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &egl_bg_vertices[3]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, wl.info.w, wl.info.h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, wl.bg);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, egl_indices);

            glVertexAttribPointer(wl.egl.positionLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), egl_fb_vertices);
            glVertexAttribPointer(wl.egl.texCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &egl_fb_vertices[3]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, wl.info.w, wl.info.h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, wl.pixels[wl.flip ^ 1]);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, egl_indices);
            eglSwapBuffers(wl.egl.display, wl.egl.surface);
        }
        else {
            usleep(1000);
        }
    }
    return NULL;
}

static int XT894_Available(void)
{
    debug("%s\n", __func__);
    return 1;
}

static void XT894_DeleteDevice(SDL_VideoDevice* device)
{
    debug("%s\n", __func__);
    if (thread_run) {
        thread_run = 0;
        pthread_join(thread_id[0], NULL);
        pthread_join(thread_id[1], NULL);
        pthread_join(thread_id[2], NULL);
        egl_free();
        wl_free();
        free(wl.bg);
    }

    if (device) {
        SDL_free(device);
    }
}

static int XT894_HWAccelBlit(SDL_Surface* src, SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect)
{
    debug("%s\n", __func__);
    return 0;
}

static int XT894_CheckHWBlit(_THIS, SDL_Surface* src, SDL_Surface* dst)
{
    debug("%s\n", __func__);
    src->map->hw_blit = XT894_HWAccelBlit;
    return 1;
}

static SDL_Rect** XT894_ListModes(_THIS, SDL_PixelFormat* format, Uint32 flags)
{
    debug("%s\n", __func__);
    return (SDL_Rect**)-1;
}

static int XT894_VideoInit(_THIS, SDL_PixelFormat* vformat)
{
    debug("%s\n", __func__);
    thread_run = 1;
    wl.bg = SDL_malloc(LCD_W * LCD_H * 2);
    memset(wl.bg, 0, LCD_W * LCD_H * 2);
    pthread_create(&thread_id[0], NULL, wl_thread, NULL);
    pthread_create(&thread_id[1], NULL, keypad_thread, NULL);
    pthread_create(&thread_id[2], NULL, draw_thread, NULL);
    while (wl.init == 0) {
        usleep(100000);
    }
    return 0;
}

static SDL_Surface* XT894_SetVideoMode(_THIS, SDL_Surface* current, int width, int height, int bpp, Uint32 flags)
{
    debug("%s\n", __func__);

    if (width == 0) {
        width = 640;
    }
    if (height == 0) {
        height = 480;
    }
    bpp = 16;

    wl.ready = 0;
    wl.info.w = width;
    wl.info.h = height;
    wl.info.bpp = bpp;
    wl.info.size = wl.info.w * (wl.info.bpp / 8) * wl.info.h;
    debug("%s, w:%d, h:%d, bpp:%d\n", __func__, wl.info.w, wl.info.h, wl.info.bpp);

    int c0 = LCD_H / wl.info.w;
    int c1 = LCD_W / wl.info.h;
    int scale = c0 > c1 ? c1 : c0;
    debug("%s, scale:%d\n", __func__, scale);

    float y0 = ((float)(wl.info.w * scale) / LCD_H);
    float x0 = ((float)(wl.info.h * scale) / LCD_W);
    debug("%s, x0:%f, y0:%f\n", __func__, x0, y0);

    egl_fb_vertices[0] = -x0;
    egl_fb_vertices[1] = y0;
    egl_fb_vertices[5] = -x0;
    egl_fb_vertices[6] = -y0;
    egl_fb_vertices[10] =  x0;
    egl_fb_vertices[11] = -y0;
    egl_fb_vertices[15] =  x0;
    egl_fb_vertices[16] =  y0;
   
    // double buffer
    wl.flip = 0;
    wl.data = SDL_malloc(wl.info.size * 2);
    memset(wl.data, 0, wl.info.size *2);
    wl.pixels[0] = (uint16_t*)wl.data;
    wl.pixels[1] = (uint16_t*)(wl.data + wl.info.size);
    debug("%s, pixels:%p\n", __func__, wl.data);

	if (!SDL_ReallocFormat(current, bpp, 0, 0, 0, 0)) {
		SDL_SetError("failed to allocate new pixel format for requested mode");
		return NULL;
	}

    vsurf = current;
	current->flags = flags | SDL_DOUBLEBUF | SDL_PREALLOC;
	current->w = width;
	current->h = height;
    current->pitch = width * (bpp / 8);
    current->pixels = wl.pixels[wl.flip];
    wl.ready = 1;
    return current;
}

static int XT894_AllocHWSurface(_THIS, SDL_Surface* surface)
{
    debug("%s\n", __func__);
    return 0;
}

static void XT894_FreeHWSurface(_THIS, SDL_Surface* surface)
{
    debug("%s\n", __func__);
}

static int XT894_LockHWSurface(_THIS, SDL_Surface* surface)
{
    return 0;
}

static void XT894_UnlockHWSurface(_THIS, SDL_Surface* surface)
{
}

static int XT894_FlipHWSurface(_THIS, SDL_Surface* surface)
{
    if (wl.init && wl.ready) {
        wl.flip ^= 1;
        vsurf->pixels = wl.pixels[wl.flip];
    }
    return 0;
}

static void XT894_UpdateRects(_THIS, int numrects, SDL_Rect* rects)
{
    XT894_FlipHWSurface(NULL, NULL);
}

static int XT894_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color* colors)
{
    debug("%s\n", __func__);
    return 0;
}

static void XT894_VideoQuit(_THIS)
{
    debug("%s\n", __func__);
}

static SDL_VideoDevice* XT894_CreateDevice(int devindex)
{
    SDL_VideoDevice* device = NULL;

    debug("%s\n", __func__);
    device = (SDL_VideoDevice*)SDL_malloc(sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return 0;
    }

    device->VideoInit = XT894_VideoInit;
    device->ListModes = XT894_ListModes;
    device->SetVideoMode = XT894_SetVideoMode;
    device->CreateYUVOverlay = NULL;
    device->SetColors = XT894_SetColors;
    device->UpdateRects = XT894_UpdateRects;
    device->VideoQuit = XT894_VideoQuit;
    device->AllocHWSurface = XT894_AllocHWSurface;
    device->CheckHWBlit = XT894_CheckHWBlit;
    device->FillHWRect = NULL;
    device->SetHWColorKey = NULL;
    device->SetHWAlpha = NULL;
    device->LockHWSurface = XT894_LockHWSurface;
    device->UnlockHWSurface = XT894_UnlockHWSurface;
    device->FlipHWSurface = XT894_FlipHWSurface;
    device->FreeHWSurface = XT894_FreeHWSurface;
    device->SetCaption = NULL;
    device->SetIcon = NULL;
    device->IconifyWindow = NULL;
    device->GrabInput = NULL;
    device->GetWMInfo = NULL;
    device->InitOSKeymap = XT894_InitOSKeymap;
    device->PumpEvents = XT894_PumpEvents;
    device->free = XT894_DeleteDevice;
    device->CreateWMCursor = NULL;
    device->ShowWMCursor = NULL;
    device->CheckMouseMode = NULL;
    device->UpdateMouse = NULL;
    return device;
}

VideoBootStrap XT894_bootstrap = {"xt894", "SDL XT894 video driver", XT894_Available, XT894_CreateDevice};

