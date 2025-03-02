/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/
#ifndef BENCHMARK_SLEEP_H_
#define BENCHMARK_SLEEP_H_

namespace benchmark {
const int kNumMillisPerSecond = 1000;
const int kNumMicrosPerMilli = 1000;
const int kNumMicrosPerSecond = kNumMillisPerSecond * 1000;
const int kNumNanosPerMicro = 1000;
const int kNumNanosPerSecond = kNumNanosPerMicro * kNumMicrosPerSecond;

void SleepForMilliseconds(int milliseconds);
void SleepForSeconds(double seconds);
}  // end namespace benchmark

#endif  // BENCHMARK_SLEEP_H_
