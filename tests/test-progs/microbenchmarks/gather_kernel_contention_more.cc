#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <omp.h>

// #define GEM5
#ifdef GEM5
#include <gem5/m5ops.h>
#define A1_TAG       0
#define A2_TAG       1
#define A3_TAG       2
#define A4_TAG       3
#define A5_TAG       4
#define A6_TAG       5
#define B_TAG       6
#define C_TAG       7
#endif

// #define ORIGINALLENGTH 100000000
#define LENGTH 100000000

std::vector<double> a1;
std::vector<double> a2;
std::vector<double> a3;
std::vector<double> a4;
std::vector<double> a5;
std::vector<double> a6;
std::vector<int> b;
std::vector<double> c1;

void measureTime( int i_length, int distance) {
    int val;
    auto start = std::chrono::high_resolution_clock::now();

#ifdef GEM5
    m5_work_begin(0, 0);
    m5_reset_stats(0, 0);
#endif
   
    // double a1_b_i[LENGTH];
#ifdef GEM5
            m5_clear_mem_region();
            m5_add_mem_region(a1.data(), &a1[LENGTH-1], A1_TAG);
            m5_add_mem_region(a2.data(), &a2[LENGTH-1], A2_TAG);
            m5_add_mem_region(a3.data(), &a3[LENGTH-1], A3_TAG);
            m5_add_mem_region(a4.data(), &a4[LENGTH-1], A4_TAG);
            m5_add_mem_region(a5.data(), &a5[LENGTH-1], A5_TAG);
            m5_add_mem_region(a6.data(), &a6[LENGTH-1], A6_TAG);
            m5_add_mem_region(b.data(), &b[LENGTH-1], B_TAG);
            m5_add_mem_region(c1.data(), &c1[LENGTH-1], C_TAG);
#endif
        auto end1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration1 = end1 - start;
    std::cout << "1: Time taken for allocating buffer for i_length = " << i_length 
              << " is " << duration1.count() << " seconds with distance: " << distance << std::endl;
    for (int i = 0; i < LENGTH; ++i) {
        c1[i] = a1[b[i]] + a2[b[i]] + a3[b[i]] + a4[b[i]] + a5[b[i]] + a6[b[i]];
    }



    // // auto end1 = std::chrono::high_resolution_clock::now();
    // // std::chrono::duration<double> duration1 = end1 - start;
    // // std::cout << "Time taken for generating a_b_i vectors is " << duration1.count() << " seconds." << std::endl;
    
    // // TODO: Try SPMV super sparse... kernels...
    // for (int i = 0; i < i_length; ++i) {
    //         c1[i] += a1_b_i[i];
    //     }
#ifdef GEM5
            m5_dump_stats(0, 0);
            m5_work_end(0, 0);
            m5_clear_mem_region();
#endif


    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    std::cout << "1: Time taken for i_length = " << i_length 
              << " is " << duration.count() << " seconds with distance: " << distance << std::endl;

    start = std::chrono::high_resolution_clock::now();
}

int main() {
    // Example usage with configurable a and ranges
    int length = LENGTH;
    c1 = std::vector<double>(length);
    a1 = std::vector<double>(length);
    a2 = std::vector<double>(length);
    a3 = std::vector<double>(length);
    a4 = std::vector<double>(length);
    a5 = std::vector<double>(length);
    a6 = std::vector<double>(length);
    std::iota(a1.begin(), a1.end(), 1.0); // Fill a with values 1, 2, ..., 100
    std::iota(a2.begin(), a2.end(), 1.0); // Fill a with values 1, 2, ..., 100
    std::iota(a3.begin(), a3.end(), 1.0); // Fill a with values 1, 2, ..., 100
    std::iota(a4.begin(), a4.end(), 1.0); // Fill a with values 1, 2, ..., 100
    std::iota(a5.begin(), a5.end(), 1.0); // Fill a with values 1, 2, ..., 100
    std::iota(a6.begin(), a6.end(), 1.0); // Fill a with values 1, 2, ..., 100

    // Generate b with random indices
    b = std::vector<int>(length);
    int distance = length;
    for (int i = 0; i < length; ++i) {
        int var = rand() % distance;
        b[i] = std::max(var, i-var);
    }
    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::uniform_int_distribution<> dist(0, a1.size() - 1);
    // std::generate(b.begin(), b.end(), [&]() { return dist(gen); });
    #ifdef GEM5
    std::cout << "Fake Checkpoint started" << std::endl;
    m5_checkpoint(0, 0);
    std::cout << "Fake Checkpoint ended" << std::endl;
    #endif

    measureTime(length, distance);
    return 0;
}
