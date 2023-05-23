#!/bin/bash

THISFILE=$(python3 -c "import os; print(os.path.realpath('$0'))")
THISPATH=$(dirname $THISFILE)

test_name=$1
shift;;

# PATHS
case "$test_name" in
    (burgers)
        ;;
    (advection)
        ;;
    (calculat_pi)
        ;;
    (kokkos_pi)
        ;;
    (particle_leapfrog)
        ;;
    (particle_tracers)
        ;;
    (particles) 
        ;;
    (poisson) 
        ;;
    (sparse_advection) 
        ;;
    (stochastic_subgrid) 
        ;;
    (*)
        ;;
esac
