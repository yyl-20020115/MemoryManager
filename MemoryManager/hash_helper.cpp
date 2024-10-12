#include "hash_helper.h"
#include <corecrt_math.h>

bool hash_helper::is_prime(size_t candidate)
{
	if (candidate <= 1) return false;
	if ((candidate & 1) == 1)
	{
		size_t dc = candidate % 6;
		if (dc != 1 && dc != 5) return false;
		size_t limit = sqrt(candidate);
		for (size_t divisor = 3; divisor <= limit; divisor += 2)
			if ((candidate % divisor) == 0)
				return false;
		return true;
	}
	return candidate == 2;
}

size_t hash_helper::generate_prime_less_than(size_t n)
{
	for (size_t i = n; i > 0ULL; i--)
		if (is_prime(i))
			return i;
	return 1ULL;
}
