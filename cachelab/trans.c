/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    REQUIRES(M > 0);
    REQUIRES(N > 0);
    // Use different blocks for different matrices

    // Variable block sizes
    int a, b, c,tmp;

    a = 8;
    b = 4;
    c = 16;

    switch(M) {
        // 32 x 32

        case 32:

            for (int i = 0; i < N; i += a) {
                for (int j = 0; j < M; j += a) {
                    for (int k = i; k < i + a; k++) {
                        for (int l = j; l < j + a; l++) {
                            // Is on diagonal?
                            if (k == l) {
                                // Store local for out of loop
                                tmp = A[k][l];
                                }
                            else {
                                // Switch
                                B[l][k] = A[k][l];
                            }
                        }
                        if (i == j) B[k][k] = tmp;
                    }
                }
            }

            break;
        // 64 x 64
        case 64:
            for (int i = 0; i < N; i += b) {
                for (int j = 0; j < M; j += b) {
                    for (int k = j; (k < N) &&  k < j + b; k++) {
                        for (int l = i; (l < M) && l < i + b; l++) {
                            if (k == l) {
                                tmp = A[k][l];
                            }
                            else {
                                B[l][k] = A[k][l];
                            }


                        }
                        if (i == j) {
                            B[k][k] = tmp;
                        }
                        }

                }
            }


            break;
        // 61 x 67
        case 61:
            for (int i = 0; i < N; i += c) {
                for (int j = 0; j < M; j += c) {
                   for (int k = i; (k < N) && k < i + c; k++) {
                       for (int l = j; (l < M) && l < j + c; l++) {
                                tmp = A[k][l];
                               B[l][k] = A[k][l];
                       }
                    }
                }
            }
            break;
    }


    ENSURES(is_transpose(M, N, A, B));
}



/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

