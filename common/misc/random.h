#pragma once

/*
 * Random - A simple random number generator class (based on drand48_r)
 */

#include <time.h>
#include <cassert>
#include <cstdlib>

template <class T>
class Random
{
public:
   Random()
   {
      seed(time(NULL));
   }

   inline void seed(long int seedval)
   {
      __attribute__((unused)) int ret = srand48_r(seedval, &buffer);
      assert(ret == 0);
   }
   inline T next(T value)
   {
      double result;
      __attribute__((unused)) int ret = drand48_r(&buffer, &result);
      assert(ret == 0);
      return static_cast<T>(result * value);
   }

private:
   drand48_data buffer;
};
