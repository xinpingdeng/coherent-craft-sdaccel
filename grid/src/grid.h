/*
******************************************************************************
** GRID CODE HEADER FILE
******************************************************************************
*/
#pragma once

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ap_fixed.h>
#include <ap_int.h>
#include <assert.h>
#include <hls_stream.h>
#include "ap_axi_sdata.h"

#define FLOAT     1
//#define DATA_WIDTH     32     // We use float 32-bits complex numbers
#define DATA_WIDTH     16       // We use ap_fixed 16-bits complex numbers
#define MFFT_SIZE            256
#define COORD_WIDTH    16       // Wider than the required width, but to 2^n
#define BURST_WIDTH         512
#define NSAMP_PER_BURST     (BURST_WIDTH/(2*DATA_WIDTH))

#define MDM                 1024
#define MTIME               256
#define MSAMP_PER_UV_OUT    (MFFT_SIZE*MFFT_SIZE)    // MFFT_SIZE^2
#define MSAMP_PER_UV_IN     4368

#define MUV                 (MDM*MTIME)
#define MBURST_PER_UV_OUT   (MSAMP_PER_UV_OUT/NSAMP_PER_BURST)
#define MBURST_PER_UV_IN    (MSAMP_PER_UV_IN/NSAMP_PER_BURST)

#define INTEGER_WIDTH       (DATA_WIDTH/2)

#if DATA_WIDTH == 32
#define DATA_RANGE          4096
#if FLOAT == 1
typedef float uv_data_t;
#else
typedef int uv_data_t;
#endif

#elif DATA_WIDTH == 16
#define DATA_RANGE          127
#if FLOAT == 1
typedef ap_fixed<DATA_WIDTH, INTEGER_WIDTH> uv_data_t; // The size of this should be DATA_WIDTH
#else
typedef ap_int<DATA_WIDTH> uv_data_t; // The size of this should be DATA_WIDTH
#endif
#endif

typedef ap_uint<2*DATA_WIDTH> uv_t; // Use for the top-level interface
typedef ap_uint<COORD_WIDTH>  coord_t; // Use inside the kernel

typedef struct burst_coord{
  coord_t data[NSAMP_PER_BURST];
}burst_coord; 

#define MAX_PALTFORMS       16
#define MAX_DEVICES         16
#define PARAM_VALUE_SIZE    1024
#define MEM_ALIGNMENT       4096  // memory alignment on device
#define LINE_LENGTH         4096

typedef struct burst_uv{
  uv_t data[NSAMP_PER_BURST];
}burst_uv; // The size of this should be 512; BURST_DATA_WIDTH

typedef hls::stream<burst_uv> fifo_uv;

typedef ap_axiu<BURST_WIDTH, 0, 0, 0> stream_t; 
typedef hls::stream<stream_t> stream_uv;

int grid(
	 uv_data_t *in,
	 coord_t *coord,
	 uv_data_t *out,
	 int nuv_per_cu,
         int nsamp_per_uv_in,
         int nsamp_per_uv_out
	 );

int read_coord(
	       char *fname,
	       int flen,
	       int *coord);
