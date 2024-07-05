#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#define DISTANCE 131072
std::vector<double> a1;
std::vector<double> a2;
std::vector<double> a3;
std::vector<double> a4;
std::vector<double> a5;
std::vector<double> a6;
std::vector<double> a7;
std::vector<double> a8;
std::vector<double> a9;
std::vector<int> b;
std::vector<double> c1;
std::vector<double> c2;

void measureTime5( int i_length, int distance) {
    int val;
    auto start = std::chrono::high_resolution_clock::now();
    
    double* a1_b_i = new double[i_length];
    for (int i = 0; i < i_length; ++i) {
        a1_b_i[i] = a1[b[i]];
    }
    double* a2_b_i = new double[i_length];
    for (int i = 0; i < i_length; ++i) {
        a2_b_i[i] = a2[b[i]];
    }
    double* a3_b_i = new double[i_length];
    for (int i = 0; i < i_length; ++i) {
        a3_b_i[i] = a3[b[i]];
    }
    double* a4_b_i = new double[i_length];
    for (int i = 0; i < i_length; ++i) {
        a4_b_i[i] = a4[b[i]];
    }
    double* a5_b_i = new double[i_length];
    for (int i = 0; i < i_length; ++i) {
        a5_b_i[i] = a5[b[i]];
    }

    // auto end1 = std::chrono::high_resolution_clock::now();
    // std::chrono::duration<double> duration1 = end1 - start;
    // std::cout << "Time taken for generating a_b_i vectors is " << duration1.count() << " seconds." << std::endl;
    
    // TODO: Try SPMV super sparse... kernels...
    for (int i = 0; i < i_length; ++i) {
            c1[i] += a1_b_i[i] + a2_b_i[i] + a3_b_i[i] * a4_b_i[i] + a5_b_i[i];
        }


    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    std::cout << "1: (5 memory access)Time taken for i_length = " << i_length 
              << " is " << duration.count() << " seconds with distance: " << distance << std::endl;

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < i_length; ++i) {
            c2[i] += a1[b[i]] + a2[b[i]] + a3[b[i]] * a4[b[i]] + a5[b[i]]; 
        }
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;

    std::cout << "2: (5 memory access)Time taken for i_length = " << i_length 
              << " is " << duration.count() << " secondswith distance: " << distance << std::endl;

    // verify c1 equals to c2
    for (int i = 0; i < i_length; ++i) {
        if (c1[i] != c2[i]) {
            std::cout << "c1[" << i << "] = " << c1[i] << " != c2[" << i << "] = " << c2[i] << std::endl;
            break;
        }
    }

    delete [] a1_b_i;
    delete [] a2_b_i;
    delete [] a3_b_i;
    delete [] a4_b_i;
    delete [] a5_b_i;
}


void measureTime( int i_length, int distance) {
    int val;
    auto start = std::chrono::high_resolution_clock::now();

    double* a1_b_i = new double[i_length];
    double* a2_b_i = new double[i_length];
    double* a3_b_i = new double[i_length];
    double* a4_b_i = new double[i_length];
    double* a5_b_i = new double[i_length];
    double* a6_b_i = new double[i_length];
    double* a7_b_i = new double[i_length];
    double* a8_b_i = new double[i_length];
    double* a9_b_i = new double[i_length];

    auto end1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration1 = end1 - start;
    std::cout << "Time taken for creating buffer for a_b_i is " << duration1.count() << " seconds." << std::endl;

    for (int i = 0; i < i_length; ++i) {
        a1_b_i[i] = a1[b[i]];
    }
    
    for (int i = 0; i < i_length; ++i) {
        a2_b_i[i] = a2[b[i]];
    }
    
    for (int i = 0; i < i_length; ++i) {
        a3_b_i[i] = a3[b[i]];
    }
    
    for (int i = 0; i < i_length; ++i) {
        a4_b_i[i] = a4[b[i]];
    }
    
    for (int i = 0; i < i_length; ++i) {
        a5_b_i[i] = a5[b[i]];
    }
    
    for (int i = 0; i < i_length; ++i) {
        a6_b_i[i] = a6[b[i]];
    }
   
    for (int i = 0; i < i_length; ++i) {
        a7_b_i[i] = a7[b[i]];
    }
    
    for (int i = 0; i < i_length; ++i) {
        a8_b_i[i] = a8[b[i]];
    }
    
    for (int i = 0; i < i_length; ++i) {
        a9_b_i[i] = a9[b[i]];
    }

    
    
    // TODO: Try SPMV super sparse... kernels...
    for (int i = 0; i < i_length; ++i) {
            c1[i] += a1_b_i[i] + a2_b_i[i] + a3_b_i[i] * a4_b_i[i] + a5_b_i[i] + a6_b_i[i] + a7_b_i[i] + a8_b_i[i] + a9_b_i[i];
        }


    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    std::cout << "1: Time taken for i_length = " << i_length 
              << " is " << duration.count() << " seconds with distance: " << distance << std::endl;

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < i_length; ++i) {
            c2[i] += a1[b[i]] + a2[b[i]] + a3[b[i]] * a4[b[i]] + a5[b[i]] + a6[b[i]] + a7[b[i]] + a8[b[i]] + a9[b[i]]; 
        }
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;

    std::cout << "2: Time taken for i_length = " << i_length 
              << " is " << duration.count() << " secondswith distance: " << distance << std::endl;

    // verify c1 equals to c2
    for (int i = 0; i < i_length; ++i) {
        if (c1[i] != c2[i]) {
            std::cout << "c1[" << i << "] = " << c1[i] << " != c2[" << i << "] = " << c2[i] << std::endl;
            break;
        }
    }

    delete [] a1_b_i;
    delete [] a2_b_i;
    delete [] a3_b_i;
    delete [] a4_b_i;
    delete [] a5_b_i;
    delete [] a6_b_i;
    delete [] a7_b_i;
    delete [] a8_b_i;
    delete [] a9_b_i;
}

int main() {
    // Example usage with configurable a and ranges
    int length = 500000000;
    c1 = std::vector<double>(length);
    c2 = std::vector<double>(length);
    a1 = std::vector<double>(length);
    std::iota(a1.begin(), a1.end(), 1.0); // Fill a with values 1, 2, ..., 100
    a2 = std::vector<double>(length);
    std::iota(a2.begin(), a2.end(), 1.0); // Fill a with values 1, 2, ..., 100
    a3 = std::vector<double>(length);
    std::iota(a3.begin(), a3.end(), 1.0); // Fill a with values 1, 2, ..., 100
    a4 = std::vector<double>(length);
    std::iota(a4.begin(), a4.end(), 1.0); // Fill a with values 1, 2, ..., 100
    a5 = std::vector<double>(length);
    std::iota(a5.begin(), a5.end(), 1.0); // Fill a with values 1, 2, ..., 100
    a6 = std::vector<double>(length);
    std::iota(a6.begin(), a6.end(), 1.0); // Fill a with values 1, 2, ..., 100
    a7 = std::vector<double>(length);
    std::iota(a7.begin(), a7.end(), 1.0); // Fill a with values 1, 2, ..., 100
    a8 = std::vector<double>(length);
    std::iota(a8.begin(), a8.end(), 1.0); // Fill a with values 1, 2, ..., 100
    a9 = std::vector<double>(length);
    std::iota(a9.begin(), a9.end(), 1.0); // Fill a with values 1, 2, ..., 100

    int b_size = 500000000; // Size of b vector

    // Generate b with random indices
    b = std::vector<int>(b_size);
    int distance = 1;
    for (int i = 0; i < b_size; ++i) {
        int var = rand() % distance;
        b[i] = std::max(var, i-var);
    }
    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::uniform_int_distribution<> dist(0, a1.size() - 1);
    // std::generate(b.begin(), b.end(), [&]() { return dist(gen); });

    measureTime(b_size, distance);

    distance = 131072;
    for (int i = 0; i < b_size; ++i) {
        int var = rand() % distance;
        b[i] = std::max(var, i-var);
    }
    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::uniform_int_distribution<> dist(0, a1.size() - 1);
    // std::generate(b.begin(), b.end(), [&]() { return dist(gen); });

    measureTime(b_size, distance);

    distance = 1310720;
    for (int i = 0; i < b_size; ++i) {
        int var = rand() % distance;
        b[i] = std::max(var, i-var);
    }
    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::uniform_int_distribution<> dist(0, a1.size() - 1);
    // std::generate(b.begin(), b.end(), [&]() { return dist(gen); });

    measureTime(b_size, distance);

    distance = 13107200;
    for (int i = 0; i < b_size; ++i) {
        int var = rand() % distance;
        b[i] = std::max(var, i-var);
    }
    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::uniform_int_distribution<> dist(0, a1.size() - 1);
    // std::generate(b.begin(), b.end(), [&]() { return dist(gen); });

    measureTime(b_size, distance);

    distance = length;
    for (int i = 0; i < b_size; ++i) {
        int var = rand() % distance;
        b[i] = std::max(var, i-var);
    }
    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::uniform_int_distribution<> dist(0, a1.size() - 1);
    // std::generate(b.begin(), b.end(), [&]() { return dist(gen); });

    measureTime(b_size, distance);

    return 0;
}
