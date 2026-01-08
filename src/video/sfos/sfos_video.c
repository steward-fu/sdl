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

#if DEBUG
    #define debug(...) printf("[SDL] "__VA_ARGS__)
#else
    #define debug(...) (void)0
#endif

#define GL_BGRA 0x80E1

extern uint8_t mykey[KEY_MAX][2];

typedef enum {
    FILTER_PIXEL = 0,
    FILTER_BLUR
} sfos_filter_t;

static int pixel_perfect = 1;
static wayland wl = { 0 };
static sfos_filter_t filter = 0;
static int reload_shader = 0;
static char shader_name[MAX_PATH] = { 0 };

static EGLint surf_cfg[] = {
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

static EGLint ctx_cfg[] = {
    EGL_CONTEXT_CLIENT_VERSION, 
    2, 
    EGL_NONE
};

#if defined(XT897) || defined(XT894)
static GLfloat bg_vertices[] = {
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 
     1.0f,  1.0f, 0.0f, 1.0f, 0.0f
};
#endif

#if defined(QX1000) || defined(QX1050)
static GLfloat bg_vertices[] = {
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f,
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
     1.0f,  1.0f, 0.0f, 1.0f, 0.0f
};
#endif

static GLfloat fb_vertices[] = {
    -0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 
    -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
     0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 
     0.5f,  0.5f, 0.0f, 1.0f, 0.0f
};

static GLushort indices[] = {
    0, 1, 2, 0, 2, 3
};

static const char *def_vert_src =
"   attribute vec4 vert_tex_pos;                                            \n"
"   attribute vec2 vert_tex_coord;                                          \n"
"   varying vec2 frag_tex_coord;                                            \n"

"   void main()                                                             \n"
"   {                                                                       \n"
"       const float angle = 270.0 * (3.1415 * 2.0) / 360.0;                 \n"
"       mat4 rot = mat4(                                                    \n"
"           cos(angle), -sin(angle), 0.0, 0.0,                              \n"
"           sin(angle),  cos(angle), 0.0, 0.0,                              \n"
"                  0.0,         0.0, 1.0, 0.0,                              \n"
"                  0.0,         0.0, 0.0, 1.0);                             \n"
"       gl_Position = vert_tex_pos * rot;                                   \n"
"       frag_tex_coord = vert_tex_coord;                                    \n"
"}                                                                          \n";

static const char *def_frag_src =
"   precision highp float;                                                  \n"

"   varying vec2 frag_tex_coord;                                            \n"
"   uniform vec4 frag_screen;                                               \n"
"   uniform sampler2D frag_tex_sampler;                                     \n"

"   void main()                                                             \n"
"   {                                                                       \n"
"       vec3 tex = texture2D(frag_tex_sampler, frag_tex_coord).rgb;         \n"
"       gl_FragColor = vec4(tex, 1.0);                                      \n"
"   }                                                                       \n";

static const char *def_frag_lcd1x_src =
"precision highp float;\n"

"uniform vec4 frag_screen;\n"
"varying vec2 frag_tex_coord;\n"
"uniform sampler2D frag_tex_sampler;\n"

"#define BRIGHTEN_SCANLINES 16.0\n"
"#define BRIGHTEN_LCD 4.0\n"

"#define PI 3.141592654\n"

"const float NDS_SCREEN_HEIGHT = 240.0;\n"
"const float INV_BRIGHTEN_SCANLINES_INC = 1.0 / (BRIGHTEN_SCANLINES + 1.0);\n"
"const float INV_BRIGHTEN_LCD_INC = 1.0 / (BRIGHTEN_LCD + 1.0);\n"

"void main()\n"
"{\n"
"	vec2 angle = 2.0 * PI * (((frag_tex_coord.xy * frag_screen.zw) * NDS_SCREEN_HEIGHT * frag_screen.y) - 0.25);\n"

"	float yfactor = (BRIGHTEN_SCANLINES + sin(angle.y)) * INV_BRIGHTEN_SCANLINES_INC;\n"
"	float xfactor = (BRIGHTEN_LCD + sin(angle.x)) * INV_BRIGHTEN_LCD_INC;\n"

"	vec3 colour = texture2D(frag_tex_sampler, frag_tex_coord.xy).rgb;\n"
"	colour.rgb = yfactor * xfactor * colour.rgb;\n"

"	gl_FragColor = vec4(colour.rgb, 1.0);\n"
"}";

static const char *def_frag_lcd3x_src =
"precision highp float;\n"

"uniform vec4 frag_screen;\n"
"varying vec2 frag_tex_coord;\n"
"uniform sampler2D frag_tex_sampler;\n"

"#define PI 3.141592654\n"
"#define brighten_scanlines 16.0\n"
"#define brighten_lcd 4.0\n"
"void main()\n"
"{\n"
"    vec3 res = texture2D(frag_tex_sampler, frag_tex_coord).xyz;\n"
"    vec2 omega = PI * 2.0 * frag_screen.xy;\n"
"    vec3 offsets = PI * vec3(0.5, 0.5 - (2.0 / 3.0), 0.5 - (4.0 / 3.0));\n"
"    vec2 angle = frag_tex_coord * omega;\n"
"    float yfactor = (brighten_scanlines + sin(angle.y)) / (brighten_scanlines + 1.0);\n"
"    vec3 xfactors = (brighten_lcd + sin(angle.x + offsets)) / (brighten_lcd + 1.0);\n"
"    vec3 color = yfactor * xfactors * res;\n"
"    gl_FragColor = vec4(color.x, color.y, color.z, 1.0);\n"
"}\n";

static void cb_handle(
    void* dat,
    struct wl_registry* reg,
    uint32_t id,
    const char* intf,
    uint32_t ver)
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

    wl.disp_ready = 1;
    while (wl.thread.running) {
        if (wl.draw_ready) {
            wl_display_dispatch(wl.display);
        }
        else {
            usleep(15000);
        }
    }
    wl.disp_ready = 0;

    debug("%s--\n", __func__);
    return NULL;
}

static void* input_handler(void* pParam)
{
    int fd = -1;
    struct input_event ev = { 0 };
    const char *path = DEV_PATH;

    debug("%s++\n", __func__);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        debug("%s, failed to open %s\n", __func__, path);
        return NULL;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    while (wl.thread.running) {
        if (read(fd, &ev, sizeof(struct input_event)) > 0) {
            if (ev.type == EV_KEY) {
                mykey[ev.code][ev.value] = 1;
                debug("%s, code:%d, value:%d\n", __func__, ev.code, ev.value);
            }
        }
        else {
            usleep(15000);
        }
    }
    close(fd);

    debug("%s--\n", __func__);
    return NULL;
}

static void cb_ping(void* dat, struct wl_shell_surface* shell_surf, uint32_t serial)
{
    wl_shell_surface_pong(shell_surf, serial);
}

static void cb_configure(
    void* dat,
    struct wl_shell_surface* shell_surf,
    uint32_t edges,
    int32_t w,
    int32_t h)
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

static void egl_quit(void)
{
    eglSwapBuffers(wl.egl.display, wl.egl.surface);
    eglDestroySurface(wl.egl.display, wl.egl.surface);
    eglDestroyContext(wl.egl.display, wl.egl.context);
    eglTerminate(wl.egl.display);

    glUseProgram(0);
    glDeleteProgram(wl.egl.program);
}

static void wl_quit(void)
{
    wl_shell_surface_destroy(wl.shell_surface);
    wl_shell_destroy(wl.shell);
    wl_surface_destroy(wl.surface);
    wl_compositor_destroy(wl.compositor);
    wl_registry_destroy(wl.registry);
    wl_egl_window_destroy(wl.window);
    wl_display_disconnect(wl.display);

    free(wl.bg);
    wl.bg = NULL;
    for (int i = 0; i <MAX_FB; i++) {
        free(wl.fg[i]);
        wl.fg[i] = NULL;
    }
}

static void wl_init(void)
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

    debug("%s, wl.display @%p\n", __func__, wl.display);
    debug("%s, wl.registry @%p\n", __func__, wl.registry);
    debug("%s, wl.surface @%p\n", __func__, wl.surface);
    debug("%s, wl.shell @%p\n", __func__, wl.shell);
    debug("%s, wl.shell_surface @%p\n", __func__, wl.shell_surface);
    debug("%s, wl.region @%p\n", __func__, wl.region);

    wl.bg = calloc(LCD_W * LCD_H * 4, 1);
    for (int i = 0; i < MAX_FB; i++) {
        wl.fg[i] = calloc(LCD_W * LCD_H * 4, 1);
    }
}

static int apply_shader_code(const char *name)
{
    int r = 0;
    GLint success = 0;
    GLint frag_shader = 0;
    GLint vert_shader = 0;
    const char *vert_src = def_vert_src;
    const char *frag_src = def_frag_src;

    debug("call %s(name=%p)\n", __func__, name);

    if (name && !strcmp(name, "LCD1X")) {
        frag_src = def_frag_lcd1x_src;
        debug("use LCD1X\n");
    }
    else if (name && !strcmp(name, "LCD3X")) {
        frag_src = def_frag_lcd3x_src;
        debug("use LCD3X\n");
    }

    do {
        vert_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vert_shader, 1, (const char * const *)&vert_src, NULL);
        glCompileShader(vert_shader);
        debug("vert_shader id=%d\n", vert_shader);

        glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &success);
        debug("vert_shader compile status (%s)\n", success ? "success" : "fail");
        if (!success) {
            r = -1;
            debug("failed to compile vertex shader\n");
            break;
        }

        frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(frag_shader, 1, (const char * const *)&frag_src, NULL);
        glCompileShader(frag_shader);
        debug("frag_shader id=%d\n", frag_shader);

        glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &success);
        debug("frag_shader compile status (%s)\n", success ? "success" : "fail");
        if (!success) {
            r = -1;
            debug("failed to compile fragment shader\n");
            break;
        }

        glUseProgram(0);
        if (wl.egl.program) {
            glDeleteProgram(wl.egl.program);
            if (glIsProgram(wl.egl.program)) {
                debug("opengl es program still exists\n");
            }
            else {
                debug("opengl es program deleted\n");
            }
        }

        wl.egl.program = glCreateProgram();
        glAttachShader(wl.egl.program, vert_shader);
        glAttachShader(wl.egl.program, frag_shader);
        glLinkProgram(wl.egl.program);
        glDeleteShader(vert_shader);
        glDeleteShader(frag_shader);

        glGetProgramiv(wl.egl.program, GL_LINK_STATUS, &success);
        debug("opengl es program link status (%s)\n", success ? "success" : "fail");
        if (!success) {
            r = -1;
            debug("failed to link program\n");
            break;
        }

        glUseProgram(wl.egl.program);

        wl.egl.pos = glGetAttribLocation(wl.egl.program, "vert_tex_pos");
        wl.egl.coord = glGetAttribLocation(wl.egl.program, "vert_tex_coord");
        wl.egl.screen = glGetUniformLocation(wl.egl.program, "frag_screen");
        wl.egl.sampler = glGetUniformLocation(wl.egl.program, "frag_tex_sampler");
    } while(0);

    return r;
}

static void egl_init(void)
{
    EGLint cnt = 0;
    EGLint major = 0;
    EGLint minor = 0;
    EGLConfig cfg = 0;
    const char *shader = getenv("SFOS_SHADER");

    wl.egl.display = eglGetDisplay((EGLNativeDisplayType)wl.display);
    eglInitialize(wl.egl.display, &major, &minor);
    eglGetConfigs(wl.egl.display, NULL, 0, &cnt);
    eglChooseConfig(wl.egl.display, surf_cfg, &cfg, 1, &cnt);
    wl.egl.surface = eglCreateWindowSurface(wl.egl.display, cfg, wl.window, NULL);
    wl.egl.context = eglCreateContext(wl.egl.display, cfg, EGL_NO_CONTEXT, ctx_cfg);
    eglMakeCurrent(wl.egl.display, wl.egl.surface, wl.egl.surface, wl.egl.context);

    apply_shader_code(shader);

    glGenTextures(1, &wl.egl.tex);
    glBindTexture(GL_TEXTURE_2D, wl.egl.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

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
}

static void* draw_handler(void* pParam)
{
    int pre_flip = -1;
    int pre_app_flip = -1;

    debug("%s++\n", __func__);

    wl_init();
    egl_init();
    wl.draw_ready = 1;

    while (wl.thread.running) {
        if (reload_shader) {
            reload_shader = 0;
            apply_shader_code(shader_name);
        }

        if (wl.disp_ready && ((pre_flip != wl.flip) || (pre_app_flip != wl.app_flip))) {
            pre_flip = wl.flip;
            pre_app_flip = wl.app_flip;

            glUniform4f(wl.egl.screen, wl.info.w, wl.info.h, 1.0 / wl.info.w, 1.0 / wl.info.h);

            glVertexAttribPointer(
                wl.egl.pos,
                3,
                GL_FLOAT,
                GL_FALSE,
                5 * sizeof(GLfloat),
                fb_vertices
            );

            glVertexAttribPointer(
                wl.egl.coord,
                2,
                GL_FLOAT,
                GL_FALSE,
                5 * sizeof(GLfloat),
                &fb_vertices[3]
            );

            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                wl.info.bpp == 16 ? GL_RGB : (wl.swap_color ? GL_BGRA : GL_RGBA),
                wl.info.w,
                wl.info.h,
                0,
                wl.info.bpp == 16 ? GL_RGB : (wl.swap_color ? GL_BGRA : GL_RGBA),
                wl.info.bpp == 16 ? GL_UNSIGNED_SHORT_5_6_5 : GL_UNSIGNED_BYTE,
                wl.app_fg ? wl.app_fg : wl.fg[wl.flip ^ 1]
            );
            wl.app_fg = NULL;

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
            eglSwapBuffers(wl.egl.display, wl.egl.surface);
            debug("swap buffer\n");
#if 1
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
#else
            glVertexAttribPointer(
                wl.egl.pos,
                3,
                GL_FLOAT,
                GL_FALSE,
                5 * sizeof(GLfloat),
                bg_vertices
            );

            glVertexAttribPointer(
                wl.egl.coord,
                2,
                GL_FLOAT,
                GL_FALSE,
                5 * sizeof(GLfloat),
                &bg_vertices[3]
            );

            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGB,
                LCD_H,
                LCD_W,
                0,
                GL_RGB,
                GL_UNSIGNED_SHORT_5_6_5,
                wl.bg
            );

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
#endif
        }
        else {
            usleep(10);
        }
    }

    egl_quit();
    wl_quit();
    wl.draw_ready = 0;

    debug("%s--\n", __func__);

    return NULL;
}

static int available(void)
{
    debug("%s\n", __func__);

    return 1;
}

static void delete_device(SDL_VideoDevice* device)
{
    debug("%s\n", __func__);

    if (wl.thread.running) {
        wl.thread.running = 0;
        pthread_join(wl.thread.id[0], NULL);
        pthread_join(wl.thread.id[1], NULL);
        pthread_join(wl.thread.id[2], NULL);
    }

    if (device) {
        SDL_free(device);
    }
}

static int hw_accel_blit(SDL_Surface* src, SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect)
{
    debug("%s\n", __func__);

    return 0;
}

static int check_hw_blit(_THIS, SDL_Surface* src, SDL_Surface* dst)
{
    debug("%s\n", __func__);

    src->map->hw_blit = hw_accel_blit;
    return 1;
}

static SDL_Rect** list_modes(_THIS, SDL_PixelFormat* format, Uint32 flags)
{
    debug("%s\n", __func__);

    return (SDL_Rect**)-1;
}

static int video_init(_THIS, SDL_PixelFormat* vformat)
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

    var = getenv("SFOS_STRETCH");
    if (var && !strcmp(var, "ASPECT")) {
        pixel_perfect = 0;
    }
    else {
        pixel_perfect = 1;
    }

    wl.thread.running = 1;
    pthread_create(&wl.thread.id[0], NULL, disp_handler, NULL);
    pthread_create(&wl.thread.id[1], NULL, input_handler, NULL);
    pthread_create(&wl.thread.id[2], NULL, draw_handler, NULL);
    while ((wl.disp_ready == 0) || (wl.draw_ready == 0)) {
        usleep(15000);
    }

    this->info.current_w = LCD_H;
    this->info.current_h = LCD_W;
    debug("set window size as %dx%d\n", this->info.current_w, this->info.current_h);

    return 0;
}

static int change_geometry(int w, int h, int bpp)
{
    debug("call %s(w=%d, h=%d, bpp=%d)\n", __func__, w, h, bpp);

    if ((w == 0) || (h == 0) || (bpp == 0)) {
        w = 640;
        h = 480;
        bpp = 16;
    }

    wl.info.w = w;
    wl.info.h = h;
    wl.info.bpp = bpp;
    wl.info.size = wl.info.w * (wl.info.bpp / 8) * wl.info.h;
    debug("%s, w=%d, h=%d, bpp=%d\n", __func__, wl.info.w, wl.info.h, wl.info.bpp);

    float c0 = (float)LCD_H / wl.info.w;
    float c1 = (float)LCD_W / wl.info.h;
    float scale = c0 > c1 ? c1 : c0;

    if (pixel_perfect) {
        scale = (int)scale;
    }
    if (scale <= 0) {
        scale = 1;
    }
    debug("%s, scale=%lf\n", __func__, scale);

    float y0 = ((float)(wl.info.h * scale) / LCD_W);
    float x0 = ((float)(wl.info.w * scale) / LCD_H);

    fb_vertices[0] = -x0;
    fb_vertices[1] = y0;
    fb_vertices[5] = -x0;
    fb_vertices[6] = -y0;
    fb_vertices[10] =  x0;
    fb_vertices[11] = -y0;
    fb_vertices[15] =  x0;
    fb_vertices[16] =  y0;

    return 0;
}

static SDL_Surface* set_video_mode(_THIS, SDL_Surface* current, int w, int h, int bpp, Uint32 flags)
{
    debug("call %s(w=%d, h=%d, bpp=%d)\n", __func__, w, h, bpp);

    change_geometry(w, h, bpp);

	if (!SDL_ReallocFormat(current, bpp, 0, 0, 0, 0)) {
		SDL_SetError("failed to allocate new pixel format for requested mode");
		return NULL;
	}

    wl.swap_color = 0;
	current->flags = flags | SDL_DOUBLEBUF | SDL_PREALLOC;
	current->w = w;
	current->h = h;
    current->pitch = w * (bpp / 8);
    current->pixels = wl.fg[wl.flip];

    return current;
}

static int alloc_hw_surface(_THIS, SDL_Surface* surface)
{
    debug("%s\n", __func__);

    return 0;
}

static void free_hw_surface(_THIS, SDL_Surface* surface)
{
    debug("%s\n", __func__);
}

static int lock_hw_surface(_THIS, SDL_Surface* surface)
{
    return 0;
}

static void unlock_hw_surface(_THIS, SDL_Surface* surface)
{
}

static int flip_hw_surface(_THIS, SDL_Surface* surface)
{
    debug("call %s\n", __func__);

    if (wl.disp_ready && wl.draw_ready) {
        if (surface) {
            wl.flip ^= 1;
            debug("%s, flip=%d\n", __func__, wl.flip);

            surface->pixels = wl.fg[wl.flip];
            debug("%s, update surface buffer to %p\n", __func__, surface->pixels);
        }
    }

    return 0;
}

static void update_rects(_THIS, int numrects, SDL_Rect* rects)
{
    flip_hw_surface(NULL, NULL);
}

static int set_colors(_THIS, int firstcolor, int ncolors, SDL_Color* colors)
{
    debug("%s\n", __func__);

    return 0;
}

static void video_quit(_THIS)
{
    debug("%s\n", __func__);
}

static SDL_VideoDevice* create_device(int devindex)
{
    SDL_VideoDevice* device = NULL;

    debug("%s\n", __func__);

    device = (SDL_VideoDevice*)SDL_malloc(sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return 0;
    }

    device->VideoInit = video_init;
    device->ListModes = list_modes;
    device->SetVideoMode = set_video_mode;
    device->CreateYUVOverlay = NULL;
    device->SetColors = set_colors;
    device->UpdateRects = update_rects;
    device->VideoQuit = video_quit;
    device->AllocHWSurface = alloc_hw_surface;
    device->CheckHWBlit = check_hw_blit;
    device->FillHWRect = NULL;
    device->SetHWColorKey = NULL;
    device->SetHWAlpha = NULL;
    device->LockHWSurface = lock_hw_surface;
    device->UnlockHWSurface = unlock_hw_surface;
    device->FlipHWSurface = flip_hw_surface;
    device->FreeHWSurface = free_hw_surface;
    device->SetCaption = NULL;
    device->SetIcon = NULL;
    device->IconifyWindow = NULL;
    device->GrabInput = NULL;
    device->GetWMInfo = NULL;
    device->InitOSKeymap = sfos_init_os_keymap;
    device->PumpEvents = sfos_pump_events;
    device->free = delete_device;
    device->CreateWMCursor = NULL;
    device->ShowWMCursor = NULL;
    device->CheckMouseMode = NULL;
    device->UpdateMouse = NULL;

    return device;
}

VideoBootStrap sfos_bootstrap = {
    "sfos",
    "SDL Video Driver for Sailfish OS",
    available,
    create_device
};

int fast_flip(const void *p, int wait)
{
    wl.app_fg = (void *)p;
    wl.app_flip ^= 1;
    debug("%s, wl.app_flip=%d, p=%p, wait=%d\n", __func__, wl.app_flip, p, wait);

    debug("%s, wait++\n", __func__);
    while (wait && wl.app_fg) {
        usleep(10);
    }
    debug("%s, wait--\n", __func__);

    return 0;
}

int swap_color(int val)
{
    wl.swap_color = !!val;

    return 0;
}

int is_fast_done(void)
{
    return wl.app_fg == NULL ? 1 : 0;
}

int load_shader_code(const char *name)
{
    reload_shader = 1;

    shader_name[0] = 0;
    if (name) {
        strcpy(shader_name, name);
    }

    return 0;
}

#endif

