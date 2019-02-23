#include "pch.h"
#include <iostream>

#include <SDL.h>

int main(int argv, char** argc)
{
	if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
	{
		std::cerr << "SDL failed to start" << std::endl;
	}

    std::cout << "Hello World!\n"; 
	return 0;
}


