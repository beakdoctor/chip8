#include <stdio.h>
#include <SDL2/SDL.h>
#include "chip8.h"

int main(int argc, char **argv) {
	SDL_Event event;

	SDL_Init(SDL_INIT_VIDEO);
 
	SDL_Window *window = SDL_CreateWindow("SDL2 Pixel Drawing",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
 
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
	SDL_Texture *texture = SDL_CreateTexture(renderer,
			SDL_PIXELFORMAT_ARGB8888,
			SDL_TEXTUREACCESS_STATIC,
			CHIP8_SCREEN_WIDTH, CHIP8_SCREEN_HEIGHT);

	Uint32 pixels[CHIP8_SCREEN_WIDTH * CHIP8_SCREEN_HEIGHT];
	memset(pixels, 255, sizeof(pixels));

	struct chip8 state;
	init_state(&state);
	load_rom(&state, argv[1]);

	uint64_t cycles = 0;
	// Used for the sound and delay timers.
	uint32_t last_event = SDL_GetTicks();
	bool quit = false;	
	while(!quit) {
		if (state.wait_for_input) {
			while (SDL_WaitEvent(&event)) {
				switch (event.type) {
					case SDL_KEYDOWN:
						{
							SDL_Keysym key = event.key.keysym;
							// TODO: set inputs
							state.wait_for_input = false;
						}
						break;
				}
				if (!state.wait_for_input) {
				   break;
				}	   
			}
		}	
		while (SDL_PollEvent(&event)) {
			switch (event.type)
			{
				case SDL_QUIT:
					quit = true;
					break;
				case SDL_KEYDOWN:
					{
						SDL_Keysym key = event.key.keysym;
						if (key.sym == SDLK_q) {
							quit = true;
						}
					}
					break;
			}
		}

		// Handle timers
		uint32_t ticks = SDL_GetTicks();
		if (ticks - last_event > 1000/60) {
			cycles = 0;
			if (state.st > 0) state.st--;
			if (state.dt > 0) state.dt--;
			last_event = ticks;
		}

		emulate_cycle(&state);

		cycles++;

		if (state.update_screen) {
			SDL_RenderClear(renderer);
			for (int i  = 0; i < sizeof(state.gfx) / sizeof(state.gfx[0]); i++) {
				pixels[i] = state.gfx[i] ? 0xff00ff00 : 0xff000000;
			}
			SDL_UpdateTexture(texture, NULL, pixels, CHIP8_SCREEN_WIDTH * sizeof(Uint32));
			SDL_RenderCopy(renderer, texture, NULL, NULL);
			SDL_RenderPresent(renderer);	
			state.update_screen = false;
		}
	}

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
