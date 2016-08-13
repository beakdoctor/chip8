#include <errno.h>
#include <stdbool.h>
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

	state->update_screen = true;
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
				// 00E0 - CLS Clear the display..
				LOG("0x%x: CLS\n", opcode);
				for (int i = 0; i < CHIP8_GFX_SIZE; i++) {
					state->gfx[i] = 0;
				}
				state->update_screen = true;
			}
			break;
		case 0x1000:
			{
				// 1nnn - JP addr - Jump to location nnn.
				// The interpreter sets the program counter to nnn.
				uint16_t addr = opcode & 0x0fff;
				LOG("0x%x: JP 0x%x\n", opcode, addr);
				state->pc = addr;
				goto end_cycle;
			}
		case 0x6000:
			{
				// 6xkk - LD Vx, byte - Set Vx = kk.
				// The interpreter puts the value kk into register Vx.
				uint8_t x = (opcode & 0x0f00) >> 8;
				uint8_t byte = opcode & 0x00ff;
				LOG("0x%x: LD V%d, 0x%x\n", opcode, x, byte);
				state->v[x] = byte;
				break;
			}
		case 0xa000:
			{
				// Annn - LD I, addr - Set I = nnn.
				// The value of register I is set to nnn.
				uint16_t addr = opcode & 0x0fff;
				LOG("0x%x: LD I, 0x%x\n", opcode, addr);
				state->i = addr;
				break;
			}
		case 0xd000:
			{
				// Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision.
				//
				// The interpreter reads n bytes from memory, starting at the address stored in I.
				// These bytes are then displayed as sprites on screen at coordinates (Vx, Vy). 
				// Sprites are XORed onto the existing screen. If this causes any pixels to be erased, 
				// VF is set to 1, otherwise it is set to 0. If the sprite is positioned so part of it
				// is outside the coordinates of the display, it wraps around to the opposite side of the screen.
				uint8_t x = (opcode & 0x0f00) >> 8;
				uint8_t y = (opcode & 0x00f0) >> 4;
				uint8_t n = opcode & 0x000f;
				LOG("0x%x: DRW V%d, V%d, %d\n", opcode, x, y, n);
				state->update_screen = true;
				break;
			}
		case 0xc000: 
			{
				// Cxkk - RND Vx, byte - Set Vx = random byte AND kk.
				// The interpreter generates a random number from 0 to 255, which is then ANDed with the value kk.
				// The results are stored in Vx.
				uint8_t x = (opcode & 0x0f00) >> 8;
				uint8_t byte = opcode & 0x00ff;
				LOG("0x%x: RND V%d, 0x%x\n", opcode, x, byte);
				uint8_t r = rand() & byte;
				state->v[x] = r;
				break;
			}
		case 0xf000:
			{
				uint8_t x = opcode & 0x0f00;
				if ((opcode & 0x00ff) == 0x0a) {
					// Fx0A - LD Vx, K - Wait for a key press, store the value of the key in Vx.
					// All execution stops until a key is pressed, then the value of that key is stored in Vx.	
					uint8_t x = (opcode & 0x0f00) >> 8;
					LOG("0x%x: LD V%d, K\n", opcode, x);
					break;
				} else if ((opcode & 0x00ff) == 0x29) {
					// Fx29 - LD F, Vx - Set I = location of sprite for digit Vx.
					// The value of I is set to the location for the hexadecimal sprite corresponding to the value of Vx.
					uint8_t x = (opcode & 0x0f00) >> 8;
					LOG("0x%x: LD F, V%d\n", opcode, x);
					state->i = state->v[x] * 5;
					break;
				} else if ((opcode & 0x00ff) == 0x33) {
					// Fx33 - LD B, Vx - Store BCD representation of Vx in memory locations I, I+1, and I+2.
					// The interpreter takes the decimal value of Vx, and places the hundreds 
					// digit in memory at location in I, the tens digit at location I+1, 
					// and the ones digit at location I+2.	
					uint8_t x = (opcode & 0x0f00) >> 8;
					LOG("0x%x: LD B, V%d\n", opcode, x);
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
					// Fx65 - LD Vx, [I] - Read registers V0 through Vx from memory starting at location I.
					// The interpreter reads values from memory starting at location I into registers V0 through Vx.	
					uint8_t x = (opcode & 0x0f00) >> 8;
					LOG("0x%x: LD V%d, [I]\n", opcode, x);
					for (int i = 0; i <= state->v[x]; i++) {
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
end_cycle:
	return;
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
