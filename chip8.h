#include <stdbool.h>
#include <stdint.h>

#define CHIP8_MEM_SIZE 4096
#define CHIP8_NUM_REGS 16
#define CHIP8_SCREEN_WIDTH 64
#define CHIP8_SCREEN_HEIGHT 32
#define CHIP8_GFX_SIZE CHIP8_SCREEN_WIDTH  * CHIP8_SCREEN_HEIGHT 
#define CHIP8_STACK_SIZE 16
#define CHIP8_NUM_KEYS 16

struct chip8 {
	uint8_t mem[CHIP8_MEM_SIZE];
	uint8_t v[CHIP8_NUM_REGS];
	uint16_t i;
	uint16_t pc;
	uint8_t gfx[CHIP8_GFX_SIZE];
	uint8_t delay_timer;
	uint8_t sound_timer;
	uint16_t stack[CHIP8_STACK_SIZE];
	uint16_t sp;
	uint8_t key[CHIP8_NUM_KEYS];
	uint8_t st;
	uint8_t dt;
	bool update_screen;
	bool wait_for_input;
};

void init_state(struct chip8 *);
void load_rom(struct chip8 *, char *path);
void emulate_cycle(struct chip8 *);
void print_state(struct chip8 *);
void print_gfx(struct chip8 *);	
void panic(struct chip8 *, uint16_t);
