/*************************************************************************
 *                                                                       *
 *       N  A  S     P A R A L L E L     B E N C H M A R K S  3.4        *
 *                                                                       *
 *                      O p e n M P     V E R S I O N                    *
 *                                                                       *
 *                                  I S                                  *
 *                                                                       *
 *************************************************************************
 *                                                                       *
 *   This benchmark is an OpenMP version of the NPB IS code.             *
 *   It is described in NAS Technical Report 99-011.                     *
 *                                                                       *
 *   Permission to use, copy, distribute and modify this software        *
 *   for any purpose with or without fee is hereby granted.  We          *
 *   request, however, that all derived work reference the NAS           *
 *   Parallel Benchmarks 3.4. This software is provided "as is"          *
 *   without express or implied warranty.                                *
 *                                                                       *
 *   Information on NPB 3.4, including the technical report, the         *
 *   original specifications, source code, results and information       *
 *   on how to submit new results, is available at:                      *
 *                                                                       *
 *          http://www.nas.nasa.gov/Software/NPB/                        *
 *                                                                       *
 *   Send comments or suggestions to  npb@nas.nasa.gov                   *
 *                                                                       *
 *         NAS Parallel Benchmarks Group                                 *
 *         NASA Ames Research Center                                     *
 *         Mail Stop: T27A-1                                             *
 *         Moffett Field, CA   94035-1000                                *
 *                                                                       *
 *         E-mail:  npb@nas.nasa.gov                                     *
 *         Fax:     (650) 604-3957                                       *
 *                                                                       *
 *************************************************************************
 *                                                                       *
 *   Author: M. Yarrow                                                   *
 *           H. Jin                                                      *
 *                                                                       *
 *************************************************************************/

#include "npbparams.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

// #define TIMER_ENABLED
#ifdef _OPENMP
#include <omp.h>
#endif

#define GEM5
#ifdef GEM5
#include <gem5/m5ops.h>
#endif

#ifdef TIMER_ENABLED
#include <time.h>
#endif

// #define DO_VERIFY

/*****************************************************************/
/* For serial IS, buckets are not really req'd to solve NPB1 IS  */
/* spec, but their use on some machines improves performance, on */
/* other machines the use of buckets compromises performance,    */
/* probably because it is extra computation which is not req'd.  */
/* (Note: Mechanism not understood, probably cache related)      */
/* Example:  SP2-66MhzWN:  50% speedup with buckets              */
/* Example:  SGI Indy5000: 50% slowdown with buckets             */
/* Example:  SGI O2000:   400% slowdown with buckets (Wow!)      */
/*****************************************************************/

/* Uncomment below for cyclic schedule */
/*#define SCHED_CYCLIC*/

/******************/
/* default values */
/******************/
#ifndef CLASS
#define CLASS 'S'
#endif

/*************/
/*  CLASS S  */
/*************/
#if CLASS == 'S'
#define TOTAL_KEYS_LOG_2 16
#define MAX_KEY_LOG_2 11
#define NUM_BUCKETS_LOG_2 9
#endif

/*************/
/*  CLASS W  */
/*************/
#if CLASS == 'W'
#define TOTAL_KEYS_LOG_2 20
#define MAX_KEY_LOG_2 16
#define NUM_BUCKETS_LOG_2 10
#endif

/*************/
/*  CLASS A  */
/*************/
#if CLASS == 'A'
#define TOTAL_KEYS_LOG_2 23
#define MAX_KEY_LOG_2 19
#define NUM_BUCKETS_LOG_2 10
#endif

/*************/
/*  CLASS B  */
/*************/
#if CLASS == 'B'
#define TOTAL_KEYS_LOG_2 25
#define MAX_KEY_LOG_2 21
#define NUM_BUCKETS_LOG_2 10
#endif

/*************/
/*  CLASS C  */
/*************/
#if CLASS == 'C'
#define TOTAL_KEYS_LOG_2 27
#define MAX_KEY_LOG_2 23
#define NUM_BUCKETS_LOG_2 10
#endif

/*************/
/*  CLASS D  */
/*************/
#if CLASS == 'D'
#define TOTAL_KEYS_LOG_2 31
#define MAX_KEY_LOG_2 27
#define NUM_BUCKETS_LOG_2 10
#endif

/*************/
/*  CLASS E  */
/*************/
#if CLASS == 'E'
#define TOTAL_KEYS_LOG_2 35
#define MAX_KEY_LOG_2 31
#define NUM_BUCKETS_LOG_2 10
#endif

#if (CLASS == 'D' || CLASS == 'E')
#define TOTAL_KEYS (1L << TOTAL_KEYS_LOG_2)
#define TOTAL_KS1 (1 << (TOTAL_KEYS_LOG_2 - 8))
#define TOTAL_KS2 (1 << 8)
#define MAX_KEY (1L << MAX_KEY_LOG_2)
#else
#define TOTAL_KEYS (1 << TOTAL_KEYS_LOG_2)
#define TOTAL_KS1 TOTAL_KEYS
#define TOTAL_KS2 1
#define MAX_KEY (1 << MAX_KEY_LOG_2)
#endif
#define NUM_BUCKETS (1 << NUM_BUCKETS_LOG_2)
#define NUM_KEYS TOTAL_KEYS
#define SIZE_OF_BUFFERS NUM_KEYS

// Changed configuration
// #define MAX_ITERATIONS 10
#define MAX_ITERATIONS 4
#ifdef DO_VERIFY
#define TEST_ARRAY_SIZE 5
#endif
/*************************************/
/* Typedef: if necessary, change the */
/* size of int here by changing the  */
/* int type to, say, long            */
/*************************************/
#if (CLASS == 'D' || CLASS == 'E')
typedef long INT_TYPE;
#else
typedef int INT_TYPE;
#endif

/********************/
/* Some global info */
/********************/
#ifdef DO_VERIFY
INT_TYPE *key_buff_ptr_global; /* used by full_verify to get */
                               /* copies of rank info        */

int passed_verification;
#endif

/************************************/
/* These are the three main arrays. */
/* See SIZE_OF_BUFFERS def above    */
/************************************/
INT_TYPE key_array[SIZE_OF_BUFFERS], key_buff1[MAX_KEY],
    key_buff2[SIZE_OF_BUFFERS], **key_buff1_aptr = NULL;
#ifdef DO_VERIFY
INT_TYPE partial_verify_vals[TEST_ARRAY_SIZE];
#endif

INT_TYPE **bucket_size, bucket_ptrs[NUM_BUCKETS];
#pragma omp threadprivate(bucket_ptrs)

#ifdef DO_VERIFY
/**********************/
/* Partial verif info */
/**********************/
INT_TYPE test_index_array[TEST_ARRAY_SIZE], test_rank_array[TEST_ARRAY_SIZE];

int S_test_index_array[TEST_ARRAY_SIZE] = {48427, 17148, 23627, 62548, 4431},
    S_test_rank_array[TEST_ARRAY_SIZE] = {0, 18, 346, 64917, 65463},

    W_test_index_array[TEST_ARRAY_SIZE] = {357773, 934767, 875723, 898999,
                                           404505},
    W_test_rank_array[TEST_ARRAY_SIZE] = {1249, 11698, 1039987, 1043896,
                                          1048018},

    A_test_index_array[TEST_ARRAY_SIZE] = {2112377, 662041, 5336171, 3642833,
                                           4250760},
    A_test_rank_array[TEST_ARRAY_SIZE] = {104, 17523, 123928, 8288932, 8388264},

    B_test_index_array[TEST_ARRAY_SIZE] = {41869, 812306, 5102857, 18232239,
                                           26860214},
    B_test_rank_array[TEST_ARRAY_SIZE] = {33422937, 10244, 59149, 33135281, 99},

    C_test_index_array[TEST_ARRAY_SIZE] = {44172927, 72999161, 74326391,
                                           129606274, 21736814},
    C_test_rank_array[TEST_ARRAY_SIZE] = {61147, 882988, 266290, 133997595,
                                          133525895};

long D_test_index_array[TEST_ARRAY_SIZE] = {1317351170, 995930646, 1157283250,
                                            1503301535, 1453734525},
     D_test_rank_array[TEST_ARRAY_SIZE] = {1, 36538729, 1978098519, 2145192618,
                                           2147425337},

     E_test_index_array[TEST_ARRAY_SIZE] = {21492309536L, 24606226181L,
                                            12608530949L, 4065943607L,
                                            3324513396L},
     E_test_rank_array[TEST_ARRAY_SIZE] = {3L, 27580354L, 3248475153L,
                                           30048754302L, 31485259697L};
#endif

/***********************/
/* function prototypes */
/***********************/
double randlc(double *X, double *A);

#ifdef DO_VERIFY
void full_verify(void);
#endif

/*
 *    FUNCTION RANDLC (X, A)
 *
 *  This routine returns a uniform pseudorandom double precision number in the
 *  range (0, 1) by using the linear congruential generator
 *
 *  x_{k+1} = a x_k  (mod 2^46)
 *
 *  where 0 < x_k < 2^46 and 0 < a < 2^46.  This scheme generates 2^44 numbers
 *  before repeating.  The argument A is the same as 'a' in the above formula,
 *  and X is the same as x_0.  A and X must be odd double precision integers
 *  in the range (1, 2^46).  The returned value RANDLC is normalized to be
 *  between 0 and 1, i.e. RANDLC = 2^(-46) * x_1.  X is updated to contain
 *  the new seed x_1, so that subsequent calls to RANDLC using the same
 *  arguments will generate a continuous sequence.
 *
 *  This routine should produce the same results on any computer with at least
 *  48 mantissa bits in double precision floating point data.  On Cray systems,
 *  double precision should be disabled.
 *
 *  David H. Bailey     October 26, 1990
 *
 *     IMPLICIT DOUBLE PRECISION (A-H, O-Z)
 *     SAVE KS, R23, R46, T23, T46
 *     DATA KS/0/
 *
 *  If this is the first call to RANDLC, compute R23 = 2 ^ -23, R46 = 2 ^ -46,
 *  T23 = 2 ^ 23, and T46 = 2 ^ 46.  These are computed in loops, rather than
 *  by merely using the ** operator, in order to insure that the results are
 *  exact on all systems.  This code assumes that 0.5D0 is represented exactly.
 */

/*****************************************************************/
/*************           R  A  N  D  L  C             ************/
/*************                                        ************/
/*************    portable random number generator    ************/
/*****************************************************************/

static int KS = 0;
static double R23, R46, T23, T46;
#pragma omp threadprivate(KS, R23, R46, T23, T46)

double randlc(double *X, double *A) {
    double T1, T2, T3, T4;
    double A1;
    double A2;
    double X1;
    double X2;
    double Z;
    int i, j;

    if (KS == 0) {
        R23 = 1.0;
        R46 = 1.0;
        T23 = 1.0;
        T46 = 1.0;

        for (i = 1; i <= 23; i++) {
            R23 = 0.50 * R23;
            T23 = 2.0 * T23;
        }
        for (i = 1; i <= 46; i++) {
            R46 = 0.50 * R46;
            T46 = 2.0 * T46;
        }
        KS = 1;
    }

    /*  Break A into two parts such that A = 2^23 * A1 + A2 and set X = N.  */

    T1 = R23 * *A;
    j = T1;
    A1 = j;
    A2 = *A - T23 * A1;

    /*  Break X into two parts such that X = 2^23 * X1 + X2, compute
      Z = A1 * X2 + A2 * X1  (mod 2^23), and then
      X = 2^23 * Z + A2 * X2  (mod 2^46).                            */

    T1 = R23 * *X;
    j = T1;
    X1 = j;
    X2 = *X - T23 * X1;
    T1 = A1 * X2 + A2 * X1;

    j = R23 * T1;
    T2 = j;
    Z = T1 - T23 * T2;
    T3 = T23 * Z + A2 * X2;
    j = R46 * T3;
    T4 = j;
    *X = T3 - T46 * T4;
    return (R46 * *X);
}

/*****************************************************************/
/************   F  I  N  D  _  M  Y  _  S  E  E  D    ************/
/************                                         ************/
/************ returns parallel random number seq seed ************/
/*****************************************************************/

/*
 * Create a random number sequence of total length nn residing
 * on np number of processors.  Each processor will therefore have a
 * subsequence of length nn/np.  This routine returns that random
 * number which is the first random number for the subsequence belonging
 * to processor rank kn, and which is used as seed for proc kn ran # gen.
 */

double find_my_seed(int kn,   /* my processor rank, 0<=kn<=num procs */
                    int np,   /* np = num procs                      */
                    long nn,  /* total num of ran numbers, all procs */
                    double s, /* Ran num seed, for ex.: 314159265.00 */
                    double a) /* Ran num gen mult, try 1220703125.00 */
{

    double t1, t2;
    long mq, nq, kk, ik;

    if (kn == 0)
        return s;

    mq = (nn / 4 + np - 1) / np;
    nq = mq * 4 * kn; /* number of rans to be skipped */

    t1 = s;
    t2 = a;
    kk = nq;
    while (kk > 1) {
        ik = kk / 2;
        if (2 * ik == kk) {
            (void)randlc(&t2, &t2);
            kk = ik;
        } else {
            (void)randlc(&t1, &t2);
            kk = kk - 1;
        }
    }
    (void)randlc(&t1, &t2);

    return (t1);
}

/*****************************************************************/
/*************      C  R  E  A  T  E  _  S  E  Q      ************/
/*****************************************************************/

void create_seq(double seed, double a) {
    double x, s;
    INT_TYPE i, k;

#pragma omp parallel private(x, s, i, k)
    {
        INT_TYPE k1, k2;
        double an = a;
        int myid = 0, num_threads = 1;
        INT_TYPE mq;

#ifdef _OPENMP
        myid = omp_get_thread_num();
        num_threads = omp_get_num_threads();
#endif

        mq = (NUM_KEYS + num_threads - 1) / num_threads;
        k1 = mq * myid;
        k2 = k1 + mq;
        if (k2 > NUM_KEYS)
            k2 = NUM_KEYS;

        KS = 0;
        s = find_my_seed(myid, num_threads, (long)4 * NUM_KEYS, seed, an);

        k = MAX_KEY / 4;

        for (i = k1; i < k2; i++) {
            x = randlc(&s, &an);
            x += randlc(&s, &an);
            x += randlc(&s, &an);
            x += randlc(&s, &an);

            key_array[i] = k * x;
        }
    } /*omp parallel*/
}

/*****************************************************************/
/*****************    Allocate Working Buffer     ****************/
/*****************************************************************/
void *alloc_mem(size_t size) {
    void *p;

    p = (void *)malloc(size);
    if (!p) {
        perror("Memory allocation error");
        exit(1);
    }
    return p;
}

void alloc_key_buff(void) {
    INT_TYPE i;
    int num_threads = 1;

#ifdef _OPENMP
    num_threads = omp_get_max_threads();
#endif

    bucket_size = (INT_TYPE **)alloc_mem(sizeof(INT_TYPE *) * num_threads);

    for (i = 0; i < num_threads; i++) {
        bucket_size[i] = (INT_TYPE *)alloc_mem(sizeof(INT_TYPE) * NUM_BUCKETS);
    }

#pragma omp parallel for
    for (i = 0; i < NUM_KEYS; i++)
        key_buff2[i] = 0;
}

/*****************************************************************/
/*************    F  U  L  L  _  V  E  R  I  F  Y     ************/
/*****************************************************************/

#ifdef DO_VERIFY
void full_verify(void) {
    INT_TYPE i, j;
    INT_TYPE k, k1, k2;

    /*  Now, finally, sort the keys:  */

    /*  Copy keys into work array; keys in key_array will be reassigned. */

    /* Buckets are already sorted.  Sorting keys within each bucket */
#ifdef SCHED_CYCLIC
#pragma omp parallel for private(i, j, k, k1) schedule(static, 1)
#else
#pragma omp parallel for private(i, j, k, k1) schedule(dynamic)
#endif
    for (j = 0; j < NUM_BUCKETS; j++) {

        k1 = (j > 0) ? bucket_ptrs[j - 1] : 0;
        for (i = k1; i < bucket_ptrs[j]; i++) {
            k = --key_buff_ptr_global[key_buff2[i]];
            key_array[k] = key_buff2[i];
        }
    }

    /*  Confirm keys correctly sorted: count incorrectly sorted keys, if any */

    j = 0;
#pragma omp parallel for reduction(+ : j)
    for (i = 1; i < NUM_KEYS; i++)
        if (key_array[i - 1] > key_array[i])
            j++;

    if (j != 0)
        std::cout << "Full_verify: number of keys out of sort: %ld\n", (long)j);
    else
        passed_verification++;
}
#endif

/*****************************************************************/
/*************             R  A  N  K             ****************/
/*****************************************************************/

void rank(int iteration) {

    INT_TYPE i, k;
    INT_TYPE *key_buff_ptr, *key_buff_ptr2;

    int shift = MAX_KEY_LOG_2 - NUM_BUCKETS_LOG_2;
    INT_TYPE num_bucket_keys = (1L << shift);

    key_array[iteration] = iteration;
    key_array[iteration + MAX_ITERATIONS] = MAX_KEY - iteration;

#ifdef DO_VERIFY
    /*  Determine where the partial verify test keys are, load into  */
    /*  top of array bucket_size                                     */
    for (i = 0; i < TEST_ARRAY_SIZE; i++)
        partial_verify_vals[i] = key_array[test_index_array[i]];
#endif

    /*  Setup pointers to key buffers  */
    key_buff_ptr2 = key_buff2;
    key_buff_ptr = key_buff1;

#pragma omp parallel private(i, k)
    {
        INT_TYPE *work_buff, m, k1, k2;
        int myid = 0, num_threads = 1;

#ifdef _OPENMP
        myid = omp_get_thread_num();
        num_threads = omp_get_num_threads();
#endif

        /*  Bucket sort is known to improve cache performance on some   */
        /*  cache based systems.  But the actual performance may depend */
        /*  on cache size, problem size. */
        work_buff = bucket_size[myid];

        /*  Initialize */
        for (i = 0; i < NUM_BUCKETS; i++)
            work_buff[i] = 0;

            /*  Determine the number of keys in each bucket */
            // LOOP 1
#pragma omp for schedule(static)
        for (i = 0; i < NUM_KEYS; i++)
            work_buff[key_array[i] >> shift]++;

        /*  Accumulative bucket sizes are the bucket pointers.
        These are global sizes accumulated upon to each bucket */
        bucket_ptrs[0] = 0;
        for (k = 0; k < myid; k++)
            bucket_ptrs[0] += bucket_size[k][0];

        for (i = 1; i < NUM_BUCKETS; i++) {
            bucket_ptrs[i] = bucket_ptrs[i - 1];
            for (k = 0; k < myid; k++)
                bucket_ptrs[i] += bucket_size[k][i];
            for (k = myid; k < num_threads; k++)
                bucket_ptrs[i] += bucket_size[k][i - 1];
        }

        /*  Sort into appropriate bucket */
        // LOOP 2
#pragma omp for schedule(static)
        for (i = 0; i < NUM_KEYS; i++) {
            k = key_array[i];
            key_buff2[bucket_ptrs[k >> shift]++] = k;
        }

        /*  The bucket pointers now point to the final accumulated sizes */
        if (myid < num_threads - 1) {
            for (i = 0; i < NUM_BUCKETS; i++)
                for (k = myid + 1; k < num_threads; k++)
                    bucket_ptrs[i] += bucket_size[k][i];
        }

        /*  Now, buckets are sorted.  We only need to sort keys inside
        each bucket, which can be done in parallel.  Because the distribution
        of the number of keys in the buckets is Gaussian, the use of
        a dynamic schedule should improve load balance, thus, performance     */

#ifdef SCHED_CYCLIC
#pragma omp for schedule(static, 1)
#else
#pragma omp for schedule(dynamic)
#endif
        for (i = 0; i < NUM_BUCKETS; i++) {

            /*  Clear the work array section associated with each bucket */
            k1 = i * num_bucket_keys;
            k2 = k1 + num_bucket_keys;
            for (k = k1; k < k2; k++)
                key_buff_ptr[k] = 0;

            /*  Ranking of all keys occurs in this section:                 */

            /*  In this section, the keys themselves are used as their
            own indexes to determine how many of each there are: their
            individual population                                       */
            m = (i > 0) ? bucket_ptrs[i - 1] : 0;
            // LOOP3
            for (k = m; k < bucket_ptrs[i]; k++)
                key_buff_ptr[key_buff_ptr2[k]]++; /* Now they have individual key   */
                                                  /* population                     */

            /*  To obtain ranks of each key, successively add the individual key
            population, not forgetting to add m, the total of lesser keys,
            to the first key population */
            key_buff_ptr[k1] += m;
            for (k = k1 + 1; k < k2; k++)
                key_buff_ptr[k] += key_buff_ptr[k - 1];
        }

    } /*omp parallel*/

#ifdef DO_VERIFY
    /* This is the partial verify test section */
    /* Observe that test_rank_array vals are   */
    /* shifted differently for different cases */
    for (i = 0; i < TEST_ARRAY_SIZE; i++) {
        k = partial_verify_vals[i]; /* test vals were put here */
        if (0 < k && k <= NUM_KEYS - 1) {
            INT_TYPE key_rank = key_buff_ptr[k - 1];
            INT_TYPE test_rank = test_rank_array[i];
            int failed = 0;

            switch (CLASS) {
            case 'S':
                if (i <= 2)
                    test_rank += iteration;
                else
                    test_rank -= iteration;
                break;
            case 'W':
                if (i < 2)
                    test_rank += iteration - 2;
                else
                    test_rank -= iteration;
                break;
            case 'A':
                if (i <= 2)
                    test_rank += iteration - 1;
                else
                    test_rank -= iteration - 1;
                break;
            case 'B':
                if (i == 1 || i == 2 || i == 4)
                    test_rank += iteration;
                else
                    test_rank -= iteration;
                break;
            case 'C':
                if (i <= 2)
                    test_rank += iteration;
                else
                    test_rank -= iteration;
                break;
            case 'D':
                if (i < 2)
                    test_rank += iteration;
                else
                    test_rank -= iteration;
                break;
            case 'E':
                if (i < 2)
                    test_rank += iteration - 2;
                else if (i == 2) {
                    test_rank += iteration - 2;
                    if (iteration > 4)
                        test_rank -= 2;
                    else if (iteration > 2)
                        test_rank -= 1;
                } else
                    test_rank -= iteration - 2;
                break;
            }
            if (key_rank != test_rank)
                failed = 1;
            else
                passed_verification++;
            if (failed == 1) {
                std::cout << "Failed partial verification: iteration " << iteration << ", test key " << i << std::endl;
            }
        }
    }

    /*  Make copies of rank info for use by full_verify: these variables
      in rank are local; making them global slows down the code, probably
      since they cannot be made register by compiler                        */

    if (iteration == MAX_ITERATIONS)
        key_buff_ptr_global = key_buff_ptr;
#endif
}

/*****************************************************************/
/*************             M  A  I  N             ****************/
/*****************************************************************/

int main(int argc, char **argv) {

    std::cout << "Hello Gem5 (suggested by Luke)" << std::endl;

    int i, iteration;

#ifdef DO_VERIFY
    /*  Initialize the verification arrays if a valid class */
    for (i = 0; i < TEST_ARRAY_SIZE; i++)
        switch (CLASS) {
        case 'S':
            test_index_array[i] = S_test_index_array[i];
            test_rank_array[i] = S_test_rank_array[i];
            break;
        case 'A':
            test_index_array[i] = A_test_index_array[i];
            test_rank_array[i] = A_test_rank_array[i];
            break;
        case 'W':
            test_index_array[i] = W_test_index_array[i];
            test_rank_array[i] = W_test_rank_array[i];
            break;
        case 'B':
            test_index_array[i] = B_test_index_array[i];
            test_rank_array[i] = B_test_rank_array[i];
            break;
        case 'C':
            test_index_array[i] = C_test_index_array[i];
            test_rank_array[i] = C_test_rank_array[i];
            break;
        case 'D':
            test_index_array[i] = D_test_index_array[i];
            test_rank_array[i] = D_test_rank_array[i];
            break;
        case 'E':
            test_index_array[i] = E_test_index_array[i];
            test_rank_array[i] = E_test_rank_array[i];
            break;
        };
#endif

    /*  Printout initial NPB info */
    std::cout << "NAS Parallel Benchmarks (NPB3.4-OMP) - IS Benchmark" << std::endl;
    std::cout << " Size:  " << TOTAL_KEYS << "  (class " << CLASS << ")" << std::endl;
    std::cout << " Iterations:  " << MAX_ITERATIONS << std::endl;
#ifdef _OPENMP
    std::cout << " Number of available threads: " << omp_get_max_threads() << std::endl;
#endif

    /*  Generate random number sequence and subsequent keys on all procs */
    create_seq(314159265.00,   /* Random number gen seed */
               1220703125.00); /* Random number gen mult */

    alloc_key_buff();

#ifdef GEM5
    m5_checkpoint(0, 0);
#endif

    /*  Do one interation for free (i.e., untimed) to guarantee initialization of
      all data and code pages and respective tables */
    std::cout << "Warmup started!" << std::endl;
    rank(1);

#ifdef DO_VERIFY
    /*  Start verification counter */
    passed_verification = 0;
#endif

    if (CLASS != 'S')
        std::cout << "\n   iteration" << std::endl;

/*  This is the main iteration */
#ifdef GEM5
    std::cout << "ROI Begin!!!" << std::endl;
    m5_work_begin(0, 0);
    m5_reset_stats(0, 0);
#endif
    for (iteration = 1; iteration <= MAX_ITERATIONS; iteration++) {
#ifdef TIMER_ENABLED
        clock_t start, end;
        start = clock();
#endif
        if (CLASS != 'S')
            std::cout << "       " << iteration << std::endl;
        rank(iteration);
#ifdef TIMER_ENABLED
        double elapsed_time = ((double)(clock() - start)) / CLOCKS_PER_SEC;
        std::cout << elapsed_time << " second" << std::endl;
#endif
    }
#ifdef GEM5
    m5_dump_stats(0, 0);
    m5_work_end(0, 0);
    std::cout << "ROI End!!!" << std::endl;
#endif

#ifdef DO_VERIFY
    full_verify();
    if (passed_verification == 5 * MAX_ITERATIONS + 1)
        std::cout << "successfull" << std::endl;
    else
        std::cout << "failed" << std::endl;
#endif

    std::cout << "finished" << std::endl;

    return 1;
    /**************************/
} /*  E N D  P R O G R A M  */
/**************************/
