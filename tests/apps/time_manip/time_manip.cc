#include <cstdio>
#include <cassert>
#include <sys/time.h>
#include <time.h>

void doWork()
{
   volatile int sum = 0;
   for (volatile int i = 0; i < 10000; i++)
      sum += i;
}

void gettimeofday_test()
{
   int ret;
   struct timeval start_time, end_time;
   
   ret = gettimeofday(&start_time, NULL);
   assert(ret == 0);
   
   doWork();
   
   ret = gettimeofday(&end_time, NULL);
   assert(ret == 0);

   if (end_time.tv_usec < start_time.tv_usec)
   {
      end_time.tv_sec -= 1;
      end_time.tv_usec += 1000000;
   }

   printf("\ngettimeofday TEST:\nStart(%lu sec, %lu usec), End(%lu sec, %lu usec), Duration(%lu sec, %lu usec)\n",
          start_time.tv_sec, start_time.tv_usec, end_time.tv_sec, end_time.tv_usec,
          end_time.tv_sec - start_time.tv_sec,
          end_time.tv_usec - start_time.tv_usec);
}

void clock_getime_test()
{
   int ret;
   struct timespec start_time, end_time;

   // Can use CLOCK_REALTIME or CLOCK_MONOTONIC
   ret = clock_gettime(CLOCK_MONOTONIC, &start_time);
   assert(ret == 0);

   doWork();

   ret = clock_gettime(CLOCK_MONOTONIC, &end_time);
   assert(ret == 0);
   
   if (end_time.tv_nsec < start_time.tv_nsec)
   {
      end_time.tv_sec -= 1;
      end_time.tv_nsec += 1000000000;
   }

   printf("\nclock_gettime TEST:\nStart(%lu sec, %lu nsec), End(%lu sec, %lu nsec), Duration(%lu sec, %lu nsec)\n",
          start_time.tv_sec, start_time.tv_nsec, end_time.tv_sec, end_time.tv_nsec,
          end_time.tv_sec - start_time.tv_sec,
          end_time.tv_nsec - start_time.tv_nsec);
}

void clock_getres_test()
{
   int ret;
   struct timespec res;

   // Can use CLOCK_REALTIME or CLOCK_MONOTONIC
   ret = clock_getres(CLOCK_MONOTONIC, &res);
   printf("\nclock_getres TEST:\nResult(%lu sec, %lu nsec)\n",
          res.tv_sec, res.tv_nsec);
}

void time_test()
{
   time_t start_time, start_time1, end_time;

   start_time = time(&start_time1);
   assert(start_time == start_time1);

   doWork();

   end_time = time(NULL);

   printf("\ntime TEST:\nStart(%lu sec), End(%lu sec), Duration(%lu sec)\n\n",
          start_time, end_time, end_time - start_time);
}

int main(int argc, char* argv[])
{
   int ret;

   // (1) gettimeofday
   gettimeofday_test();

   // (2) clock_gettime
   clock_getime_test();

   // (3) clock_getres
   clock_getres_test();

   // (4) time
   time_test();

   return 0;
}
