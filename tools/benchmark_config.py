#!/usr/bin/env python

splash2_list = [
      "radiosity",
      "fmm",
      "barnes",
      "cholesky",
      "raytrace",
      "volrend",
      "ocean_contiguous",
      "ocean_non_contiguous",
      "fft",
      "radix",
      "lu_contiguous",
      "lu_non_contiguous",
      "water-nsquared",
      "water-spatial",
      ]

parsec_list = [
      "canneal",
      "facesim",
      "streamcluster",
      "swaptions",
      "fluidanimate",
      "blackscholes",
      "freqmine",
      "bodytrack",
      "dedup",
      "ferret",
      ]

lite_mode_list = [
      "freqmine",
      "dedup",
      "ferret",
      "bodytrack",
      ]

app_flags_table = {
      "radix"                                      : "-p64 -n1048576",
      "fft"                                        : "-p64 -m20",
      "lu_contiguous"                              : "-p64 -n1024",
      "lu_non_contiguous"                          : "-p64 -n1024",
      "cholesky"                                   : "-p64 inputs/tk29.O",
      "barnes"                                     : "< inputs/input.65536.64",
      "fmm"                                        : "< inputs/input.65536.64",
      "ocean_contiguous"                           : "-p64 -n1026",
      "ocean_non_contiguous"                       : "-p64 -n1026",
      "water-spatial"                              : "< input",
      "water-nsquared"                             : "< input",
      "raytrace"                                   : "-p64 -m64 inputs/car.env",
      "volrend"                                    : "64 inputs/head",
      "radiosity"                                  : "-p 64 -batch -room",
      "blackscholes"                               : "64 in_64K.txt prices.txt",
      "swaptions"                                  : "-ns 64 -sm 40000 -nt 64",
      "fluidanimate"                               : "64 5 in_300K.fluid out.fluid",
      "canneal"                                    : "64 15000 2000 400000.nets 128",
      "streamcluster"                              : "10 20 128 16384 16384 1000 none output.txt 64",
      "freqmine"                                   : "kosarak_990k.dat 790",
      "dedup"                                      : "-c -p -v -t 20 -i media.dat -o output.dat.ddp",
      "ferret"                                     : "corel lsh queries 10 20 15 output.txt",
      "bodytrack"                                  : "sequenceB_4 4 4 4000 5 0 62",
      "facesim"                                    : "-timing -threads 64",
      }

name_table = {
      "radix"                                      : "RADIX",
      "fft"                                        : "FFT",
      "lu_contiguous"                              : "LU-C",
      "lu_non_contiguous"                          : "LU-NC",
      "cholesky"                                   : "CHOLESKY",
      "barnes"                                     : "BARNES",
      "fmm"                                        : "FMM",
      "ocean_contiguous"                           : "OCEAN-C",
      "ocean_non_contiguous"                       : "OCEAN-NC",
      "water-spatial"                              : "WATER-SP",
      "water-nsquared"                             : "WATER-NSQ",
      "raytrace"                                   : "RAYTRACE",
      "volrend"                                    : "VOLREND",
      "radiosity"                                  : "RADIOSITY",
      "blackscholes"                               : "BLACKSCH.",
      "swaptions"                                  : "SWAPTIONS",
      "fluidanimate"                               : "FLUIDANIM.",
      "canneal"                                    : "CANNEAL",
      "streamcluster"                              : "STREAMCLUS.",
      "freqmine"                                   : "FREQMINE",
      "dedup"                                      : "DEDUP",
      "ferret"                                     : "FERRET",
      "bodytrack"                                  : "BODYTRACK",
      "facesim"                                    : "FACESIM",
      }

threads_table = {
      "radix"                                      : 64,
      "fft"                                        : 64,
      "lu_contiguous"                              : 64,
      "lu_non_contiguous"                          : 64,
      "cholesky"                                   : 64,
      "barnes"                                     : 64,
      "fmm"                                        : 64,
      "ocean_contiguous"                           : 64,
      "ocean_non_contiguous"                       : 64,
      "water-spatial"                              : 64,
      "water-nsquared"                             : 64,
      "raytrace"                                   : 64,
      "volrend"                                    : 64,
      "radiosity"                                  : 64,
      "blackscholes"                               : 64,
      "swaptions"                                  : 64,
      "fluidanimate"                               : 64,
      "canneal"                                    : 64,
      "streamcluster"                              : 64,
      "freqmine"                                   : 64,
      "dedup"                                      : 63,
      "ferret"                                     : 63,
      "bodytrack"                                  : 63,
      "facesim"                                    : 64,
      }