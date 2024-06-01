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

#define UP      103
#define DOWN    108
#define LEFT    105
#define RIGHT   106
#define A       57
#define B       29
#define X       42
#define Y       56
#define L1      15
#define L2      18
#define R1      14
#define R2      20
#define SELECT  97
#define START   28
#define MENU    1
#define VOLUP   115
#define VOLDOWN 114

const int code[] = {
    SDLK_UP,            // UP
    SDLK_DOWN,          // DOWN
    SDLK_LEFT,          // LEFT
    SDLK_RIGHT,         // RIGHT
    SDLK_SPACE,         // A
    SDLK_LCTRL,         // B
    SDLK_LSHIFT,        // X
    SDLK_LALT,          // Y
    SDLK_t,             // L1
    SDLK_e,             // R1
    SDLK_TAB,           // L2
    SDLK_BACKSPACE,     // R2
    SDLK_RCTRL,         // SELECT
    SDLK_RETURN,        // START
    SDLK_ESCAPE,        // MENU
};

static int running = 0;
static int event_fd = -1;
static SDL_sem *event_sem = NULL;
static SDL_Thread *thread = NULL;
static uint32_t pre_keypad_bitmaps = 0;
static uint32_t cur_keypad_bitmaps = 0;

void A30_PumpEvents(_THIS)
{
    SDL_keysym keysym;

    SDL_SemWait(event_sem);
    if (pre_keypad_bitmaps != cur_keypad_bitmaps) {
        uint32_t cc = 0;
        uint32_t bit = 0;
        uint32_t changed = pre_keypad_bitmaps ^ cur_keypad_bitmaps;

        for (cc=0; cc<=MYKEY_LAST_BITS; cc++) {
            bit = 1 << cc;
            if (changed & bit) {
                keysym.scancode = cc;
                keysym.sym = code[cc];
                SDL_PrivateKeyboard((cur_keypad_bitmaps & bit) ? SDL_PRESSED : SDL_RELEASED, &keysym);
            }
        }
        pre_keypad_bitmaps = cur_keypad_bitmaps;
    }
    SDL_SemPost(event_sem);
}

void A30_InitOSKeymap(_THIS)
{
}

static void set_key(uint32_t bit, int val)
{
    if (val) {
        cur_keypad_bitmaps |= (1 << bit);
    }
    else {
        cur_keypad_bitmaps &= ~(1 << bit);
    }
}

int EventUpdate(void *data)
{
    struct input_event ev = {0};

    uint32_t l1 = L1;
    uint32_t r1 = R1;
    uint32_t l2 = L2;
    uint32_t r2 = R2;

    uint32_t a = A;
    uint32_t b = B;
    uint32_t x = X;
    uint32_t y = Y;

    uint32_t up = UP;
    uint32_t down = DOWN;
    uint32_t left = LEFT;
    uint32_t right = RIGHT;

    while (running) {
        SDL_SemWait(event_sem);

        up = UP;
        down = DOWN;
        left = LEFT;
        right = RIGHT;

        a = A;
        b = B;
        x = X;
        y = Y;

        l1 = L1;
        l2 = L2;
        r1 = R1;
        r2 = R2;

        if (event_fd > 0) {
            if (read(event_fd, &ev, sizeof(struct input_event))) {
                if ((ev.type == EV_KEY) && (ev.value != 2)) {
                    //printf("code:%d, value:%d\n", ev.code, ev.value);
                    if (ev.code == l1)      { set_key(MYKEY_L1,    ev.value); }
                    if (ev.code == r1)      { set_key(MYKEY_R1,    ev.value); }
                    if (ev.code == l2)      { set_key(MYKEY_L2,    ev.value); }
                    if (ev.code == r2)      { set_key(MYKEY_R2,    ev.value); }
                    if (ev.code == up)      { set_key(MYKEY_UP,    ev.value); }
                    if (ev.code == down)    { set_key(MYKEY_DOWN,  ev.value); }
                    if (ev.code == left)    { set_key(MYKEY_LEFT,  ev.value); }
                    if (ev.code == right)   { set_key(MYKEY_RIGHT, ev.value); }
                    if (ev.code == a)       { set_key(MYKEY_A,     ev.value); }
                    if (ev.code == b)       { set_key(MYKEY_B,     ev.value); }
                    if (ev.code == x)       { set_key(MYKEY_X,     ev.value); }
                    if (ev.code == y)       { set_key(MYKEY_Y,     ev.value); }

                    switch (ev.code) {
                    case START:   set_key(MYKEY_START, ev.value);   break;
                    case SELECT:  set_key(MYKEY_SELECT, ev.value);  break;
                    case MENU:    set_key(MYKEY_MENU, ev.value);    break;
                    case VOLUP:   set_key(MYKEY_VOLUP, ev.value);   break;
                    case VOLDOWN: set_key(MYKEY_VOLDOWN, ev.value); break;
                    }
                }
            }
        }
        SDL_SemPost(event_sem);
        usleep(1000000 / 60);
    }

    return 0;
}

void A30_EventInit(void)
{
    pre_keypad_bitmaps = 0;
    event_fd = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if(event_fd < 0){
        printf(PREFIX"Failed to open /dev/input/event3\n");
    }

    event_sem = SDL_CreateSemaphore(1);
    if(event_sem == NULL) {
        printf(PREFIX"Failed to create input semaphore");
    }

    if (event_sem != NULL) {
        running = 1;
        if((thread = SDL_CreateThread(EventUpdate, NULL)) == NULL) {
            printf(PREFIX"Failed to create input thread");
        }
    }
}

void A30_EventDeinit(void)
{
    running = 0;
    SDL_WaitThread(thread, NULL);
    SDL_DestroySemaphore(event_sem);
    if(event_fd > 0) {
        close(event_fd);
        event_fd = -1;
    }
}

