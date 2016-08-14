#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "chip8.h"

#define DEBUG true
#define LOG(...) if (DEBUG) fprintf(stderr, __VA_ARGS__)

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
	state->st = 0;
	state->dt = 0;

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

	state->wait_for_input = false;
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
	LOG("PC: 0x%x, opcode: 0x%x - ", state->pc, opcode);
	bool jmp = false;
	switch(opcode & 0xf000) {
		case 0x0000: 
			if (opcode == 0x00e0) {
				// 00E0 - CLS Clear the display..
				LOG("CLS\n");
				for (int i = 0; i < CHIP8_GFX_SIZE; i++) {
					state->gfx[i] = 0;
				}
				state->update_screen = true;
			} else if (opcode == 0x00ee) {
				// 00EE - RET - Return from a subroutine. 
				// The interpreter sets the program counter to the address at the top of the stack, then subtracts 1 from the stack pointer.
				LOG("RET\n");
				state->sp--;
				state->pc = state->stack[state->sp];
				jmp = true;
			} else {
				panic(state, opcode);
			}
			break;
		case 0x1000:
			{
				// 1nnn - JP addr - Jump to location nnn.
				// The interpreter sets the program counter to nnn.
				uint16_t addr = opcode & 0x0fff;
				LOG("JP 0x%x\n", addr);
				state->pc = addr;
				jmp = true;
				break;
			}
		case 0x2000:
			{
				// 2nnn - CALL addr - Call subroutine at nnn.
				// The interpreter increments the stack pointer, then puts the current PC on the top of the stack. The PC is then set to nnn.
				uint16_t addr = opcode & 0x0fff;
				LOG("CALL 0x%x\n", addr);
				state->stack[state->sp] = state->pc + 2;
				state->sp++;
				state->pc = addr;
				jmp = true;
			}
			break;
		case 0x3000:
			{
				// 3XNN	- Skip the following instruction if the value of register VX equals NN
				uint8_t x = (opcode & 0x0f00) >> 8;
				uint8_t byte = opcode & 0x00ff;
				LOG("SE V%d, 0x%x\n", x, byte);
				if (state->v[x] == byte) {
					state->pc += 2;
				}
			}
			break;
		case 0x4000:
			{
				// 4xkk - SNE Vx, byte - Skip next instruction if Vx != kk.
				// The interpreter compares register Vx to kk, and if they are not equal, increments the program counter by 2.
				uint8_t x = (opcode & 0x0f00) >> 8;
				uint8_t byte = opcode & 0x00ff;
				LOG("SNE V%d, 0x%x\n", x, byte);
				if (state->v[x] != byte) {
					state->pc += 2;
				}
			}
			break;
		case 0x6000:
			{
				// 6xkk - LD Vx, byte - Set Vx = kk.
				// The interpreter puts the value kk into register Vx.
				uint8_t x = (opcode & 0x0f00) >> 8;
				uint8_t byte = opcode & 0x00ff;
				LOG("LD V%d, 0x%x\n", x, byte);
				state->v[x] = byte;
				break;
			}
		case 0x7000:
			{
				// 7xkk - ADD Vx, byte - Set Vx = Vx + kk.
				// Adds the value kk to the value of register Vx, then stores the result in Vx. 
				uint8_t x = (opcode & 0x0f00) >> 8;
				uint8_t byte = opcode & 0x00ff;
				LOG("ADD V%d, 0x%x\n", x, byte);
				state->v[x] += byte;
				break;
			}
		case 0x8000:
			switch (opcode & 0x000f) {
				case 0x0:
					{
						// 8xy0 - LD Vx, Vy - Set Vx = Vy.
						// Stores the value of register Vy in register Vx.
						uint8_t x = (opcode & 0x0f00) >> 8;
						uint8_t y = (opcode & 0x00f0) >> 4;
						LOG("LD V%d, V%d\n", x, y);
						state->v[x] = state->v[y];
					}
					break;
				case 0x2:
					{
						// 8xy2 - AND Vx, Vy - Set Vx = Vx AND Vy.
						// Performs a bitwise AND on the values of Vx and Vy, then stores the result in Vx.
						uint8_t x = (opcode & 0x0f00) >> 8;
						uint8_t y = (opcode & 0x00f0) >> 4;
						LOG("AND V%d, V%d\n", x, y);
						state->v[x] &= state->v[y];
					}
					break;
				case 0x4:
					{
						// 8xy4 - ADD Vx, Vy - Set Vx = Vx + Vy, set VF = carry.
						// The values of Vx and Vy are added together. If the result is greater than
						// 8 bits (i.e., > 255,) VF is set to 1, otherwise 0. Only the lowest 8 bits
						// of the result are kept, and stored in Vx.
						uint8_t x = (opcode & 0x0f00) >> 8;
						uint8_t y = (opcode & 0x00f0) >> 4;
						LOG("AND V%d, V%d\n", x, y);
						uint8_t old = state->v[x];
						state->v[x] += state->v[y];
						state->v[0xf] = state->v[x] < old ? 1 : 0;
					}
					break;
				case 0x5:
					{
						// 8xy5 - SUB Vx, Vy - Set Vx = Vx - Vy, set VF = NOT borrow.
						// If Vx > Vy, then VF is set to 1, otherwise 0. Then Vy is subtracted from Vx, and the results stored in Vx.
						uint8_t x = (opcode & 0x0f00) >> 8;
						uint8_t y = (opcode & 0x00f0) >> 4;
						LOG("SUB V%d, V%d\n", x, y);
						state->v[0xf] = state->v[x] > state->v[y] ? 1 : 0;
						state->v[x] -= state->v[y];
					}
					break;
				default:
					panic(state, opcode);
			}
			break;
		case 0x9000:
			{
				// 9xy0 - SNE Vx, Vy - Skip next instruction if Vx != Vy.
				// The values of Vx and Vy are compared, and if they are not equal, the program counter is increased by 2.
				uint8_t x = (opcode & 0x0f00) >> 8;
				uint8_t y = (opcode & 0x00f0) >> 4;
				LOG("SNE V%d, V%d\n", x, y);
				if (state->v[x] != state->v[y]) {
					state->pc += 2;
				}
			}
			break;
		case 0xa000:
			{
				// Annn - LD I, addr - Set I = nnn.
				// The value of register I is set to nnn.
				uint16_t addr = opcode & 0x0fff;
				LOG("LD I, 0x%x\n", addr);
				state->i = addr;
			}
			break;
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
				LOG("DRW V%d, V%d, %d\n", x, y, n);
				state->v[0xf] = 0;
				uint8_t vx = state->v[x];
				uint8_t vy = state->v[y];
				for (int byte = 0; byte < n; byte++) {
					uint8_t line = state->mem[state->i + byte];
					for (int pixel = 0; pixel < 8; pixel++) {
						uint16_t y_index = (vy + byte % CHIP8_SCREEN_HEIGHT) * CHIP8_SCREEN_WIDTH;
						uint16_t x_index = vx + pixel % CHIP8_SCREEN_WIDTH;
						uint8_t old_value = state->gfx[y_index + x_index];
						uint8_t new_value = line & (0x80 >> pixel) ? 1 : 0; 
						state->gfx[y_index + x_index] ^= new_value;
						if (old_value == 1 && new_value == 0) {
							state->v[0xf] = 1;
						}
					}	
				}
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
				LOG("RND V%d, 0x%x\n", x, byte);
				uint8_t r = rand() & byte;
				state->v[x] = r;
			}
			break;
		case 0xe000:
			switch (opcode & 0x00ff) {
				case 0x9e:
					{
						// Ex9E - SKP Vx - Skip next instruction if key with the value of Vx is pressed.
						// Checks the keyboard, and if the key corresponding to the value of Vx is currently
						// in the down position, PC is increased by 2.
						uint8_t x = (opcode & 0x0f00) >> 8;
						LOG("SKP V%d\n", x);
						if (state->key[state->v[x]] == 1) {
							state->pc += 2;
						}
					}
					break;
				case 0xa1:
					{
						// ExA1 - SKNP Vx - Skip next instruction if key with the value of Vx is not pressed.
						// Checks the keyboard, and if the key corresponding to the value of Vx is currently
						// in the up position, PC is increased by 2.
						uint8_t x = (opcode & 0x0f00) >> 8;
						LOG("SKNP V%d\n", x);
						if (state->key[state->v[x]] == 0) {
							state->pc += 2;
						}
					}
					break;
				default:
					panic(state, opcode);
			}
			break;
		case 0xf000:
				switch (opcode & 0x00ff) {
					case 0x1e:
						{
							// Fx1E - ADD I, Vx - Set I = I + Vx.
							// The values of I and Vx are added, and the results are stored in I.
							uint8_t x = (opcode & 0x0f00) >> 8;
							LOG("ADD I, V%d\n", x);
							state->i += state->v[x];
						}
						break;
					case 0x07:
						{
							// Fx15 - LD DT, Vx - Set delay timer = Vx.
							// DT is set equal to the value of Vx.
							uint8_t x = (opcode & 0x0f00) >> 8;
							LOG("LD V%d, DT\n", x);
							state->v[x] = state->dt;
						}
						break;
					case 0x15:
						{
							// Fx15 - LD DT, Vx - Set delay timer = Vx.
							// DT is set equal to the value of Vx.
							uint8_t x = (opcode & 0x0f00) >> 8;
							LOG("LD DT, V%d\n", x);
							state->dt = state->v[x];
						}
						break;
					case 0x18:
						{
							// Fx18 - LD ST, Vx - Set sound timer = Vx.
							uint8_t x = (opcode & 0x0f00) >> 8;
							LOG("LD ST, V%d\n", x);
							state->st = state->v[x];
						}
						break;
					case 0x0a:
						{
							// Fx0A - LD Vx, K - Wait for a key press, store the value of the key in Vx.
							// All execution stops until a key is pressed, then the value of that key is stored in Vx.	
							uint8_t x = (opcode & 0x0f00) >> 8;
							LOG("LD V%d, K\n", x);
							state->wait_for_input = true;
						}
						break;
					case 0x29:
						{
							// Fx29 - LD F, Vx - Set I = location of sprite for digit Vx.
							// The value of I is set to the location for the hexadecimal sprite corresponding to the value of Vx.
							uint8_t x = (opcode & 0x0f00) >> 8;
							LOG("LD F, V%d\n", x);
							state->i = state->v[x] * 5;
						}
						break;
					case 0x33:
						{
							// Fx33 - LD B, Vx - Store BCD representation of Vx in memory locations I, I+1, and I+2.
							// The interpreter takes the decimal value of Vx, and places the hundreds 
							// digit in memory at location in I, the tens digit at location I+1, 
							// and the ones digit at location I+2.	
							uint8_t x = (opcode & 0x0f00) >> 8;
							LOG("LD B, V%d\n", x);
							uint8_t vx = state->v[x];
							uint8_t hundreds = vx / 100;
							uint8_t tens = vx % 100 / 10;
							uint8_t ones = vx % 100 % 10;
							uint16_t i = state->i;
							state->mem[i] = hundreds;
							state->mem[i + 1] = tens;
							state->mem[i + 2] = ones;
						}
						break;
					case 0x55:
						{
							// Fx55 - LD [I], Vx - Store registers V0 through Vx in memory starting at location I.
							// The interpreter copies the values of registers V0 through Vx into memory, starting at the address in I.
							// I is set to I + X + 1 after operation.
							uint8_t x = (opcode & 0x0f00) >> 8;
							LOG("LD [I], V%d\n", x);
							for (int i = 0; i <= x; i++) {
								state->mem[state->i++] = state->v[i];
							}
						}
						break;
					case 0x65:
						{
							// Fx65 - LD Vx, [I] - Read registers V0 through Vx from memory starting at location I.
							// The interpreter reads values from memory starting at location I into registers V0 through Vx.	
							// I is set to I + X + 1 after operation.
							uint8_t x = (opcode & 0x0f00) >> 8;
							LOG("LD V%d, [I]\n", x);
							for (int i = 0; i <= x; i++) {
								state->v[i] = state->mem[state->i++];
							}
						}
						break;
					default:
						panic(state, opcode);
				}
				break;
		default:
			panic(state, opcode);
	}

	if (!jmp) {
		state->pc += 2;
	}
}

void print_state(struct chip8 *state) {
	fprintf(stderr, "pc: 0x%x\n", state->pc);
	fprintf(stderr, "i: 0x%x\n", state->i);
	fprintf(stderr, "sp: 0x%x\n", state->sp);
	fprintf(stderr, "st: 0x%x\n", state->st);
	fprintf(stderr, "dt: 0x%x\n", state->dt);
	for (int i = 0; i < CHIP8_NUM_REGS; i++) {
		fprintf(stderr, "v%d: 0x%x\n", i, state->v[i]);
	}
	for (int i = 0; i < CHIP8_STACK_SIZE; i++) {
		fprintf(stderr, "stack[%d]: 0x%x\n", i, state->stack[i]);
	}
}

void print_gfx(struct chip8 *state) {
	for (int y = 0; y < CHIP8_SCREEN_HEIGHT; y++) {
		for (int x = 0; x < CHIP8_SCREEN_WIDTH; x++) {
			printf("%c", state->gfx[y * CHIP8_SCREEN_WIDTH + x] ? 'O' : '*');
		}
		printf("\n");
	}
	state->update_screen = false;
}

void panic(struct chip8 *state, uint16_t opcode) {
	fprintf(stderr, "Unknown opcode: 0x%x\n", opcode);
	print_state(state);
	exit(1);
}
