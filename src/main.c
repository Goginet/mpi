// Copyright 2017 Georgy Shapchits <gogi.soft.gm@gmail.com> legal

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include "mpi.h"
#include "matrix.c"

#define MATRIX_A_PATH "data/A.csv"
#define MATRIX_B_PATH "data/B.csv"
#define RESULT_MATRIX_PATH "data/C.csv"

// Get message from process
int get_message(int source, int tag, int *buf, int len)
{
    int code;
    MPI_Status status;

    code = MPI_Recv(buf, len, MPI_INT, source, tag, MPI_COMM_WORLD, &status);
    if (code != MPI_SUCCESS)
    {
        return code;
    }

    return 0;
}

// Scalar multiply row * column
int scalar_multiply(int *row, int *column, int size)
{
    int sum = 0;
    for (int i = 0; i < size; i++)
    {
        sum += row[i] * column[i];
    }

    return sum;
}

// It's a magic function)
int get_offset(int count, int iterate, int rank)
{
    rank--;
    int offset = rank - iterate;

    if (offset < 0)
    {
        offset = count + rank - iterate;
    }

    return offset;
}

// Read to matrix and send rows and columns to workers
int master(int process_count, int *rows_result)
{
    int **matrix_a;
    int **matrix_b;
    int **matrix_c;
    int *recv_buffer;
    int rows, columns, code;

    // Get matrix A size from file
    code = matrix_size(MATRIX_A_PATH, &rows, &columns);
    if (code != 0)
    {
        return code;
    }

    *rows_result = rows;

    // Read matrix A from file
    code = read_matrix(&matrix_a, MATRIX_A_PATH, rows, columns);
    if (code != 0)
    {
        return code;
    }

    // Read matrix B from file
    code = read_matrix(&matrix_b, MATRIX_B_PATH, rows, columns);
    if (code != 0)
    {
        free_matrix(matrix_a, rows, columns);
        return code;
    }

    // Send matrix size to workers
    for (int j = 0; j < process_count; j++)
    {
        MPI_Send(&rows, 1, MPI_INT, j + 1, 0, MPI_COMM_WORLD);
    }

    // Send rows and columns to workers
    for (int i = 0; i < (int)(rows / process_count); i++)
    {
        for (int j = 0; j < process_count; j++)
        {
            int index = i + j * (rows / process_count);
            int *column = (int *)malloc(columns * sizeof(int));

            for (int k = 0; k < rows; k++)
            {
                column[k] = matrix_b[k][index];
            }

            // Send row to worker
            MPI_Send(matrix_a[index], columns, MPI_INT, j + 1, 0, MPI_COMM_WORLD);
            // Send columm to worker
            MPI_Send(column, rows, MPI_INT, j + 1, 0, MPI_COMM_WORLD);

            free(column);
        }
    }

    free_matrix(matrix_a, rows, columns);
    free_matrix(matrix_b, rows, columns);

    printf("End initialization!\n");

    return 0;
}

// Calculate part of matrix
int worker(int rank, int process_count, int ***results_worker, int *rows_result)
{
    int **rows, **columns;
    int code, count, size;
    MPI_Status status;

    get_message(0, 0, &size, 1);
    *rows_result = size;
    count = size / process_count;

    code = create_matrix(&rows, count, size);
    if (code != 0)
    {
        return code;
    }

    code = create_matrix(&columns, count, size);
    if (code != 0)
    {
        return code;
    }

    code = create_matrix(results_worker, count, size);
    if (code != 0)
    {
        return code;
    }

    for (int i = 0; i < count; i++)
    {
        code = get_message(0, 0, rows[i], size);
        if (code != 0)
        {
            return code;
        }

        code = get_message(0, 0, columns[i], size);
        if (code != 0)
        {
            return code;
        }
    }

    for (int iterate_num = 0; iterate_num < process_count; iterate_num++)
    {
        // Calculate element
        for (int row = 0; row < count; row++)
        {
            for (int column = 0; column < count; column++)
            {
                int element = scalar_multiply(rows[row], columns[column], size);
                int offset = get_offset(process_count, iterate_num, rank);
                int element_column_num = column + offset * count;
                int element_row_num = row;
                (*results_worker)[element_row_num][element_column_num] = element;
            }
        }

        if (iterate_num + 1 == process_count)
        {
            break;
        }

        // Send to next
        for (int i = 0; i < count; i++)
        {
            if (rank == process_count)
            {
                MPI_Send(columns[i], size, MPI_INT, 1, 1, MPI_COMM_WORLD);
            }
            else
            {
                MPI_Send(columns[i], size, MPI_INT, rank + 1, 1, MPI_COMM_WORLD);
            }
        }

        // Get from previous
        for (int i = 0; i < count; i++)
        {
            if (rank == 1)
            {
                code = get_message(process_count, 1, columns[i], size);
                if (code != 0)
                {
                    return code;
                }
            }
            else
            {
                code = get_message(rank - 1, 1, columns[i], size);
                if (code != 0)
                {
                    return code;
                }
            }
        }
    }
}

// Merge workers results to result_matrix in master
int merge(int rank, int **result_worker, int rows, int count)
{
    int code = 0;
    int **result_matrix;
    int size = rows * rows;

    // Send result from workers
    if (rank != 0)
    {
        for (int j = 0; j < rows / count; j++)
        {
            MPI_Send(result_worker[j], rows, MPI_INT, 0, 2, MPI_COMM_WORLD);
        }
    }

    // Collect results in master
    if (rank == 0)
    {
        // Alocate memory for result matrix
        code = create_matrix(&result_matrix, rows, rows);
        if (code != 0)
        {
            return code;
        }

        // Collect result from workers and put it into workers
        for (int i = 0; i < count; i++)
        {
            for (int j = 0; j < rows / count; j++)
            {
                get_message(i + 1, 2, result_matrix[(i * rows / count) + j], rows);
            }
        }

        printf("Complite!!!\n");
        printf("Write to file!!!\n");

        // Write result mastrix into file
        code = write_matrix(result_matrix, RESULT_MATRIX_PATH, rows, rows);
        free_matrix(result_matrix, rows, rows);
    }
}

int main(int argc, char **argv)
{
    int process_rank, process_count, workers_count;
    int **result;
    int rows;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &process_count);
    MPI_Comm_rank(MPI_COMM_WORLD, &process_rank);

    workers_count = process_count - 1;

    if (process_rank == 0)
    {
        master(workers_count, &rows);
    }
    else
    {
        worker(process_rank, workers_count, &result, &rows);
    }

    merge(process_rank, result, rows, workers_count);

    MPI_Finalize();

    return 0;
}