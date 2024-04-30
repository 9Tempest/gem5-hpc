! CLASS = W
!  
!  
!  This file is generated automatically by the setparams utility.
!  It sets the number of processors and the class of the NPB
!  in this directory. Do not modify it by hand.
!  
        integer nx_default, ny_default, nz_default
        parameter (nx_default=128, ny_default=128, nz_default=128)
        integer nit_default, lm, lt_default
        parameter (nit_default=4, lm = 7, lt_default=7)
        integer debug_default
        parameter (debug_default=0)
        integer ndim1, ndim2, ndim3
        parameter (ndim1 = 7, ndim2 = 7, ndim3 = 7)
        integer kind2
        parameter (kind2=4)
        logical  convertdouble
        parameter (convertdouble = .false.)
        character compiletime*11
        parameter (compiletime='26 Apr 2024')
        character npbversion*5
        parameter (npbversion='3.4.2')
        character cs1*8
        parameter (cs1='gfortran')
        character cs2*5
        parameter (cs2='$(FC)')
        character cs3*6
        parameter (cs3='(none)')
        character cs4*6
        parameter (cs4='(none)')
        character cs5*12
        parameter (cs5='-O3 -fopenmp')
        character cs6*9
        parameter (cs6='$(FFLAGS)')
        character cs7*6
        parameter (cs7='randi8')
