#!/bin/bash
# Author: Georgy Shapchits <gogi.soft.gm@gmail.com>

USER=mpi

function run()
{
    mpiexec -n 8 -f machinefile ./mpi_hello
}