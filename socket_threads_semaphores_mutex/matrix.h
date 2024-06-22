#include <stdio.h>
#include <time.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_linalg.h>

void compute_pseudo_inverse(gsl_matrix_complex *A, gsl_matrix_complex *result) {
    int m = A->size1;  // 30 rows
    int n = A->size2;  // 40 columns
    gsl_matrix_complex *Q = gsl_matrix_complex_alloc(m, n);
    gsl_matrix_complex *R = gsl_matrix_complex_alloc(n, n);
    gsl_vector_complex *tau = gsl_vector_complex_alloc(n);


    gsl_linalg_complex_QR_decomp(A, tau);


    for (int i = 0; i < n; i++) { 
        for (int j = i; j < n; j++) {
            if (i < m) { 
                gsl_complex z = gsl_matrix_complex_get(A, i, j);
                gsl_matrix_complex_set(R, i, j, z);
            } else {
                gsl_complex zero = gsl_complex_rect(0.0, 0.0);
                gsl_matrix_complex_set(R, i, j, zero);
            }
        }
    }


    gsl_vector_complex_free(tau);
    gsl_matrix_complex_free(Q);
    gsl_matrix_complex_free(R);
}

double return_result() {
    int m = 30, n = 40;
    gsl_matrix_complex *A = gsl_matrix_complex_alloc(m, n);
    gsl_matrix_complex *pinvA = gsl_matrix_complex_alloc(n, m);

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            gsl_complex z = gsl_complex_rect(i * 0.1, j * 0.1);
            gsl_matrix_complex_set(A, i, j, z);
        }
    }

    clock_t start = clock();
    compute_pseudo_inverse(A, pinvA);
    clock_t end = clock();

    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    gsl_matrix_complex_free(A);
    gsl_matrix_complex_free(pinvA);

    return cpu_time_used;
}







