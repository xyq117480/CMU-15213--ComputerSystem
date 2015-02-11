/* 
 * Name: Aihua Peng
 * Andrew ID: aihuap
 *
 *
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
#include <math.h>
void trans_NxM(int M, int N, int A[N][M], int B[M][N]);
int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * trans_NxM - This is a function to handle NxM matrix, in 2 ways.
 *     If N == M, N must be multiple of 8. It divides matrix into 8x8 block,
 *     which again makes use of 4x4 block to ensure low miss rate.
 *     If N != M, make use of NxN mode to handle the square part.
 */
void trans_NxM(int M, int N, int A[N][M], int B[M][N]){
    int i, j, k,min;
    int a0, a1, a2, a3, a4, a5, a6, a7;
    min = M>N?N:M;
    min = 8*((int)(min/8));
    //Divide matrix into 8x8 block.
    for(i = 0; i < min; i += 8){
        for(j = 0; j < min; j += 8){
            
            //Firstly, focus on the top half of a 8x8 block in A.
            for(k = j; k < j + 4; k++){
                a0 = A[k][i];
                a1 = A[k][i+1];
                a2 = A[k][i+2];
                a3 = A[k][i+3];
                a4 = A[k][i+4];
                a5 = A[k][i+5];
                a6 = A[k][i+6];
                a7 = A[k][i+7];
                
                //Place the top-left correctly.
                B[i][k] = a0;
                B[i+1][k] = a1;
                B[i+2][k] = a2;
                B[i+3][k] = a3;
                
                //Place the top-right temporarily - use B as a 'cache'.
                B[i][k+4] = a4;
                B[i+1][k+4] = a5;
                B[i+2][k+4] = a6;
                B[i+3][k+4] = a7;
            }
        
            //con't.
            for(k = i; k < i + 4; k++){
                //Read temporary guys.
                a0 = B[k][j+4];
                a1 = B[k][j+5];
                a2 = B[k][j+6];
                a3 = B[k][j+7];
                
                //Read left-bottom of A's 8x8 block.
                a4 = A[j+4][k];
                a5 = A[j+5][k];
                a6 = A[j+6][k];
                a7 = A[j+7][k];
                
                //Place them correctly.
                B[k][j+4] = a4;
                B[k][j+5] = a5;
                B[k][j+6] = a6;
                B[k][j+7] = a7;
                B[k+4][j] = a0;
                B[k+4][j+1] = a1;
                B[k+4][j+2] = a2;
                B[k+4][j+3] = a3;
            }
            
            //Handle the remaining guys of that 8x8 block, 
            //just swapping them by diagonal.
            for(k=i+4;k<i+8;k++){
                a0 = A[j+4][k];
                a1 = A[j+5][k];
                a2 = A[j+6][k];
                a3 = A[j+7][k];
                B[k][j+4] = a0;
                B[k][j+5] = a1;
                B[k][j+6] = a2;
                B[k][j+7] = a3;
            }
        }
    }
    
    //If this is not a square matrix, do simple swap for the remaining part.
    k=0;
    if (M!=N) {
        for(i = 0; i < min; i ++)
            for(j = min; j < M; j ++){
                k = A[i][j];
                B[j][i] = k;
            }
        for (i = min; i<N; i++) {
            for (j=0; j<M;j++) {
                k = A[i][j];
                B[j][i] = k;
            }
        }
    }
}




/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    REQUIRES(M > 0);
    REQUIRES(N > 0);

    /* Input your matrix and size */
    trans_NxM(M, N, A, B);
    
    ENSURES(is_transpose(M, N, A, B));
}





/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);
    
    /* Register any additional transpose functions */
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

