
#define USE_SIMD

#include "mimalloc-new-delete.h"


#include <iostream>



#include "parser.h"



int main(void)
{
	for (int i = 0; i < 8; ++i) {
		int a = clock();
		clau::LoadData test;
		test.LoadDataFromFile("citylots.json", 0, 0, true);
		int b = clock();

		std::cout << "test end " << b - a << "ms\n";
	}
	return 0;
}
