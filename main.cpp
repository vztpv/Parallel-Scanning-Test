
#include "mimalloc-new-delete.h"


#include <iostream>



#include "parser.h"


int main(void)
{
    //clau_test::run();

    //return 0;

	clau::LoadData test;

	for (int i = 0; i < 6; ++i) {
		int a = clock();
		test.LoadDataFromFile("citylots.json",0, 0, true);
		int b = clock();

		std::cout << "test end " << b - a << "ms\n";
	}
	return 0;
	{
	
		// read raw_file(char array) using stl from "citylots.json"
		char* raw_file = nullptr;
		size_t raw_file_size = 0;
		{
			std::ifstream file("citylots.json", std::ios::binary | std::ios::ate);
			if (file.is_open()) {
				raw_file_size = file.tellg();
				raw_file = new char[raw_file_size + 1];
				file.seekg(0, std::ios::beg);
				file.read(raw_file, raw_file_size);
				raw_file[raw_file_size] = '\0';
				file.close();
			}	
			int a = clock();
			clau::Token8* tokens = new clau::Token8[raw_file_size * 72]; // allocate enough size
			uint64_t token_len = clau::write_token(raw_file, raw_file_size, tokens);
			delete[] raw_file;
			//for (uint64_t i = 0; i < token_len; ++i) {
				//clau::read_token(tokens, token_len);
				//getchar();
			//}
			delete[] tokens;
			
			int b = clock();
			std::cout << b - a << "ms\n";
			std::cout << token_len << "\n";
		}
	}
	return 0;
}
