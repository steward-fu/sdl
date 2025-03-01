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
#include "SDL_xt894_video.h"
#include "SDL_xt894_event.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"

#if 0
    #define debug(...) printf(__VA_ARGS__)
#else
    #define debug(...) (void)0
#endif

uint8_t mykey[KEY_MAX][2] = {0};
static int mymap[KEY_MAX] = {0};

void XT894_PumpEvents(_THIS)
{
    int c0 = 0;
    SDL_keysym keysym;

    keysym.mod = KMOD_NONE;
    for (c0 = 0; c0 < KEY_MAX; c0++) {
        keysym.scancode = c0;
        keysym.sym = mymap[c0];

        if (mykey[c0][1] && mymap[c0]) {
            mykey[c0][1] = 0;
            debug("Key Pressed: %d\n", mymap[c0]);
            SDL_PrivateKeyboard(SDL_PRESSED, &keysym);
        }
        if (mykey[c0][0] && mymap[c0]) {
            mykey[c0][0] = 0;
            debug("Key Released: %d\n", mymap[c0]);
            SDL_PrivateKeyboard(SDL_RELEASED, &keysym);
        }
    }
}

void XT894_InitOSKeymap(_THIS)
{
    mymap[KEY_0]          = SDLK_0;
    mymap[KEY_1]          = SDLK_1;
    mymap[KEY_2]          = SDLK_2;
    mymap[KEY_3]          = SDLK_3;
    mymap[KEY_4]          = SDLK_4;
    mymap[KEY_5]          = SDLK_5;
    mymap[KEY_6]          = SDLK_6;
    mymap[KEY_7]          = SDLK_7;
    mymap[KEY_8]          = SDLK_8;
    mymap[KEY_9]          = SDLK_9;

    mymap[KEY_UP]         = SDLK_UP;
    mymap[KEY_DOWN]       = SDLK_DOWN;
    mymap[KEY_LEFT]       = SDLK_LEFT;
    mymap[KEY_RIGHT]      = SDLK_RIGHT;
    
    mymap[KEY_A]          = SDLK_a;
    mymap[KEY_B]          = SDLK_b;
    mymap[KEY_C]          = SDLK_c;
    mymap[KEY_D]          = SDLK_d;
    mymap[KEY_E]          = SDLK_e;
    mymap[KEY_F]          = SDLK_f;
    mymap[KEY_G]          = SDLK_g;
    mymap[KEY_H]          = SDLK_h;
    mymap[KEY_I]          = SDLK_i;
    mymap[KEY_J]          = SDLK_j;
    mymap[KEY_K]          = SDLK_k;
    mymap[KEY_L]          = SDLK_l;
    mymap[KEY_M]          = SDLK_m;
    mymap[KEY_N]          = SDLK_n;
    mymap[KEY_O]          = SDLK_o;
    mymap[KEY_P]          = SDLK_p;
    mymap[KEY_Q]          = SDLK_q;
    mymap[KEY_R]          = SDLK_r;
    mymap[KEY_S]          = SDLK_s;
    mymap[KEY_T]          = SDLK_t;
    mymap[KEY_U]          = SDLK_u;
    mymap[KEY_V]          = SDLK_v;
    mymap[KEY_W]          = SDLK_w;
    mymap[KEY_X]          = SDLK_x;
    mymap[KEY_Y]          = SDLK_y;
    mymap[KEY_Z]          = SDLK_z;

    mymap[KEY_ESC]        = SDLK_ESCAPE;
    mymap[KEY_SPACE]      = SDLK_SPACE;
    mymap[KEY_CAPSLOCK]   = SDLK_CAPSLOCK;
    mymap[KEY_BACKSPACE]  = SDLK_BACKSPACE;
    mymap[KEY_TAB]        = SDLK_TAB;
    mymap[KEY_LEFTMETA]   = SDLK_LMETA;
    mymap[KEY_GRAVE]      = SDLK_BACKQUOTE;
    mymap[KEY_COMMA]      = SDLK_COMMA;
    mymap[KEY_DOT]        = SDLK_PERIOD;
    mymap[KEY_APOSTROPHE] = SDLK_QUOTEDBL;
    mymap[KEY_LEFTBRACE]  = SDLK_LEFTBRACKET;
    mymap[KEY_RIGHTBRACE] = SDLK_RIGHTBRACKET;
    mymap[KEY_ENTER]      = SDLK_RETURN;
    mymap[KEY_MINUS]      = SDLK_MINUS;
    mymap[KEY_EQUAL]      = SDLK_EQUALS;
    mymap[KEY_SLASH]      = SDLK_SLASH;
    mymap[KEY_BACKSLASH]  = SDLK_BACKSLASH;

    mymap[KEY_F1]         = SDLK_F1;
    mymap[KEY_F2]         = SDLK_F2;
    mymap[KEY_F3]         = SDLK_F3;
    mymap[KEY_F4]         = SDLK_F4;
    mymap[KEY_F5]         = SDLK_F5;

    mymap[KEY_RIGHTSHIFT] = SDLK_RSHIFT;
    mymap[KEY_LEFTSHIFT]  = SDLK_LSHIFT;
    mymap[KEY_RIGHTCTRL]  = SDLK_RCTRL;
    mymap[KEY_LEFTCTRL]   = SDLK_LCTRL;
    mymap[KEY_RIGHTALT]   = SDLK_RALT;
    mymap[KEY_LEFTALT]    = SDLK_LALT;
}

