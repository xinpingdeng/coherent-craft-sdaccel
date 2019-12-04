#include "grid.h"

// Order is assumed to be DM-TIME-UV
extern "C" {  
  void knl_grid(
		const burst_uv *in,
		const burst_coord *coord,
		burst_uv *out,
		int nuv_per_cu
		);
}

void knl_grid(
	      const burst_uv *in,
	      const burst_coord *coord,
	      burst_uv *out,
	      int nuv_per_cu
	      )
{
#pragma HLS INTERFACE m_axi port = in    offset = slave bundle = gmem0 
#pragma HLS INTERFACE m_axi port = coord offset = slave bundle = gmem1 
#pragma HLS INTERFACE m_axi port = out   offset = slave bundle = gmem2 max_write_burst_length=64

#pragma HLS INTERFACE s_axilite port = in         bundle = control
#pragma HLS INTERFACE s_axilite port = coord      bundle = control
#pragma HLS INTERFACE s_axilite port = out        bundle = control
#pragma HLS INTERFACE s_axilite port = nuv_per_cu bundle = control
  
#pragma HLS INTERFACE s_axilite port = return bundle = control

#pragma HLS DATA_PACK variable = in
#pragma HLS DATA_PACK variable = out
#pragma HLS DATA_PACK variable = coord
  
  burst_uv out_burst;
  uv_t in_tmp[2*NDATA_PER_BURST];
  coord_t2 coord_burst[NSAMP_PER_UV_OUT];
  
#pragma HLS ARRAY_PARTITION variable = in_tmp complete
#pragma HLS ARRAY_PARTITION variable = coord_burst cyclic factor=16

  int i;
  int j;
  int m;
  int loc_coord;
  int loc_in;
  int loc_unroll;
  int loc_in_burst;
  int loc_out_burst;
  
#pragma HLS DEPENDENCE variable = loc_in_burst intra true 
#pragma HLS DEPENDENCE variable = loc_in_burst inter true
  
  // Burst in all coordinate
  // It is built with index counting from 1, 0 means there is no data for output
 LOOP_BURST_COORD:
  for(i = 0; i < NBURST_PER_UV_OUT; i++){
#pragma HLS PIPELINE
    for(j = 0; j < NSAMP_PER_BURST; j++){
#pragma HLS UNROLL
      loc_coord = i*NSAMP_PER_BURST+j;
      coord_burst[loc_coord] = coord[i].data[j];
    }
  }
  
 LOOP_SET_UV_TOP:
  for(i = 0; i < nuv_per_cu; i++){
    // Maximally two input blocks will cover one output block
    // Read in first two blocks of each input UV
    // Put two blocks into a single array to reduce the source usage
    
    loc_in_burst = i*NBURST_PER_UV_IN;
    for(j = 0; j < NSAMP_PER_BURST; j++){
#pragma HLS UNROLL
      in_tmp[2*j]   = in[loc_in_burst].data[2*j];
      in_tmp[2*j+1] = in[loc_in_burst].data[2*j+1];
    }
    loc_in_burst = i*NBURST_PER_UV_IN+1;
    for(j = 0; j < NSAMP_PER_BURST; j++){
#pragma HLS UNROLL
      in_tmp[NDATA_PER_BURST+2*j]   = in[loc_in_burst].data[2*j];
      in_tmp[NDATA_PER_BURST+2*j+1] = in[loc_in_burst].data[2*j+1];
    }
    
  LOOP_SET_UV:
    for(j = 0; j < NBURST_PER_UV_OUT; j++){
#pragma HLS PIPELINE  
      // Get one output block ready in one clock cycle
      for(m = 0; m < NSAMP_PER_BURST; m++){
#pragma HLS UNROLL
	// Default output block elements be 0
	out_burst.data[2*m]   = 0;
	out_burst.data[2*m+1] = 0;

	// If there is data
	loc_coord = j*NSAMP_PER_BURST+m;
	loc_in = coord_burst[loc_coord];
	if(loc_in != 0){ 
	  loc_unroll            = (loc_in -1 - (loc_in_burst - 1)*NSAMP_PER_BURST)%(2*NSAMP_PER_BURST);
       	  out_burst.data[2*m]   = in_tmp[2*loc_unroll];
	  out_burst.data[2*m+1] = in_tmp[2*loc_unroll+1];
	}
      }
      loc_out_burst      = i*NBURST_PER_UV_OUT+j;
      out[loc_out_burst] = out_burst;
      
    LOOP_GET_LOC_IN:
      for(m = 0; m < NSAMP_PER_BURST; m++){
	loc_coord = j*NSAMP_PER_BURST+NSAMP_PER_BURST-m-1;
	loc_in = coord_burst[loc_coord];
	if(loc_in != 0){
	  break;
	}
      }
      
      if((loc_in/NSAMP_PER_BURST)>=loc_in_burst){
	loc_in_burst  = loc_in/NSAMP_PER_BURST+1;
	
	for(m = 0; m < NSAMP_PER_BURST; m++){
#pragma HLS UNROLL
	  // Shift the array with one block size
	  in_tmp[2*m]   = in_tmp[NDATA_PER_BURST+2*m];
	  in_tmp[2*m+1] = in_tmp[NDATA_PER_BURST+2*m+1];
	}
	
	for(m = 0; m < NSAMP_PER_BURST; m++){
#pragma HLS UNROLL
	  // Put the new block into the array 
	  in_tmp[NDATA_PER_BURST+2*m]   = in[loc_in_burst].data[2*m];
	  in_tmp[NDATA_PER_BURST+2*m+1] = in[loc_in_burst].data[2*m+1];
	}
      }
    }    
  }
}
