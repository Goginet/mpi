// Copyright 2017 Georgy Shapchits <gogi.soft.gm@gmail.com> legal

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include "mpi.h"

#define ERROR_OPEN_FILE 1
#define ERROR_ALLOCATE_MEMORY 2
#define ERROR_MATRIX_SIZE 2

#define MATRIX_A_PATH "data/A.csv"
#define MATRIX_B_PATH "data/B.csv"
#define RESULT_MATRIX_PATH "data/C.csv"

void free_matrix(int **matrix, int rows, int columns)
{
    if (matrix == NULL)
    {
        return;
    }

    for (size_t i = 0; i < rows; i++)
    {
        if (matrix[i] == NULL)
        {
            break;
        }
        free(matrix[i]);
    }
    free(matrix);
}

int create_matrix(int ***matrix, int rows, int columns)
{
    *matrix = (int **)malloc(rows * sizeof(int *));
    if (*matrix == NULL)
    {
        return ERROR_ALLOCATE_MEMORY;
    }

    for (int i = 0; i < rows; i++)
    {
        (*matrix)[i] = (int *)malloc(columns * sizeof(int));
        if ((*matrix)[i] == NULL)
        {
            free_matrix(*matrix, rows, columns);
            return ERROR_ALLOCATE_MEMORY;
        }
    }

    return 0;
}

int create_arr(int **arr, int size)
{
    *arr = (int *)malloc(size * sizeof(int));
    if (*arr == NULL)
    {
        return ERROR_ALLOCATE_MEMORY;
    }

    return 0;
}

int read_matrix(int ***matrix, char *fname, int rows, int columns)
{
    FILE *file;
    int temp, code;

    // Alocate memory for matrix
    // TODO: Check errors
    code = create_matrix(matrix, rows, columns);
    if (code != 0)
    {
        return code;
    }

    // Read matrix from file
    file = fopen(fname, "r");
    if (file == NULL)
    {
        return ERROR_OPEN_FILE;
    }

    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < columns; j++)
        {
            fscanf(file, "%i", &temp);
            (*matrix)[i][j] = temp;
        }
    }

    fclose(file);

    return 0;
}

int write_matrix(int **matrix, char *fname, int rows, int columns)
{
    FILE *file;

    // Open file
    file = fopen(fname, "w");
    if (file == NULL)
    {
        return ERROR_OPEN_FILE;
    }

    // Write to file
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < columns; j++)
        {
            fprintf(file, "%d ", matrix[i][j]);
        }
        fprintf(file, "\n");
    }

    fclose(file);
}

int matrix_size(char *fname, int *rows, int *columns)
{
    FILE *file;
    char *line = NULL;
    size_t len;
    int columns_counter = 0, rows_counter = 0;

    file = fopen(fname, "r");
    if (file == NULL)
    {
        printf("Error open file: %s\n", fname);
        return ERROR_OPEN_FILE;
    }

    while (getline(&line, &len, file) != -1)
    {
        columns_counter = 1;
        for (int i = 0; i < len; i++)
        {
            if (line[i] == ' ')
            {
                columns_counter++;
            }
        }
        rows_counter++;
    }

    *rows = rows_counter;
    *columns = columns_counter;

    return 0;
}

// Load data
int load_data(int ***matrix_a, int ***matrix_b, int *size)
{
    int code = 0;
    int rows_a, rows_b, columns_a, columns_b;

    // Get matrix A size from file
    code = matrix_size(MATRIX_A_PATH, &rows_a, &columns_a);
    if (code != 0)
    {
        return code;
    }

    // Get matrix b size from file
    code = matrix_size(MATRIX_B_PATH, &rows_b, &columns_b);
    if (code != 0)
    {
        return code;
    }

    if (rows_a != columns_a || rows_a != columns_b || rows_b != columns_a)
    {
        return ERROR_MATRIX_SIZE;
    }

    // Read matrix A from file
    code = read_matrix(matrix_a, MATRIX_A_PATH, rows_a, columns_a);
    if (code != 0)
    {
        return code;
    }

    // Read matrix B from file
    code = read_matrix(matrix_b, MATRIX_B_PATH, rows_b, columns_b);
    if (code != 0)
    {
        free_matrix(*matrix_a, rows_a, columns_a);
        return code;
    }

    *size = rows_a;

    return 0;
}