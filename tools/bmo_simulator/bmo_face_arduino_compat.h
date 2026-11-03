#pragma once

#define Arduino_h

#include <algorithm>
#include <cmath>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

using std::abs;
using std::max;
using std::min;

extern "C" unsigned long millis(void);
extern "C" unsigned long micros(void);
long random(long upperBound);
long random(long lowerBound, long upperBound);
void randomSeed(unsigned long seed);
