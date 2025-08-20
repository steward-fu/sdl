#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <SDL.h>
#include <SDL_image.h>
 
int main(int argc, char **argv)
{
    SDL_Surface *screen = NULL;

    if (argc != 3) {
        printf("Usage:\n  %s xxx.png xxx.h\n", argv[0]);
        exit(0);
    }
 
    SDL_Init(SDL_INIT_VIDEO);
    screen = SDL_SetVideoMode(960, 540, 16, SDL_SWSURFACE);
 
    SDL_Surface *png = IMG_Load(argv[1]);
    SDL_BlitSurface(png, NULL, screen, NULL);
    SDL_Flip(screen);
    SDL_Delay(3000);

    int x = 0;
    int y = 0;
    uint16_t *p = (uint16_t *)png->pixels;
    FILE *fp = fopen(argv[2], "w");

    fprintf(fp, "uint16_t hex_border[] = {\n");
    for (y = 0; y < 540; y++) {
        for (x = 0; x < 960; x++) {
            fprintf(fp, "0x%x, ", *p++);
        }
    }
    fprintf(fp, "};\n");
    fclose(fp);
 
    SDL_FreeSurface(png);
    SDL_Quit();
    return 0;
}
