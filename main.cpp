
#include "mimalloc-new-delete.h"


#include <iostream>



#include "parser.h"



int main(void)
{
	clau::LoadData test;

	for (int i = 0; i < 8; ++i) {
		int a = clock();
		test.LoadDataFromFile("test.json", 0, 0, true);
		int b = clock();

		std::cout << "test end " << b - a << "ms\n";
	}
	return 0;
}
