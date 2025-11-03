// LGPL-2.1 License
// (C) 2025 Steward Fu <steward.fu@gmail.com>

#include "SDL_config.h"

#if SDL_VIDEO_DRIVER_SFOS

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "sfos_video.h"
#include "sfos_event.h"

#include <SDL_image.h>

#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#if SFOS_DEBUG
    #define debug(...) printf("[SDL] "__VA_ARGS__)
#else
    #define debug(...) (void)0
#endif

#if defined(XT897)
    #define BG_HOME "/home/nemo/Data/sdl/border"
#endif

#if defined(XT894) || defined(QX1000)
    #define BG_HOME "/home/defaultuser/Data/sdl/border"
#endif

extern uint8_t mykey[KEY_MAX][2];

typedef enum {
    FILTER_PIXEL = 0,
    FILTER_BLUR
} sfos_filter_t;

static wayland wl = { 0 };
static sfos_filter_t filter = 0;
static SDL_Surface *vsurf = NULL;

EGLint surf_cfg[] = {
    EGL_SURFACE_TYPE,
    EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE,
    EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE,
    8,
    EGL_GREEN_SIZE,
    8,
    EGL_BLUE_SIZE,
    8,
    EGL_ALPHA_SIZE,
    8,
    EGL_NONE
};

EGLint ctx_cfg[] = {
    EGL_CONTEXT_CLIENT_VERSION, 
    2, 
    EGL_NONE
};

#if defined(XT897) || defined(XT894)
GLfloat bg_vertices[] = {
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 
     1.0f,  1.0f, 0.0f, 1.0f, 0.0f
};
#endif

#if defined(QX1000)
GLfloat bg_vertices[] = {
    -1.0f,  0.889f, 0.0f, 0.0f, 0.0f,
    -1.0f, -0.889f, 0.0f, 0.0f, 1.0f,
     1.0f, -0.889f, 0.0f, 1.0f, 1.0f,
     1.0f,  0.889f, 0.0f, 1.0f, 0.0f
};
#endif

GLfloat fb_vertices[] = {
    -0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 
    -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
     0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 
     0.5f,  0.5f, 0.0f, 1.0f, 0.0f
};

GLushort indices[] = {
    0, 1, 2, 0, 2, 3
};

static const char *vert_shader_code =
    "attribute vec4 vert_pos;                                           \n"
    "attribute vec2 vert_coord;                                         \n"
    "varying vec2 frag_coord;                                           \n"
    "void main()                                                        \n"
    "{                                                                  \n"
    "    gl_Position = vert_pos;                                        \n"
    "    frag_coord = vert_coord;                                       \n"
    "}                                                                  \n";

static const char *frag_shader_code =
    "precision highp float;                                             \n"
    "varying vec2 frag_coord;                                           \n"
    "uniform int frag_swap_color;                                       \n"
    "uniform float frag_aspect;                                         \n"
    "uniform float frag_angle;                                          \n"
    "uniform sampler2D frag_sampler;                                    \n"
    "const vec2 HALF = vec2(0.5);                                       \n"
    "void main()                                                        \n"
    "{                                                                  \n"
    "    vec3 tex;                                                      \n"
    "    float aSin = sin(frag_angle);                                  \n"
    "    float aCos = cos(frag_angle);                                  \n"
    "    vec2 tc = frag_coord;                                          \n"
    "    mat2 rotMat = mat2(aCos, -aSin, aSin, aCos);                   \n"
    "    mat2 scaleMat = mat2(frag_aspect, 0.0, 0.0, 1.0);              \n"
    "    mat2 scaleMatInv = mat2(1.0 / frag_aspect, 0.0, 0.0, 1.0);     \n"
    "    tc -= HALF.xy;                                                 \n"
    "    tc = scaleMatInv * rotMat * scaleMat * tc;                     \n"
    "    tc += HALF.xy;                                                 \n"
    "    if (frag_swap_color >= 1) {                                    \n"
    "        tex = texture2D(frag_sampler, tc).bgr;                     \n"
    "    }                                                              \n"
    "    else {                                                         \n"
    "        tex = texture2D(frag_sampler, tc).rgb;                     \n"
    "    }                                                              \n"
    "    gl_FragColor = vec4(tex, 1.0);                                 \n"
    "}                                                                  \n";

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

static struct wl_registry_listener cb_global = {
    .global = cb_handle,
    .global_remove = cb_remove
};

static void* disp_handler(void* pParam)
{
    debug("%s++\n", __func__);

    while (wl.thread.running) {
        if (wl.init && wl.ready) {
            wl_display_dispatch(wl.display);
        }
        else {
            usleep(1000);
        }
    }

    debug("%s--\n", __func__);
    return NULL;
}

static void* input_handler(void* pParam)
{
    int fd = -1;
    int bg_idx = 0;
    int bg_needs_init = 1;
    char buf[255] = { 0 };
    struct input_event ev = { 0 };
    const char *path = DEV_PATH;
    SDL_Surface *cvt = NULL;

    debug("%s++\n", __func__);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        debug("%s, failed to open %s\n", __func__, path);
        return NULL;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    cvt = SDL_CreateRGBSurface(SDL_SWSURFACE, 128, 128, 16, 0, 0, 0, 0);
    while (wl.thread.running) {
        if (read(fd, &ev, sizeof(struct input_event)) > 0) {
            if (ev.type == EV_KEY) {
                mykey[ev.code][ev.value] = 1;
                debug("%s, code:%d, value:%d\n", __func__, ev.code, ev.value);

                if (bg_needs_init ||
                    ((ev.code == KEY_LEFTCTRL) && (ev.value == 1)) ||
                    ((ev.code == KEY_RIGHTCTRL) && (ev.value == 1)) ||
                    ((ev.code == KEY_RIGHTSHIFT) && (ev.value == 1)) ||
                    ((ev.code == KEY_CAMERA) && (ev.value == 1)))
                {
                    bg_needs_init = 0;
                    snprintf(buf, sizeof(buf), BG_HOME "/%d.png", bg_idx++);
                    if (access(buf, F_OK) != 0) {
                        bg_idx = 0;
                        snprintf(buf, sizeof(buf), BG_HOME "/%d.png", bg_idx);
                    }

                    debug("use bg image \'%s\'\n", buf);
                    SDL_Surface *png = IMG_Load(buf);
                    if (png) {
                        SDL_Surface *t = SDL_ConvertSurface(png, cvt->format, 0);
                        if (t) {
                            memcpy(wl.bg, t->pixels, t->w * t->h * 2);
                            SDL_FreeSurface(t);
                        }
                        SDL_FreeSurface(png);
                    }
                }
            }
        }
        usleep(1000);
    }
    close(fd);

    if (cvt) {
        SDL_FreeSurface(cvt);
    }

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

void egl_free(void)
{
    wl.init = 0;
    wl.ready = 0;
    eglDestroySurface(wl.egl.display, wl.egl.surface);
    eglDestroyContext(wl.egl.display, wl.egl.context);
    wl_egl_window_destroy(wl.window);
    eglTerminate(wl.egl.display);

#if !defined(XT894) && !defined(XT897)
    glDeleteShader(wl.egl.vert_shader);
    glDeleteShader(wl.egl.frag_shader);
    glDeleteProgram(wl.egl.prog_obj);
#endif
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
    free(wl.bg);
    wl.bg = NULL;
    free(wl.data);
    wl.data = NULL;
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
    wl.window = wl_egl_window_create(wl.surface, LCD_W, LCD_H);

    debug("%s, wl.display=%p\n", __func__, wl.display);
    debug("%s, wl.registry=%p\n", __func__, wl.registry);
    debug("%s, wl.surface=%p\n", __func__, wl.surface);
    debug("%s, wl.shell=%p\n", __func__, wl.shell);
    debug("%s, wl.shell_surface=%p\n", __func__, wl.shell_surface);
    debug("%s, wl.region=%p\n", __func__, wl.region);

    wl.data = SDL_malloc(LCD_W * LCD_H * 4);
    memset(wl.data, 0, LCD_W * LCD_H * 4);

    wl.bg = SDL_malloc(LCD_W * LCD_H * 2);
    memset(wl.bg, 0, LCD_W * LCD_H * 2);
}

void egl_create(void)
{
    EGLint cnt = 0;
    EGLint major = 0;
    EGLint minor = 0;
    EGLConfig cfg = 0;

    wl.egl.display = eglGetDisplay((EGLNativeDisplayType)wl.display);
    eglInitialize(wl.egl.display, &major, &minor);
    eglGetConfigs(wl.egl.display, NULL, 0, &cnt);
    eglChooseConfig(wl.egl.display, surf_cfg, &cfg, 1, &cnt);
    wl.egl.surface = eglCreateWindowSurface(wl.egl.display, cfg, wl.window, NULL);
    wl.egl.context = eglCreateContext(wl.egl.display, cfg, EGL_NO_CONTEXT, ctx_cfg);
    eglMakeCurrent(wl.egl.display, wl.egl.surface, wl.egl.surface, wl.egl.context);

    debug("%s, egl_display=%p\n", __func__, wl.egl.display);
    debug("%s, egl_window=%p\n", __func__, wl.window);
    debug("%s, egl_surface=%p\n", __func__, wl.egl.surface);
    debug("%s, egl_context=%p\n", __func__, wl.egl.context);

    wl.egl.vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(wl.egl.vert_shader, 1, &vert_shader_code, NULL);
    glCompileShader(wl.egl.vert_shader);

    GLint compiled = 0;
    glGetShaderiv(wl.egl.vert_shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint len = 0;
        glGetShaderiv(wl.egl.vert_shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            char* info = malloc(sizeof(char) * len);
            glGetShaderInfoLog(wl.egl.vert_shader, len, NULL, info);
            debug("%s, failed to compile vert_shader: %s\n", __func__, info);
            free(info);
        }
    }

    wl.egl.frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(wl.egl.frag_shader, 1, &frag_shader_code, NULL);
    glCompileShader(wl.egl.frag_shader);
    
    glGetShaderiv(wl.egl.frag_shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint len = 0;
        glGetShaderiv(wl.egl.frag_shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            char* info = malloc(sizeof(char) * len);
            glGetShaderInfoLog(wl.egl.frag_shader, len, NULL, info);
            debug("%s, failed to compile frag_Shader: %s\n", __func__, info);
            free(info);
        }
    }

    wl.egl.prog_obj = glCreateProgram();
    glAttachShader(wl.egl.prog_obj, wl.egl.vert_shader);
    glAttachShader(wl.egl.prog_obj, wl.egl.frag_shader);
    glLinkProgram(wl.egl.prog_obj);
    glUseProgram(wl.egl.prog_obj);

    wl.egl.pos = glGetAttribLocation(wl.egl.prog_obj, "vert_pos");
    wl.egl.coord = glGetAttribLocation(wl.egl.prog_obj, "vert_coord");
    wl.egl.sampler = glGetUniformLocation(wl.egl.prog_obj, "frag_sampler");
    glUniform1f(glGetUniformLocation(wl.egl.prog_obj, "frag_angle"), 90 * (3.1415 * 2.0) / 360.0);
    glUniform1f(glGetUniformLocation(wl.egl.prog_obj, "frag_aspect"), 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGenTextures(1, &wl.egl.tex);
    glBindTexture(GL_TEXTURE_2D, wl.egl.tex);

    if (filter == FILTER_PIXEL) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glViewport(0, 0, LCD_W, LCD_H);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glVertexAttribPointer(wl.egl.pos, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), bg_vertices);
    glVertexAttribPointer(wl.egl.coord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &bg_vertices[3]);
    glEnableVertexAttribArray(wl.egl.pos);
    glEnableVertexAttribArray(wl.egl.coord);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(wl.egl.sampler, 0);

    debug("%s, textureId   0x%x\n", __func__, wl.egl.tex);
    debug("%s, samplerLoc  0x%x\n", __func__, wl.egl.sampler);
    debug("%s, positionLoc 0x%x\n", __func__, wl.egl.pos);
    debug("%s, texCoordLoc 0x%x\n", __func__, wl.egl.coord);
}

static void* draw_handler(void* pParam)
{
    int pre_flip = -1;

    debug("%s++\n", __func__);
    wl_create();
    egl_create();

    wl.init = 1;
    while (wl.thread.running) {
        if (wl.ready && (pre_flip != wl.flip)) {
            pre_flip = wl.flip;
            glVertexAttribPointer(wl.egl.pos, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), bg_vertices);
            glVertexAttribPointer(wl.egl.coord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &bg_vertices[3]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 960, 540, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, wl.bg);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

            glVertexAttribPointer(wl.egl.pos, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), fb_vertices);
            glVertexAttribPointer(wl.egl.coord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &fb_vertices[3]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, wl.info.w, wl.info.h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, wl.pixels[wl.flip ^ 1]);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
            eglSwapBuffers(wl.egl.display, wl.egl.surface);
        }
        else {
            usleep(10);
        }
    }
    return NULL;
}

static int SFOS_Available(void)
{
    debug("%s\n", __func__);

    return 1;
}

static void SFOS_DeleteDevice(SDL_VideoDevice* device)
{
    debug("%s\n", __func__);

    if (wl.thread.running) {
        wl.thread.running = 0;
        pthread_join(wl.thread.id[0], NULL);
        pthread_join(wl.thread.id[1], NULL);
        pthread_join(wl.thread.id[2], NULL);
        egl_free();
        wl_free();
    }

    if (device) {
        SDL_free(device);
    }
}

static int SFOS_HWAccelBlit(SDL_Surface* src, SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect)
{
    debug("%s\n", __func__);
    return 0;
}

static int SFOS_CheckHWBlit(_THIS, SDL_Surface* src, SDL_Surface* dst)
{
    debug("%s\n", __func__);
    src->map->hw_blit = SFOS_HWAccelBlit;
    return 1;
}

static SDL_Rect** SFOS_ListModes(_THIS, SDL_PixelFormat* format, Uint32 flags)
{
    debug("%s\n", __func__);

    return (SDL_Rect**)-1;
}

static int SFOS_VideoInit(_THIS, SDL_PixelFormat* vformat)
{
    const char *var = NULL;

    debug("%s\n", __func__);

    var = getenv("SFOS_FILTER");
    if (var && !strcmp(var, "BLUR")) {
        filter = FILTER_BLUR;
    }
    else {
        filter = FILTER_PIXEL;
    }

    wl.thread.running = 1;
    pthread_create(&wl.thread.id[0], NULL, disp_handler, NULL);
    pthread_create(&wl.thread.id[1], NULL, input_handler, NULL);
    pthread_create(&wl.thread.id[2], NULL, draw_handler, NULL);
    while (wl.init == 0) {
        usleep(100000);
    }
    return 0;
}

static SDL_Surface* SFOS_SetVideoMode(_THIS, SDL_Surface* current, int width, int height, int bpp, Uint32 flags)
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

    float c0 = (float)LCD_H / wl.info.w;
    float c1 = (float)LCD_W / wl.info.h;
    float scale = c0 > c1 ? c1 : c0;

    if (filter == FILTER_PIXEL) {
        scale = (int)scale;
    }
    if (scale <= 0) {
        scale = 1;
    }
    debug("%s, scale:%d\n", __func__, scale);

    float y0 = ((float)(wl.info.w * scale) / LCD_H);
    float x0 = ((float)(wl.info.h * scale) / LCD_W);
    debug("%s, x0:%f, y0:%f\n", __func__, x0, y0);

    fb_vertices[0] = -x0;
    fb_vertices[1] = y0;
    fb_vertices[5] = -x0;
    fb_vertices[6] = -y0;
    fb_vertices[10] =  x0;
    fb_vertices[11] = -y0;
    fb_vertices[15] =  x0;
    fb_vertices[16] =  y0;

    // double buffer
    wl.flip = 0;
    memset(wl.data, 0, wl.info.size * 2);
    wl.pixels[0] = (uint16_t *)wl.data;
    wl.pixels[1] = (uint16_t *)(wl.data + wl.info.size);
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

static int SFOS_AllocHWSurface(_THIS, SDL_Surface* surface)
{
    debug("%s\n", __func__);
    return 0;
}

static void SFOS_FreeHWSurface(_THIS, SDL_Surface* surface)
{
    debug("%s\n", __func__);
}

static int SFOS_LockHWSurface(_THIS, SDL_Surface* surface)
{
    return 0;
}

static void SFOS_UnlockHWSurface(_THIS, SDL_Surface* surface)
{
}

static int SFOS_FlipHWSurface(_THIS, SDL_Surface* surface)
{
    if (wl.init && wl.ready) {
        wl.flip ^= 1;
        vsurf->pixels = wl.pixels[wl.flip];
    }
    return 0;
}

static void SFOS_UpdateRects(_THIS, int numrects, SDL_Rect* rects)
{
    SFOS_FlipHWSurface(NULL, NULL);
}

static int SFOS_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color* colors)
{
    debug("%s\n", __func__);
    return 0;
}

static void SFOS_VideoQuit(_THIS)
{
    debug("%s\n", __func__);
}

static SDL_VideoDevice* SFOS_CreateDevice(int devindex)
{
    SDL_VideoDevice* device = NULL;

    debug("%s\n", __func__);

    device = (SDL_VideoDevice*)SDL_malloc(sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return 0;
    }

    device->VideoInit = SFOS_VideoInit;
    device->ListModes = SFOS_ListModes;
    device->SetVideoMode = SFOS_SetVideoMode;
    device->CreateYUVOverlay = NULL;
    device->SetColors = SFOS_SetColors;
    device->UpdateRects = SFOS_UpdateRects;
    device->VideoQuit = SFOS_VideoQuit;
    device->AllocHWSurface = SFOS_AllocHWSurface;
    device->CheckHWBlit = SFOS_CheckHWBlit;
    device->FillHWRect = NULL;
    device->SetHWColorKey = NULL;
    device->SetHWAlpha = NULL;
    device->LockHWSurface = SFOS_LockHWSurface;
    device->UnlockHWSurface = SFOS_UnlockHWSurface;
    device->FlipHWSurface = SFOS_FlipHWSurface;
    device->FreeHWSurface = SFOS_FreeHWSurface;
    device->SetCaption = NULL;
    device->SetIcon = NULL;
    device->IconifyWindow = NULL;
    device->GrabInput = NULL;
    device->GetWMInfo = NULL;
    device->InitOSKeymap = SFOS_InitOSKeymap;
    device->PumpEvents = SFOS_PumpEvents;
    device->free = SFOS_DeleteDevice;
    device->CreateWMCursor = NULL;
    device->ShowWMCursor = NULL;
    device->CheckMouseMode = NULL;
    device->UpdateMouse = NULL;
    return device;
}

VideoBootStrap SFOS_bootstrap = {
    "sfos",
    "SDL Video Driver for Sailfish OS",
    SFOS_Available,
    SFOS_CreateDevice
};

#endif

