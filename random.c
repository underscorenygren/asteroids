/* Utility library for randomization.
 * All assume uniforme probability distribution. */
#ifndef RANDOM_C
#define RANDOM_C

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* returns a float in the range (0.0, a) */
float random_float(float a) {
	float res = (float)rand()/((float)(RAND_MAX/a));
	assert(res > 0.0);
	assert(res <= a);
	return  res;
}

/* returns true with probability a */
bool random_prob(float a) {
	uint32_t res = rand();
	return (float)res < RAND_MAX * a;
}

/* returns an int in the range (0, n) (non-inclusive) */
uint32_t random_uint32_t(uint32_t n) {
	uint32_t res = rand();
	uint32_t chunk = RAND_MAX / n;
	uint32_t i = 0;

	//We iterate until result is larger than the chunk
	//cutoff (i+1), at which point we know we are in that i.
	for (; i < n; i++) {
		if (res < chunk * (i+1)) {
			break;
		}
	}
	//NB since we check against i+1, i will always be < n
	assert(i < n);

	return i;
}

/* returns an angle in (0, 360.0) */
float random_angle() {
	return 360.0 * random_float(1.0);
}


/* Seeds random with time */
void random_seed() {
	srand(time(NULL));
}

#endif /* RANDOM_C */
