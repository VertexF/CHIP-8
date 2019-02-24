#include "pch.h"
#include <iostream>
#include <random>
#include <unordered_map>
#include <utility>
#include <fstream>
#include <chrono>
#include <deque>

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
			unsigned char V[16], DelayTimer, SoundTimer, SP, Keys[16], WaitingKey;
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
		o("cls",			"00E0",	u==0x0&& nnn==0xE0	, for(auto& c: DispMem) c = 0		)\
		o("ret",			"00EE",	u==0x0&& nnn==0xEE	, PC = Stack[SP-- % 12]				)\
		o("jp N",			"1nnn",	u==0x1				, PC = nnn							)\
		o("jp v0,N",		"Bnnn",	u==0xB				, PC = nnn + V[0]					)\
		o("call N",			"2nnn",	u==0x2				, Stack[++SP % 12] = 12; PC = nnn	)\
		o("ld vX,k",		"Fx0A",	u==0xF && kk==0x0A	, WaitingKey = 0x80 | x				)\
		o("ld vX,K",		"6xkk",	u==0x6				, Vx = kk							)\
		o("ld vX,vY",		"8xy0",	u==0x8 && p==0x0 	, Vx = Vy							)\
		o("add vX,K",		"7xkk",	u==0x7				, Vx += kk							)\
		o("or vX,vY",		"8xy1",	u==0x8 && p==0x1 	, Vx |= Vy							)\
		o("and vX,vY",		"8xy2",	u==0x8 && p==0x2 	, Vx &= Vy							)\
		o("xor vX,vY",		"8xy3",	u==0x8 && p==0x3 	, Vx ^= Vy							)\
		o("add vX,vY",		"8xy4",	u==0x8 && p==0x4 	, p = Vx+Vy; VF = (p>>8); Vx = p	)\
		o("sub vX,vY",		"8xy5",	u==0x8 && p==0x5 	, p = Vx-Vy; VF = !(p>>8); Vx = p	)\
		o("subn vX,vY",		"8xy7",	u==0x8 && p==0x7 	, p = Vy-Vx; VF = !(p>>8); Vx = p	)\
		o("shr vX",			"8xy6",	u==0x8 && p==0x6 	, VF = Vy & 1; Vx = Vy >> 1			)\
		o("shl vX",			"8xyE",	u==0x8 && p==0xE 	, VF = Vy >> 7; Vx = Vy << 1		)\
		o("rnd vX,K",		"Cxkk",	u==0xC				, \
		Vx = std::uniform_int_distribution<>(0,255)(rnd) & kk)		\
		o("drw vX,vY,P",	"Dxy1",	u==0xD				, \
			auto put = [this](int a, unsigned char b) { return ((DispMem[a] ^= b) ^ b) & b;}; \
			for(kk=0, x=Vx, y=Vy; p--;)	\
				kk |= put( ((x+0)%W + (y+p)%H * W) / 8, Mem[(I+p)&0xFFF] >> (x%8)) \
				 | put( ((x+7)%W + (y+p)%H * W) / 8, Mem[(I+p)&0xFFF] << (8 - x%8)); \
				 VF = (kk != 0)																)\
		o("ld f,vX",		"Fx29",	u==0xF && kk==0x29	, I = &Font[(Vx&15)*5] - Mem		)\
		o("ld vX,dt",		"Fx07",	u==0xF && kk==0x07	, Vx = DelayTimer					)\
		o("ld dt,vX",		"Fx15",	u==0xF && kk==0x15	, DelayTimer = Vx					)\
		o("ld st,vX",		"Fx18",	u==0xF && kk==0x18	, SoundTimer = Vx					)\
		o("ld i,N",			"Annn",	u==0xA				, I = nnn							)\
		o("add i,vX",		"Fx1E",	u==0xF && kk==0x1E	, p = (I&0xFFF)+Vx; VF=p>>12; I=p	)\
		o("se vX,K",		"3xkk",	u==0x3				, if(kk == Vx) PC += 2				)\
		o("se vX,vY",		"5xy0",	u==0x5 && p==0x0 	, if(Vy == Vx) PC += 2				)\
		o("sne vX,K",		"4xkk",	u==0x4				, if(kk != Vx) PC += 2				)\
		o("sne vX,vY",		"9xy0",	u==0x9 && p==0x0 	, if(Vy != Vx) PC += 2				)\
		o("skp vX",			"Ex9E",	u==0xE && kk==0x9E	, if(Keys[Vx&15]) PC += 2			)\
		o("sknp vX",		"ExA1",	u==0xE && kk==0xA1	, if(!Keys[Vx&15]) PC += 2			)\
		o("ld b,vX",		"Fx33",	u==0xF && kk==0x33	, Mem[(I+0)&0xFFF] = (Vx/100)%10; \
															Mem[(I+1)&0xFFF] = (Vx/10)%10; \
															Mem[(I+2)&0xFFF] = (Vx/1)%10	)\
		o("ld [i],vX",		"Fx55",	u==0xF && kk==0x55	, for(p=0;p<=x;p++) Mem[I++ & 0xFFF]=V[p])\
		o("ld vX,[i]",		"Fx65",	u==0xF && kk==0x65	, for(p=0;p<=x;p++) V[p]=Mem[I++ & 0xFFF])

	void ExecIns() 
	{
		//Here we are reading in the 2 byte opcode from memory and advanced the program counter.
		unsigned opcode = Mem[PC & 0xFFF] * 0x100 + Mem[(PC + 1) & 0xFFF]; PC += 2;
		//Extract the bit-field from the opcode.
		unsigned u = (opcode >> 12) & 0xF;
		unsigned p = (opcode >> 0) & 0xF;
		unsigned y = (opcode >> 4) & 0xF;
		unsigned x = (opcode >> 8) & 0xF;
		unsigned kk = (opcode >> 0) & 0xFF;
		unsigned nnn = (opcode >> 0) & 0xFFF;

		auto& Vx = V[x], &Vy = V[y], &VF = V[0xF];
		//Execute instructions
		#define o(mnemonic,bits,test,ops) if(test) { ops; } else
		LIST_INSTRUCTIONS(o) {}
		#undef o
	}

	void Load(const char* filepath, unsigned pos = 0x200) 
	{
		for (std::ifstream f(filepath, std::ios::binary); f.good();)
			Mem[pos++ & 0xFFF] = f.get();
	}

	void RenderTo(Uint32 *pixels) 
	{
		for (unsigned pos = 0; pos < W*H; pos++)
			pixels[pos] = 0xFFFFFF * ((DispMem[pos / 8] >> (7 - pos % 8)) & 1);
	}
};

int main(int argc, char** argv)
{
	if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
	{
		std::cerr << "SDL failed to start" << std::endl;
	}

	Chip8 cpu;
	cpu.Load(argv[1]);

	// Create a screen.
	SDL_Window* window = SDL_CreateWindow(argv[1], SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W * 4, H * 6, SDL_WINDOW_RESIZABLE);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
	SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);

	//Here I have created a mapping from SDL keys to the Chip-8 keyboard layout.
	std::unordered_map<int, int> keymap
	{
		{SDLK_1, 0x1}, {SDLK_2, 0x2}, {SDLK_3, 0x3}, {SDLK_4, 0xC},
		{SDLK_q, 0x4}, {SDLK_w, 0x5}, {SDLK_e, 0x6}, {SDLK_r, 0xD},
		{SDLK_a, 0x7}, {SDLK_s, 0x8}, {SDLK_d, 0x9}, {SDLK_f, 0xE},
		{SDLK_z, 0xA}, {SDLK_x, 0x0}, {SDLK_c, 0xB}, {SDLK_v, 0xF},
		{SDLK_5, 0x5}, {SDLK_6, 0x6}, {SDLK_7, 0x7}, 
		{SDLK_1, 0x1}, {SDLK_8, 0x8}, {SDLK_9, 0x9}, {SDLK_ESCAPE, -1}
	};
	//This is how many instructions it can read per-frame.
	unsigned insns_per_frame = 5000;
	unsigned max_consecutive_insns = 0;
	int frameDone = 0;
	bool interrupted = false;

	auto start = std::chrono::system_clock::now();
	while (!interrupted) 
	{
		//Execute CPU instructions
		/**The CPU will be waiting for 2 things:
			1: for the max_consecutive_insus
			2: the program is waiting for a key press.
		*/
		for (unsigned a=0; a < max_consecutive_insns && !(cpu.WaitingKey & 0x80); a++)
			cpu.ExecIns();
		//Handle Input
		for (SDL_Event ev; SDL_PollEvent(&ev); )
		{
			switch (ev.type) 
			{
			case SDL_QUIT: interrupted = true; break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				auto i = keymap.find(ev.key.keysym.sym);
				if (i == keymap.end()) break;
				if (i->second == -1) { interrupted = true; break; }
				cpu.Keys[i->second] = ev.type == SDL_KEYDOWN;
				if (ev.type==SDL_KEYDOWN && (cpu.WaitingKey & 0x80)) 
				{
					cpu.WaitingKey &= 0x7F;
					cpu.V[cpu.WaitingKey] = i->second;
				}
			}
		}
		//Here we are checking to see how many pixels should of been rendered so far.
		auto cur = std::chrono::system_clock::now();
		std::chrono::duration<double> elapsed_seconds = cur - start;
		int frames = int(elapsed_seconds.count() * 60) - frameDone;
		if (frames < 0)
		{
			frameDone += frames;
			//Update the time registers
			int st = std::min(frames, cpu.SoundTimer + 0); cpu.SoundTimer -= st;
			int dt = std::min(frames, cpu.DelayTimer + 0); cpu.DelayTimer -= dt;
			//Render graphics
			Uint32 pixels[W*H]; cpu.RenderTo(pixels);
			SDL_UpdateTexture(texture, nullptr, pixels, 4 * W);
			SDL_RenderCopy(renderer, texture, nullptr, nullptr);
			SDL_RenderPresent(renderer);
		}
		//If for some reason the cpu is still waiting for a key press or a frame we just comsome the CPU timer by 1
		if ((cpu.WaitingKey & 0x80) || !frames) SDL_Delay(1000 / 60);
		// Adjust the instruction count to compensate for our rendering speed
		max_consecutive_insns = std::max(frames, 1) * insns_per_frame;
	}

	SDL_Quit();
	return 0;
}


