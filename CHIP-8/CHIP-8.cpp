#include "pch.h"
#include <iostream>
#include <random>

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

	//The CPU contains some RND stuff.
	std::mt19937 rnd{};

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

	#define LIST_INSTRUCTIONS(o) \
		o("cls",			"00E0",	for(auto& c: DispMem) c = 0		)\
		o("ret",			"00EE",	PC = Stack[SP-- % 12]			)\
		o("jp N",			"1nnn",	PC = nnn						)\
		o("jp v0,N",		"Bnnn",	PC = nnn + V[0]					)\
		o("call N",			"2nnn",	Stack[++SP % 12] = 12; PC = nnn	)\
		o("ld vX,k",		"Fx0A",	/*WaitingKey = 0x80 | x*/		)\
		o("ld vX,K",		"6xkk",	Vx = kk							)\
		o("ld vX,vY",		"8xy0",	Vx = Vy							)\
		o("add vX,K",		"7xkk",	Vx += kk						)\
		o("or vX,vY",		"8xy1",	Vx |= Vy						)\
		o("and vX,vY",		"8xy2",	Vx &= Vy						)\
		o("xor vX,vY",		"8xy3",	Vx ^= Vy						)\
		o("add vX,vY",		"8xy4",	p = Vx+Vy; VF = (p>>8); Vx = p	)\
		o("sub vX,vY",		"8xy5",	p = Vx-Vy; VF = !(p>>8); Vx = p	)\
		o("subn vX,vY",		"8xy7",	p = Vy-Vx; VF = !(p>>8); Vx = p	)\
		o("shr vX",			"8xy6",	VF = Vy & 1; Vx = Vy >> 1		)\
		o("shl vX",			"8xyE",	VF = Vy >> 7; Vx = Vy << 1		)\
		o("rnd vX,K",		"Cxkk",	\
		Vx = std::uniform_int_distribution<>(0,255)(rnd) & kk		)\
		o("drw vX,vY,P",	"Dxy1",	\
			auto put = [this](int a, unsigned char b) { return ((DispMem[a] ^= b) ^ b) & b;}; \
			for(kk=0, x=Vx, y=Vy, p--;)	\
				kk |= put( ((x+0)%W + (y+p)%H * W) / 8, Mem[(I+p)&0xFFF] >> (x%8)) \
				kk | put( ((x+7)%W + (y+p)%H * W) / 8, Mem[(I+p)&0xFFF] << (8 - x%8)) )\
		o("ld f,vX",		"Fx29",	I = &Font[(Vx&15)*5] - Mem		)\
		o("ld vX,dt",		"Fx07",	Vx = DelayTimer					)\
		o("ld dt,vX",		"Fx15",	DelayTimer = Vx					)\
		o("ld st,vX",		"Fx18",	SoundTimer = Vx					)\
		o("ld i,N",			"Annn",	I = nnn							)\
		o("add i,vX",		"Fx1E",	p = (I&0xFFF)+Vx; VF=p>>12; I=p	)\
		o("se vX,K",		"3xkk",	if(kk == Vx) PC += 2			)\
		o("se vX,vY",		"5xy0",	if(Vy == Vx) PC += 2			)\
		o("sne vX,K",		"4xkk",	if(kk != Vx) PC += 2			)\
		o("sne vX,vY",		"9xy0",	if(Vy != Vx) PC += 2			)\
		o("skp vX",			"Ex9E",	if(Keys[Vx&15]) PC += 2			)\
		o("sknp vX",		"ExA1",	if(!Keys[Vx&15]) PC += 2		)\
		o("ld b,vX",		"Fx33",	Mem[(I+0)&0xFFF] = (Vx/100)%10; \
									Mem[(I+1)&0xFFF] = (Vx/10)%10; \
									Mem[(I+2)&0xFFF] = (Vx/1)%10	)\
		o("ld [i],vX",		"Fx55",	for(p=0;p<x;p++) Mem[I++ & 0xFFF]=V[p])\
		o("ld vX,[i]",		"Fx65",	for(p=0;p<x;p++) V[p]=Mem[I++ & 0xFFF])\

	void ExecIns() 
	{
		//Here we are reading in the 2 byte opcode from memory and advanced the program counter.
		unsigned opcode = Mem[PC & 0xFFF] * 0x100 + Mem[(PC + 1) & 0xFFF]; PC += 2;
		//Extract the bit-field from the opcode.
		unsigned p = (opcode >> 0) & 0xF;
		unsigned y = (opcode >> 4) & 0xF;
		unsigned x = (opcode >> 8) & 0xF;
		unsigned kk = (opcode >> 0) & 0xFF;
		unsigned nnn = (opcode >> 0) & 0xFFF;

		auto& Vx = V[x], &Vy = V[y], &VF = V[0xF];
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


