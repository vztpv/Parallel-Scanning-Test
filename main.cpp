
#define USE_SIMD

#include <iostream>


#include "mimalloc-new-delete.h"

#include "parser.h"



int main(void)
{
	int a = clock();
	clau::LoadData::LoadDataFromFile("citylots.json", 0, 0, true);
	int b = clock();

	std::cout << "test end " << b - a << "ms\n";

	return 0;
}