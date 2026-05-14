#include "TestRunner.h"

int main()
{
	if (!tests::TestRunner::RunAll())
	{
		return 1;
	}

	return 0;
}