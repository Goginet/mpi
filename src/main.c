// Copyright 2017 Georgy Shapchits <gogi.soft.gm@gmail.com> legal

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include "mpi.h"
#include "matrix.c"

#define TEST_MAX_SIZE 1000000
#define TEST_STEP 1000

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
int master(int **matrix_a, int **matrix_b, int process_count, int size)
{
    int *recv_buffer;
    int code;

    // Send matrix size to workers
    for (int j = 0; j < process_count; j++)
    {
        MPI_Send(&size, 1, MPI_INT, j + 1, 0, MPI_COMM_WORLD);
    }

    // Send rows and columns to workers
    for (int i = 0; i < (int)(size / process_count); i++)
    {
        for (int j = 0; j < process_count; j++)
        {
            int index = i + j * (size / process_count);
            int *column = (int *)malloc(size * sizeof(int));

            for (int k = 0; k < size; k++)
            {
                column[k] = matrix_b[k][index];
            }

            // Send row to worker
            MPI_Send(matrix_a[index], size, MPI_INT, j + 1, 0, MPI_COMM_WORLD);
            // Send columm to worker
            MPI_Send(column, size, MPI_INT, j + 1, 0, MPI_COMM_WORLD);

            free(column);
        }
    }

    return 0;
}

// Calculate part of matrix
int worker(int ***matrix_part, int *part_size, int rank, int process_count)
{
    int **rows, **columns;
    int code, count, size;
    MPI_Status status;

    get_message(0, 0, &size, 1);
    *part_size = size;
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

    code = create_matrix(matrix_part, count, size);
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
                (*matrix_part)[element_row_num][element_column_num] = element;
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
int merge(int ***matrix_c, int **matrix_part, int rank, int rows, int process_count)
{
    int code = 0;

    // Send result from workers
    if (rank != 0)
    {
        for (int j = 0; j < rows / process_count; j++)
        {
            MPI_Send(matrix_part[j], rows, MPI_INT, 0, 2, MPI_COMM_WORLD);
        }
    }

    // Collect results in master
    if (rank == 0)
    {
        // Alocate memory for result matrix
        code = create_matrix(matrix_c, rows, rows);
        if (code != 0)
        {
            return code;
        }

        // Collect result from workers and put it into workers
        for (int i = 0; i < process_count; i++)
        {
            for (int j = 0; j < rows / process_count; j++)
            {
                get_message(i + 1, 2, (*matrix_c)[(i * rows / process_count) + j], rows);
            }
        }
    }
}

int run(int count_generate)
{
    int **matrix_a, **matrix_b, **matrix_c;
    int **matrix_part;
    int matrix_size, code;
    int process_rank, process_count, workers_count;
    double t_load_start, t_load_end, t_broadcast_start, t_broadcast_end, t_end;

    MPI_Comm_size(MPI_COMM_WORLD, &process_count);
    MPI_Comm_rank(MPI_COMM_WORLD, &process_rank);

    workers_count = process_count - 1;

    if (process_rank == 0)
    {
        printf("\n-------------Start test %d matrix size--------------\n", count_generate);
        printf("Start Load data!\n");
        t_load_start = MPI_Wtime();

        if (count_generate != 0)
        {
            code = generate_data(&matrix_a, &matrix_b, count_generate);
            matrix_size = count_generate;
        }
        else
        {
            code = load_data(&matrix_a, &matrix_b, &matrix_size);
        }

        if (code != 0 || matrix_size % workers_count != 0)
        {
            printf("Error load data!\n");
            return code;
        }
        t_load_end = MPI_Wtime();
        printf("End Load data. Time: %f\n", t_load_end - t_load_start);

        t_broadcast_start = MPI_Wtime();
        master(matrix_a, matrix_b, workers_count, matrix_size);
        t_broadcast_end = MPI_Wtime();
        printf("End broadcast send to workers! Time: %f\n", t_broadcast_end - t_broadcast_start);

        free_matrix(matrix_a, matrix_size, matrix_size);
        free_matrix(matrix_b, matrix_size, matrix_size);
    }
    else
    {
        worker(&matrix_part, &matrix_size, process_rank, workers_count);
    }

    merge(&matrix_c, matrix_part, process_rank, matrix_size, workers_count);

    if (process_rank == 0)
    {
        t_end = MPI_Wtime();

        double load_time = t_load_end - t_load_start;
        double broadcast_time = t_broadcast_end - t_broadcast_start;
        double merge_time = t_end - t_broadcast_end;
        double total_time = t_end - t_load_end;

        printf("Matrix multiply END!!!!!!!!!\n");
        printf("Load: %f\n", load_time);
        printf("BroadcastSend: %f\n", broadcast_time);
        printf("Work: %f\n", merge_time);
        printf("Total time : %f\n", total_time);
        printf("Write result to file\n");
        printf("-------------End test %d matrix size--------------\n", count_generate);

#ifndef TEST_MAX_SIZE
        code = write_matrix(matrix_c, RESULT_MATRIX_PATH, matrix_size, matrix_size);
#endif
        free_matrix(matrix_c, matrix_size, matrix_size);
    }

    return 0;
}

int main(int argc, char **argv)
{
    int code = 0;
    MPI_Init(&argc, &argv);

#ifdef TEST_MAX_SIZE
    for (int size = 1000; size < TEST_MAX_SIZE; size += TEST_STEP)
    {
        code = run(size);
    }
#else
    run(0);
#endif

    MPI_Finalize();

    return code;
}