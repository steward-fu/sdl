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

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/input.h>

#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "../../video/a30/SDL_a30_video.h"
#include "../../video/a30/SDL_a30_event.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"

#define SERIAL_GAMEDECK         "/dev/ttyS0"
#define MIYOO_AXIS_MAX_COUNT    16
#define MIYOO_PLAYER_MAGIC      0xFF
#define MIYOO_PLAYER_MAGIC_END  0xFE
#define MIYOO_PAD_FRAME_LEN     6
#define MYJOY_MODE_KEYPAD       0
#define MYJOY_MODE_MOUSE        1
#define MYJOY_MODE_JOYPAD       2

struct MIYOO_PAD_FRAME {
    uint8_t magic;
    uint8_t unused0;
    uint8_t unused1;
    uint8_t axis0;
    uint8_t axis1;
    uint8_t magicEnd;
};

static int max_x = 200;
static int zero_x = 135;
static int min_x = 75;

static int max_y = 200;
static int zero_y = 135;
static int min_y = 75;

static int mode = MYJOY_MODE_KEYPAD;
static int dzone = 65;

static int g_lastX = 0;
static int g_lastY = 0;

static int s_fd = -1;
static struct MIYOO_PAD_FRAME s_frame = {0};
static int32_t s_miyoo_axis[MIYOO_AXIS_MAX_COUNT] = {0};
static int32_t s_miyoo_axis_last[MIYOO_AXIS_MAX_COUNT] = {0};

static int running = 0;
static pthread_t thread_id = 0;

static int uart_open(const char *port)
{
    int fd = -1;

    fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (-1 == fd) {
        printf("Failed to open uart\n");
        return -1;
    }

    if (fcntl(fd, F_SETFL, 0) < 0) {
        printf("Failed to call fcntl\n");
        return -1;
    }
    return fd;
}

static void uart_close(int fd)
{
    close(fd);
}

static int uart_set(int fd, int speed, int flow_ctrl, int databits, int stopbits, int parity)
{
    int i = 0;
    int speed_arr[] = {B115200, B19200, B9600, B4800, B2400, B1200, B300};
    int name_arr[] = {115200, 19200, 9600, 4800, 2400, 1200, 300};
    struct termios options = {0};

    if (tcgetattr(fd, &options) != 0) {
        printf("Failed to get uart attributes\n");
        return -1;
    }

    for (i = 0; i < sizeof(speed_arr) / sizeof(int); i++) {
        if (speed == name_arr[i]) {
            cfsetispeed(&options, speed_arr[i]);
            cfsetospeed(&options, speed_arr[i]);
        }
    }

    options.c_cflag |= CLOCAL;
    options.c_cflag |= CREAD;
    switch (flow_ctrl) {
    case 0:
        options.c_cflag &= ~CRTSCTS;
        break;
    case 1:
        options.c_cflag |= CRTSCTS;
        break;
    case 2:
        options.c_cflag |= IXON | IXOFF | IXANY;
        break;
    }

    options.c_cflag &= ~CSIZE;
    switch (databits) {
    case 5:
        options.c_cflag |= CS5;
        break;
    case 6:
        options.c_cflag |= CS6;
        break;
    case 7:
        options.c_cflag |= CS7;
        break;
    case 8:
        options.c_cflag |= CS8;
        break;
    default:
        return -1;
    }

    switch (parity) {
    case 'n':
    case 'N':
        options.c_cflag &= ~PARENB;
        options.c_iflag &= ~INPCK;
        break;
    case 'o':
    case 'O':
        options.c_cflag |= (PARODD | PARENB);
        options.c_iflag |= INPCK;
        break;
    case 'e':
    case 'E':
        options.c_cflag |= PARENB;
        options.c_cflag &= ~PARODD;
        options.c_iflag |= INPCK;
        break;
    case 's':
    case 'S':
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        break;
    default:
        return -1;
    }

    switch (stopbits) {
    case 1:
        options.c_cflag &= ~CSTOPB;
        break;
    case 2:
        options.c_cflag |= CSTOPB;
        break;
    default:
        return -1;
    }

    options.c_oflag &= ~OPOST;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(INLCR | ICRNL | IGNCR);
    options.c_oflag &= ~(ONLCR | OCRNL);
    options.c_cc[VTIME] = 1;
    options.c_cc[VMIN] = 1;

    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        return -1;
    }
    return 0;
}

static int uart_init(int fd, int speed, int flow_ctrl, int databits, int stopbits, int parity)
{
    if (uart_set(fd, speed, flow_ctrl, databits, stopbits, parity) == -1) {
        return -1;
    }
    return 0;
}

static int uart_read(int fd, char *rcv_buf, int data_len)
{
    int f_sel;
    fd_set f_read;
    struct timeval time = {0};

    FD_ZERO(&f_read);
    FD_SET(fd, &f_read);

    time.tv_sec = 10;
    time.tv_usec = 0;
    f_sel = select(fd + 1, &f_read, NULL, NULL, &time);
    if (f_sel) {
        return read(fd, rcv_buf, data_len);
    }
    return 0;
}

static int filterDeadzone(int newAxis, int oldAxis)
{
    if (abs(newAxis - oldAxis) < dzone) {
        return 1;
    }
    return 0;
}

static int limitValue8(int value)
{
    if (value > 127) {
        value = 127;
    }
    else if (value < -128) {
        value = -128;
    }
    return value;
}

static void check_axis_event(void)
{
    int i = 0;

    for (i = 0; i < MIYOO_AXIS_MAX_COUNT; i++) {
        if (s_miyoo_axis[i] != s_miyoo_axis_last[i]) {
            if (!filterDeadzone(s_miyoo_axis[i], s_miyoo_axis_last[i])) {
                if (i == 0) {
                    g_lastX = limitValue8(s_miyoo_axis[i]);
                    //printf("X %d\n", g_lastX);
                }
                else if (i == 1) {
                    g_lastY = limitValue8(s_miyoo_axis[i]);
                    //printf("Y %d\n", g_lastY);
                }
            }
        }
        s_miyoo_axis_last[i] = s_miyoo_axis[i];
    }
}

static int miyoo_frame_to_axis_x(uint8_t rawX)
{
    int value = 0;

    if (rawX > zero_x) {
        value = (rawX - zero_x) * 126 / (max_x - zero_x);
    }

    if (rawX < zero_x) {
        value = (rawX - zero_x) * 126 / (zero_x - min_x);
    }

    if (value > 0 && value < dzone) {
        return 0;
    }

    if (value < 0 && value > -dzone) {
        return 0;
    }
    return value;
}

static int miyoo_frame_to_axis_y(uint8_t rawY)
{
    int value = 0;

    if (rawY > zero_y) {
        value = (rawY - zero_y) * 126 / (max_y - zero_y);
    }

    if (rawY < zero_y) {
        value = (rawY - zero_y) * 126 / (zero_y - min_y);
    }

    if (value > 0 && value < dzone) {
        return 0;
    }

    if (value < 0 && value > -dzone) {
        return 0;
    }
    return value;
}

static int parser_miyoo_input(const char *cmd, int len)
{
    int i = 0;
    int p = 0;

    if ((!cmd) || (len < MIYOO_PAD_FRAME_LEN)) {
        return - 1;
    }

    for (i = 0; i < len - MIYOO_PAD_FRAME_LEN + 1; i += MIYOO_PAD_FRAME_LEN) {
        for (p = 0; p < MIYOO_PAD_FRAME_LEN - 1; p++) {
            if ((cmd[i] == MIYOO_PLAYER_MAGIC) && (cmd[i + MIYOO_PAD_FRAME_LEN - 1] == MIYOO_PLAYER_MAGIC_END)) {
                memcpy(&s_frame, cmd + i, sizeof(s_frame));
                break;
            }
            else {
                i++;
            }
        }
    }
    s_miyoo_axis[ABS_X] = miyoo_frame_to_axis_x(s_frame.axis0);
    s_miyoo_axis[ABS_Y] = miyoo_frame_to_axis_y(s_frame.axis1);
    check_axis_event();
    return 0;
}

static int miyoo_init_serial_input(void)
{
    memset(&s_frame, 0, sizeof(s_frame));
    memset(s_miyoo_axis, 0, sizeof(s_miyoo_axis));
    memset(s_miyoo_axis_last, 0, sizeof(s_miyoo_axis_last));
    s_fd = uart_open(SERIAL_GAMEDECK);
    uart_init(s_fd, 9600, 0, 8, 1, 'N');
    if (s_fd <= 0) {
        return -1;
    }
    return 0;
}

static void miyoo_close_serial_input(void)
{
}

static int chk_str(const char *src, const char *dst)
{
    int len = strlen(dst);
    return (memcmp(src, dst, len) == 0) ? 1 : 0;
}

static int miyoo_read_joystick_config(void)
{
    FILE *f = NULL;
    char buf[255] = {0};
    const char *path = "/config/joypad.config";

    f = fopen(path, "r");
    if (f) {
        while (fgets(buf, sizeof(buf), f)) {
            if (chk_str(buf, "x_min=")) {
                printf(PREFIX"X_MIN %d\n", atoi(&buf[6]));
            }
            else if (chk_str(buf, "x_max=")) {
                printf(PREFIX"X_MAX %d\n", atoi(&buf[6]));
            }
            else if (chk_str(buf, "x_zero=")) {
                printf(PREFIX"X_ZERO %d\n", atoi(&buf[7]));
            }
            else if (chk_str(buf, "y_min=")) {
                printf(PREFIX"Y_MIN %d\n", atoi(&buf[6]));
            }
            else if (chk_str(buf, "y_max=")) {
                printf(PREFIX"Y_MAX %d\n", atoi(&buf[6]));
            }
            else if (chk_str(buf, "y_zero=")) {
                printf(PREFIX"Y_ZERO %d\n", atoi(&buf[7]));
            }
        }
        fclose(f);
    }
    return 0;
}

void* joystick_handler(void *param)
{
    int len = 0;
    char rcv_buf[255] = {0};

    while (running) {
        len = uart_read(s_fd, rcv_buf, 99);
        if (len > 0) {
            rcv_buf[len] = '\0';
            parser_miyoo_input(rcv_buf, len);
        }
        usleep(10000);
    }
    return NULL;
}

int SDL_SYS_JoystickInit(void)
{
    printf(PREFIX"%s\n", __func__);

    running = 1;
	SDL_numjoysticks = 1;
    miyoo_read_joystick_config();
    miyoo_init_serial_input();
    pthread_create(&thread_id, NULL, joystick_handler, NULL);
    return 1;
}

void SDL_SYS_JoystickQuit(void)
{
    printf(PREFIX"%s\n", __func__);

    running = 0;
    miyoo_close_serial_input();
    pthread_join(thread_id, NULL);
    uart_close(s_fd);
    s_fd = -1;
}

const char *SDL_SYS_JoystickName(int index)
{
	if (!index) {
		return "A30 Joystick";
    }
	SDL_SetError("No joystick available with that index");
	return NULL;
}

int SDL_SYS_JoystickOpen(SDL_Joystick *joystick)
{
	joystick->nbuttons = 8;
	joystick->naxes = 2;
	joystick->nhats = 0;
	joystick->nballs = 0;
	return 0;
}

void SDL_SYS_JoystickUpdate(SDL_Joystick *joystick)
{
    const int LTH = -50;
    const int RTH = 50;
    const int UTH = -50;
    const int DTH = 50;

    static int pre_x = -1;
    static int pre_y = -1;

    if (mode == MYJOY_MODE_KEYPAD) {
        static int pre_up = 0;
        static int pre_down = 0;
        static int pre_left = 0;
        static int pre_right = 0;

        uint32_t u_key = SDLK_UP;
        uint32_t d_key = SDLK_DOWN;
        uint32_t l_key = SDLK_LEFT;
        uint32_t r_key = SDLK_RIGHT;

        SDL_keysym keysym;
        keysym.scancode = 1; 
        if (g_lastX != pre_x) {
            pre_x = g_lastX;
            if (pre_x < LTH) {
                if (pre_left == 0) {
                    pre_left = 1;
                    keysym.sym = l_key;
                    SDL_PrivateKeyboard(SDL_PRESSED, &keysym);
                }
            }
            else if (pre_x > RTH){
                if (pre_right == 0) {
                    pre_right = 1;
                    keysym.sym = r_key;
                    SDL_PrivateKeyboard(SDL_PRESSED, &keysym);
                }
            }
            else {
                if (pre_left != 0) {
                    pre_left = 0;
                    keysym.sym = l_key;
                    SDL_PrivateKeyboard(SDL_RELEASED, &keysym);
                }
                if (pre_right != 0) {
                    pre_right = 0;
                    keysym.sym = r_key;
                    SDL_PrivateKeyboard(SDL_RELEASED, &keysym);
                }
            }
        }

        if (g_lastY != pre_y) {
            pre_y = g_lastY;
            if (pre_y < UTH) {
                if (pre_up == 0) {
                    pre_up = 1;
                    keysym.sym = u_key;
                    SDL_PrivateKeyboard(SDL_PRESSED, &keysym);
                }
            }
            else if (pre_y > DTH){
                if (pre_down == 0) {
                    pre_down = 1;
                    keysym.sym = d_key;
                    SDL_PrivateKeyboard(SDL_PRESSED, &keysym);
                }
            }
            else {
                if (pre_up != 0) {
                    pre_up = 0;
                    keysym.sym = u_key;
                    SDL_PrivateKeyboard(SDL_RELEASED, &keysym);
                }
                if (pre_down != 0) {
                    pre_down = 0;
                    keysym.sym = d_key;
                    SDL_PrivateKeyboard(SDL_RELEASED, &keysym);
                }
            }
        }
    }
    else if (mode == MYJOY_MODE_MOUSE) {
        const int XRES = 320;
        const int YRES = 240;
        const int INTV = 3;
        static int xx = XRES / 2;
        static int yy = YRES / 2;

        pre_x = g_lastX;
        if (pre_x < LTH) {
            if (xx > 0) {
                xx -= INTV;
            }
        }
        if (pre_x > RTH) {
            if (xx < XRES) {
                xx += INTV;
            }
        }
        pre_y = g_lastY;
        if (pre_y < UTH) {
            if (yy > 0) {
                yy -= INTV;
            }
        }
        if (pre_y > DTH) {
            if (yy < YRES) {
                yy += INTV;
            }
        }

        //if (win && (xx || yy)) {
            //SDL_SendMouseMotion(win, 0, 0, xx, yy);
        //}
    }
    else {
        if (g_lastX != pre_x) {
            pre_x = g_lastX;
            SDL_PrivateJoystickAxis(joystick, 0, pre_x);
        }

        if (g_lastY != pre_y) {
            pre_y = g_lastY;
            SDL_PrivateJoystickAxis(joystick, 1, pre_x);
        }
    }
}

void SDL_SYS_JoystickClose(SDL_Joystick *joystick)
{
}

