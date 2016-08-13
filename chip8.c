#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "chip8.h"

#define LOG(...) fprintf(stderr, __VA_ARGS__)

uint8_t fontset[80] =
{ 
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};


void init_state(struct chip8 *state) {
	srand(time(NULL));

	state->pc = 0x200;
	state->i = 0;
	state->sp = 0;

	for (int i = 0; i < CHIP8_GFX_SIZE; i++) {
		state->gfx[i] = 0;
	}
	for (int i = 0; i < CHIP8_STACK_SIZE; i++) {
		state->stack[i] = 0;
	}
	for (int i = 0; i < CHIP8_NUM_REGS; i++) {
		state->v[i] = 0;
	}
	for (int i = 0; i < CHIP8_MEM_SIZE; i++) {
		state->mem[i] = 0;
	}

	for (int i = 0; i < 80; i++) {
		state->mem[i] = fontset[i];
	}
}

void load_rom(struct chip8 *state, char *path) {
	FILE *rom;
	if ((rom = fopen(path, "rb")) == NULL) {
		LOG("Cannot open %s: %s\n", path, strerror(errno));
		exit(1);
	}
	size_t bytes_read = fread(state->mem + 0x200, sizeof(state->mem[0]), CHIP8_MEM_SIZE - 1 - 0x200, rom);
	LOG("Read %zu bytes of data\n", bytes_read);
}

void emulate_cycle(struct chip8 *state) {
	uint16_t opcode = state->mem[state->pc] << 8 | state->mem[state->pc + 1];
	switch(opcode & 0xf000) {
		case 0x0000: 
			if (opcode == 0x00e0) {
				LOG("CLS\n");
				for (int i = 0; i < CHIP8_GFX_SIZE; i++) {
					state->gfx[i] = 0;
				}
			}
			break;
		case 0x6000:
			// Set Vx = kk.
			// The interpreter puts the value kk into register Vx.
			LOG("LD Vx, byte\n");
			uint8_t x = opcode & 0x0f00;
			state->v[x] = opcode & 0x00ff;
			break;
		case 0xa000:
			LOG("LD I, addr\n");
			state->i = opcode & 0x0fff;
			break;
		case 0xc000: 
			{
				LOG("RND Vx, byte\n");
				uint8_t r = rand() & (opcode & 0xff);
				state->v[opcode & 0x0f00] = r;
				break;
			}
		case 0xf000:
			{
				uint8_t x = opcode & 0x0f00;
				if ((opcode & 0x00ff) == 0x29) {
					// Set I = location of sprite for digit Vx.
					// The value of I is set to the location for the hexadecimal sprite corresponding to the value of Vx.
					LOG("LD F, Vx\n");
					uint8_t vx = state->v[opcode & 0x0f00];
					state->i = vx * 5;
					break;
				} else if ((opcode & 0x00ff) == 0x33) {
					// Store BCD representation of Vx in memory locations I, I+1, and I+2.
					// The interpreter takes the decimal value of Vx, and places the hundreds 
					// digit in memory at location in I, the tens digit at location I+1, 
					// and the ones digit at location I+2.	
					LOG("LD B, Vx\n");
					uint8_t vx = state->v[x];
					uint8_t hundreds = vx / 100 % 100;
					uint8_t tens = vx % 100 / 10;
					uint8_t ones = vx % 100 % 10;
					uint16_t i = state->i;
					state->mem[i] = hundreds;
					state->mem[i + 1] = tens;
					state->mem[i + 2] = ones;

					break;
				} else if ((opcode & 0x00ff) == 0x65) {
					// Read registers V0 through Vx from memory starting at location I.
					// The interpreter reads values from memory starting at location I into registers V0 through Vx.	
					LOG("LD Vx, [I]\n");
					uint8_t x = opcode & 0x0f00;
					for (int i = 0; i <= x; i++) {
						state->v[i] = state->mem[state->i + i];
					}
					break;
				}
			}
		default:
			LOG("Unknown opcode: 0x%x\n", opcode);
			print_state(state);
			exit(1);
	}
	state->pc += 2;
}

void print_state(struct chip8 *state) {
	LOG("pc: 0x%x\n", state->pc);
	LOG("i: 0x%x\n", state->i);
	LOG("sp: 0x%x\n", state->sp);
	for (int i = 0; i < CHIP8_NUM_REGS; i++) {
		LOG("v%d: 0x%x\n", i, state->v[i]);
	}
	for (int i = 0; i < CHIP8_STACK_SIZE; i++) {
		LOG("stack[%d]: 0x%x\n", i, state->stack[i]);
	}
}
