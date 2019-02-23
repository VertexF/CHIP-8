#include "pch.h"
#include <iostream>

#include <SDL.h>

static constexpr const unsigned W = 64, H = 32;

struct Chip8
{
	union
	{
		//Chip-8 has 0x1000 of RAM.
		unsigned char Mem[0x1000] = { 0 };
		//Up to 0x200 of the RAM is simulated internals.
		struct
		{
			unsigned char V[16], DelayTimer, SoundTimer, SP, Keys[16];
			unsigned char DispMem[W * H / 8], Font[16 * 5]; //Monochrome
			unsigned short PC, Stack[12], I;
		};
	};
};

int main(int argv, char** argc)
{
	if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
	{
		std::cerr << "SDL failed to start" << std::endl;
	}

    std::cout << "Hello World!\n"; 
	return 0;
}


