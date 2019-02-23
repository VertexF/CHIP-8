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

	//The initialisation
	Chip8()
	{
		//Here we are installing the Font.
		auto *p = Font;
		for (unsigned n: { 0xF999F, 0x26227, 0xF1F8F, 0xF1F1F, 0x99F11, 0xF8F1F, 0xF8F9F, 0xF1244,
							0xF9F9F, 0xF9F1F, 0xF9F99, 0xE9E9E, 0xF888F, 0xE999E, 0xF8F8F, 0xF8F88}) 
		{
			for (int a = 16; a >= 0; a -= 4) 
				*p++ = (n >> a) & 0xF;
		}
	}
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


