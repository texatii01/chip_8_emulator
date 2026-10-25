#include <array>
#include <iostream>
#include <string>
#include <fstream>


struct CPU {

	std::array<uint8_t, 16> regs = { 0 }; //main registers

	uint16_t VI = 0; //addr
	
	uint16_t pc = 0x200; //keeps track of current position in memory

	

};

struct MemBus {

	std::array<uint16_t, 0xFFF> ram = { 0 }; // 4096Kbs of ram


};

int main() {

	//initialization

	CPU cpu;

	MemBus membus;

	//todo: character loader to the ram (0-f)




	//FILE CHECKER AND OPENER

	std::ifstream file("pong.ch8");

	if (!file.is_open) {

		std::cerr << "Error: Could not open file." << std::endl;
		return 1;
	}

	while (std::getline(file, file) {

		std::cout
	}
	



	return;
}