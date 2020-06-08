#include <locale>
#include <stdio.h>
#include <iostream>
#include "LFQueue/MSQueue.hpp"
#include "LFQueue/MSQueueTests.hpp"


int main() {
	setlocale(LC_ALL, "Russian");

	new MSQueueTests();

	return 0;
}