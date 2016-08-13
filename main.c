#include "chip8.h"

int main(int argc, char **argv) {
	struct chip8 state;
	init_state(&state);
	load_rom(&state, argv[1]);
	while(1) {
		emulate_cycle(&state);
	}
}
