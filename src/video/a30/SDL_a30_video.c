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

struct _video vid = {0};

GLfloat bgVertices[] = {
   -1.0f,  1.0f,  0.0f,  0.0f,  0.0f,
   -1.0f, -1.0f,  0.0f,  0.0f,  1.0f,
    1.0f, -1.0f,  0.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  0.0f,  1.0f,  0.0f
};

GLfloat vVertices[] = {
   -1.0f,  1.0f,  0.0f,  0.0f,  0.0f,
   -1.0f, -1.0f,  0.0f,  0.0f,  1.0f,
    1.0f, -1.0f,  0.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  0.0f,  1.0f,  0.0f
};
GLushort indices[] = {0, 1, 2, 0, 2, 3};

const char *vShaderSrc =
    "attribute vec4 a_position;   \n"
    "attribute vec2 a_texCoord;   \n"
    "attribute float a_alpha;     \n"
    "varying vec2 v_texCoord;     \n"
    "void main()                  \n"
    "{                            \n"
    "    const float angle = 90.0 * (3.1415 * 2.0) / 360.0;                                                                            \n"
    "    mat4 rot = mat4(cos(angle), -sin(angle), 0.0, 0.0, sin(angle), cos(angle), 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0); \n"
    "    gl_Position = a_position * rot; \n"
    "    v_texCoord = a_texCoord;        \n"
    "}                                   \n";

const char *fShaderSrc =
    "precision mediump float;                                  \n"
    "varying vec2 v_texCoord;                                  \n"
    "uniform float s_alpha;                                    \n"
    "uniform sampler2D s_texture;                              \n"
    "void main()                                               \n"
    "{                                                         \n"
    "    if (s_alpha >= 2.0) {                                 \n"
    "        gl_FragColor = texture2D(s_texture, v_texCoord);  \n"
    "    }                                                     \n"
    "    else if (s_alpha > 0.0) {                             \n"
    "        vec3 tex = texture2D(s_texture, v_texCoord).rgb;  \n"
    "        gl_FragColor = vec4(tex, s_alpha);                \n"
    "    }                                                     \n"
    "    else {                                                \n"
    "        vec3 tex = texture2D(s_texture, v_texCoord).rgb;  \n"
    "        gl_FragColor = vec4(tex, 1.0);                    \n"
    "    }                                                     \n"
    "}                                                         \n";

static struct _cpu_clock cpu_clock[] = {
    {96, 0x80000110},
    {144, 0x80000120},
    {192, 0x80000130},
    {216, 0x80000220},
    {240, 0x80000410},
    {288, 0x80000230},
    {336, 0x80000610},
    {360, 0x80000420},
    {384, 0x80000330},
    {432, 0x80000520},
    {480, 0x80000430},
    {504, 0x80000620},
    {528, 0x80000a10},
    {576, 0x80000530},
    {624, 0x80000c10},
    {648, 0x80000820},
    {672, 0x80000630},
    {720, 0x80000920},
    {768, 0x80000730},
    {792, 0x80000a20},
    {816, 0x80001010},
    {864, 0x80000830},
    {864, 0x80001110},
    {912, 0x80001210},
    {936, 0x80000c20},
    {960, 0x80000930},
    {1008, 0x80000d20},
    {1056, 0x80000a30},
    {1080, 0x80000e20},
    {1104, 0x80001610},
    {1152, 0x80000b30},
    {1200, 0x80001810},
    {1224, 0x80001020},
    {1248, 0x80000c30},
    {1296, 0x80001120},
    {1344, 0x80000d30},
    {1368, 0x80001220},
    {1392, 0x80001c10},
    {1440, 0x80000e30},
    {1488, 0x80001e10},
    {1512, 0x80001420},
    {1536, 0x80000f30},
    {1584, 0x80001520},
    {1632, 0x80001030},
    {1656, 0x80001620},
    {1728, 0x80001130},
    {1800, 0x80001820},
    {1824, 0x80001230},
    {1872, 0x80001920},
    {1920, 0x80001330},
    {1944, 0x80001a20},
    {2016, 0x80001430},
    {2088, 0x80001c20},
    {2112, 0x80001530},
    {2160, 0x80001d20},
    {2208, 0x80001630},
    {2232, 0x80001e20},
    {2304, 0x80001730},
    {2400, 0x80001830},
    {2496, 0x80001930},
    {2592, 0x80001a30},
    {2688, 0x80001b30},
    {2784, 0x80001c30},
    {2880, 0x80001d30},
    {2976, 0x80001e30},
    {3072, 0x80001f30},
};

static int max_cpu_item = sizeof(cpu_clock) / sizeof(struct _cpu_clock);

static int get_core(int index)
{
    FILE *fd = NULL;
    char buf[255] = {0};

    sprintf(buf, "cat /sys/devices/system/cpu/cpu%d/online", index % 4);
    fd = popen(buf, "r");
    if (fd == NULL) {
        return -1;
    }
    fgets(buf, sizeof(buf), fd);
    pclose(fd);
    return atoi(buf);
}

static void check_before_set(int num, int v)
{
    char buf[255] = {0};

    if (get_core(num) != v) {
        sprintf(buf, "echo %d > /sys/devices/system/cpu/cpu%d/online", v ? 1 : 0, num);
        system(buf);
    }
}

static void set_core(int n)
{
    if (n <= 1) {
        printf(PREFIX"New CPU Core: 1\n");
        check_before_set(0, 1);
        check_before_set(1, 0);
        check_before_set(2, 0);
        check_before_set(3, 0);
    }
    else if (n == 2) {
        printf(PREFIX"New CPU Core: 2\n");
        check_before_set(0, 1);
        check_before_set(1, 1);
        check_before_set(2, 0);
        check_before_set(3, 0);
    }
    else if (n == 3) {
        printf(PREFIX"New CPU Core: 3\n");
        check_before_set(0, 1);
        check_before_set(1, 1);
        check_before_set(2, 1);
        check_before_set(3, 0);
    }
    else {
        printf(PREFIX"New CPU Core: 4\n");
        check_before_set(0, 1);
        check_before_set(1, 1);
        check_before_set(2, 1);
        check_before_set(3, 1);
    }
}

static int set_best_match_cpu_clock(int clk)
{
    int cc = 0;

    system("echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    for (cc = 0; cc < max_cpu_item; cc++) {
        if (cpu_clock[cc].clk >= clk) {
            printf(PREFIX"Set Best Match CPU %dMHz (0x%08x)\n", cpu_clock[cc].clk, cpu_clock[cc].reg);
            *vid.cpu_ptr = cpu_clock[cc].reg;
            return cc;
        }
    }
    return -1;
}

static int fb_flip(void)
{
    if (vid.vsurf == NULL) {
        return -1;
    }

#if 0
    glUniform1f(vid.alphaLoc, 1.0 - ((float)nds.alpha.val / 10.0));
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
#endif
    glBindTexture(GL_TEXTURE_2D, vid.texID[TEX_SCR]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if ((vid.vsurf->pitch / vid.vsurf->w) == 2) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, vid.vsurf->w, vid.vsurf->h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, vid.vsurf->pixels);
    }
    else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vid.vsurf->w, vid.vsurf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, vid.vsurf->pixels);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vid.texID[TEX_SCR]);
    glVertexAttribPointer(vid.posLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vVertices);
    glVertexAttribPointer(vid.texLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &vVertices[3]);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

    glUniform1f(vid.alphaLoc, 0.0);
    glDisable(GL_BLEND);
    eglSwapBuffers(vid.eglDisplay, vid.eglSurface);

    vid.fb_flip ^= 1;
    vid.vsurf->pixels = vid.fb_mem[vid.fb_flip];
    return 0;
}

static int A30_Available(void)
{
    return 1;
}

static void A30_DeleteDevice(SDL_VideoDevice *device)
{
    if(device){
        SDL_free(device);
    }
}

static int A30_HWAccelBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
    return 0;
}

static int A30_CheckHWBlit(_THIS, SDL_Surface *src, SDL_Surface *dst)
{
    src->map->hw_blit = A30_HWAccelBlit;
    return 1;
}

static SDL_Rect **A30_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
    return (SDL_Rect **)-1;
}

static int A30_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
    EGLint egl_major = 0;
    EGLint egl_minor = 0;
    EGLint num_configs = 0;
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLint window_attributes[] = {
        EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
        EGL_NONE
    };
    EGLint const context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };

    vid.mem_fd = open("/dev/mem", O_RDWR);
    if (vid.mem_fd < 0) {
        printf("Failed to open /dev/mem\n");
        return -1;
    }

    vid.ccu_mem = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, vid.mem_fd, CCU_BASE);
    if (vid.ccu_mem == MAP_FAILED) {
        printf("Failed to map memory\n");
        return -1;
    }

    printf(PREFIX"CCU MMap %p\n", vid.ccu_mem);
    vid.cpu_ptr = (uint32_t *)&vid.ccu_mem[0x00];

    set_best_match_cpu_clock(INIT_CPU_CLOCK);
    set_core(INIT_CPU_CORE);

    vid.eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(vid.eglDisplay, &egl_major, &egl_minor);
    eglChooseConfig(vid.eglDisplay, config_attribs, &vid.eglConfig, 1, &num_configs);
    vid.eglSurface = eglCreateWindowSurface(vid.eglDisplay, vid.eglConfig, 0, window_attributes);
    vid.eglContext = eglCreateContext(vid.eglDisplay, vid.eglConfig, EGL_NO_CONTEXT, context_attributes);
    eglMakeCurrent(vid.eglDisplay, vid.eglSurface, vid.eglSurface, vid.eglContext);

    vid.vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vid.vShader, 1, &vShaderSrc, NULL);
    glCompileShader(vid.vShader);

    vid.fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vid.fShader, 1, &fShaderSrc, NULL);
    glCompileShader(vid.fShader);

    vid.pObject = glCreateProgram();
    glAttachShader(vid.pObject, vid.vShader);
    glAttachShader(vid.pObject, vid.fShader);
    glLinkProgram(vid.pObject);
    glUseProgram(vid.pObject);

    eglSwapInterval(vid.eglDisplay, 1);
    vid.posLoc = glGetAttribLocation(vid.pObject, "a_position");
    vid.texLoc = glGetAttribLocation(vid.pObject, "a_texCoord");
    vid.samLoc = glGetUniformLocation(vid.pObject, "s_texture");
    vid.alphaLoc = glGetUniformLocation(vid.pObject, "s_alpha");

    glGenTextures(TEX_MAX, vid.texID);

    glViewport(0, 0, LCD_H, LCD_W);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnableVertexAttribArray(vid.posLoc);
    glEnableVertexAttribArray(vid.texLoc);
    glUniform1i(vid.samLoc, 0);
    glUniform1f(vid.alphaLoc, 0.0);

    vid.fb_mem[0] = malloc(LCD_W * LCD_H * 4);
    vid.fb_mem[1] = malloc(LCD_W * LCD_H * 4);

    A30_EventInit();
    return 0;
}

static void A30_VideoQuit(_THIS)
{
    set_best_match_cpu_clock(DEINIT_CPU_CLOCK);
    set_core(DEINIT_CPU_CORE);
    system("echo ondemand > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");

    A30_EventDeinit();

    glDeleteTextures(TEX_MAX, vid.texID);
    eglMakeCurrent(vid.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(vid.eglDisplay, vid.eglContext);
    eglDestroySurface(vid.eglDisplay, vid.eglSurface);
    eglTerminate(vid.eglDisplay);

    free(vid.fb_mem[0]);
    free(vid.fb_mem[1]);
    vid.fb_mem[0] = NULL;
    vid.fb_mem[1] = NULL;

    if (vid.ccu_mem != MAP_FAILED) {
        munmap(vid.ccu_mem, 4096);
        vid.ccu_mem = NULL;
    }

    if (vid.mem_fd > 0) {
        close(vid.mem_fd);
        vid.mem_fd = -1;
    }
}

static SDL_Surface *A30_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags)
{
    if(!SDL_ReallocFormat(current, bpp, 0, 0, 0, 0)) {
        SDL_SetError("Failed to allocate new pixel format for requested mode");
        return NULL;
    }

    vid.vsurf = current;
    current->flags = flags | SDL_DOUBLEBUF | SDL_PREALLOC;
    current->w = width;
    current->h = height;
    current->pitch = width * (bpp / 8);
    current->pixels = vid.fb_mem[0];
    printf(PREFIX"Width:%d, Height:%d, BPP:%d\n", width, height, bpp);
    return current;
}

static int A30_AllocHWSurface(_THIS, SDL_Surface *surface)
{
    return 0;
}

static void A30_FreeHWSurface(_THIS, SDL_Surface *surface)
{
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
    if (vid.vsurf) {
        fb_flip();
    }
    return 0;
}

static void A30_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
    if (vid.vsurf) {
        fb_flip();
    }
}

static int A30_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
    return 0;
}

static SDL_VideoDevice *A30_CreateDevice(int devindex)
{
    SDL_VideoDevice *device = NULL;

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

