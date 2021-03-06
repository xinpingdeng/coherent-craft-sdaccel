/*
******************************************************************************
** MAIN FUNCTION
******************************************************************************
*/

#include "util_sdaccel.h"
#include "prepare.h"

int main(int argc, char* argv[]){
  // Check argument
  if (argc != 2) {
    fprintf(stderr, "ERROR: Failed to execute the program!\n");
    fprintf(stderr, "USAGE: %s xclbin\n", argv[0]);
    fprintf(stderr, "INFO: Please re-execute it with the above usage method!\n");
    return EXIT_FAILURE;
  }	

  // Prepare host buffers
  cl_int ndata1;
  cl_int ndata2;
  cl_int nchan        = 288;
  cl_int nbaseline    = 435;
  cl_int ntime_per_cu = 256;
  cl_int nsamp_per_time;
  cl_int nburst_per_time;

  if(is_hw_emulation()){
    nchan        = 288;
    ntime_per_cu = 10;
    nbaseline    = 15;
  }
  if(is_sw_emulation()){
    nchan        = 288;
    ntime_per_cu = 10;
    nbaseline    = 15;    
  }
  nsamp_per_time  = nchan*nbaseline-(nchan*nbaseline)%(NSAMP_PER_BURST*BURST_LENGTH);  // 288*435 = 2^5*3^3*5*29 for all channel and baseline
  nburst_per_time = nsamp_per_time/NSAMP_PER_BURST;
  
  ndata1 = 2 * nsamp_per_time;
  ndata2 = 2 * ntime_per_cu * nsamp_per_time;
  
  data_t *in_pol1 = NULL;
  data_t *in_pol2 = NULL;
  data_t *sw_out = NULL;
  data_t *hw_out = NULL;
  data_t *cal_pol1 = NULL;
  data_t *cal_pol2 = NULL;
  data_t *sky = NULL;
  data_t *sw_average_pol1 = NULL;
  data_t *sw_average_pol2 = NULL;
  data_t *hw_average_pol1 = NULL;
  data_t *hw_average_pol2 = NULL;

  in_pol1  = (data_t *)aligned_alloc(MEM_ALIGNMENT, ndata2*sizeof(data_t));
  in_pol2  = (data_t *)aligned_alloc(MEM_ALIGNMENT, ndata2*sizeof(data_t));
  sw_out   = (data_t *)aligned_alloc(MEM_ALIGNMENT, ndata2*sizeof(data_t));
  hw_out   = (data_t *)aligned_alloc(MEM_ALIGNMENT, ndata2*sizeof(data_t));  
  cal_pol1 = (data_t *)aligned_alloc(MEM_ALIGNMENT, ndata1*sizeof(data_t));
  cal_pol2 = (data_t *)aligned_alloc(MEM_ALIGNMENT, ndata1*sizeof(data_t));
  sky      = (data_t *)aligned_alloc(MEM_ALIGNMENT, ndata1*sizeof(data_t));
  sw_average_pol1 = (data_t *)aligned_alloc(MEM_ALIGNMENT, ndata1*sizeof(data_t));
  sw_average_pol2 = (data_t *)aligned_alloc(MEM_ALIGNMENT, ndata1*sizeof(data_t));
  hw_average_pol1 = (data_t *)aligned_alloc(MEM_ALIGNMENT, ndata1*sizeof(data_t));
  hw_average_pol2 = (data_t *)aligned_alloc(MEM_ALIGNMENT, ndata1*sizeof(data_t));
  
  fprintf(stdout, "INFO: %f MB memory used on host in total\n",
	  (4*ndata2 + 7*ndata1)*sizeof(data_t)/(1024.*1024.));
  fprintf(stdout, "INFO: %f MB memory used on device in total\n",
	  (3*ndata2 + 5*ndata1)*sizeof(data_t)/(1024.*1024.));
  fprintf(stdout, "INFO: %f MB memory used on device for raw input\n",
	  2*ndata2*sizeof(data_t)/(1024.*1024.));  
  fprintf(stdout, "INFO: %f MB memory used on device for raw output\n",
	  ndata2*sizeof(data_t)/(1024.*1024.));  
  
  // Prepare input
  cl_uint i;
  srand(time(NULL));
  for(i = 0; i < ndata2; i++){
    in_pol1[i] = (data_t)(0.99*(rand()%DATA_RANGE));
    in_pol2[i] = (data_t)(0.99*(rand()%DATA_RANGE));
  }  
  for(i = 0; i < ndata1; i++){
    cal_pol1[i] = (data_t)(0.99*(rand()%DATA_RANGE));
    cal_pol2[i] = (data_t)(0.99*(rand()%DATA_RANGE));
    sky[i]      = (data_t)(0.99*(rand()%DATA_RANGE));
  }
  
  // Calculate on host
  cl_float cpu_elapsed_time;
  struct timespec host_start;
  struct timespec host_finish;
  clock_gettime(CLOCK_REALTIME, &host_start);
  prepare(in_pol1, in_pol2, cal_pol1, cal_pol2, sky, sw_out, sw_average_pol1, sw_average_pol2, nsamp_per_time, ntime_per_cu);
  fprintf(stdout, "INFO: DONE HOST EXECUTION\n");
  clock_gettime(CLOCK_REALTIME, &host_finish);
  cpu_elapsed_time = (host_finish.tv_sec - host_start.tv_sec) + (host_finish.tv_nsec - host_start.tv_nsec)/1.0E9L;
  
  // Get platform ID and info
  cl_int err;
  cl_uint platforms;
  cl_int get_platform_id = 0;
  cl_platform_id platform_id;
  cl_platform_id platform_ids[MAX_PALTFORMS];
  char platform_name[PARAM_VALUE_SIZE];  
  OCL_CHECK(err, err = clGetPlatformIDs(MAX_PALTFORMS, platform_ids, &platforms));
  for(i = 0; i < platforms; i++){
    OCL_CHECK(err, err = clGetPlatformInfo(platform_ids[i], CL_PLATFORM_VENDOR, PARAM_VALUE_SIZE, (void *)platform_name, NULL));
    if(strcmp(platform_name, "Xilinx") == 0){
      platform_id = platform_ids[i];
      get_platform_id = 1;
      break;
    }
  }
  if(get_platform_id ==0){
    fprintf(stderr, "ERROR: Failed to get platform ID!\n");
    fprintf(stderr, "ERROR: Please look into the file \"%s\" above line [%d]!\n", __FILE__, __LINE__);
    fprintf(stderr, "ERROR: Test failed ...!\n");
    return EXIT_FAILURE;
  }
  
  // Get device ID and info
  cl_uint devices;
  cl_int get_device_id = 0;
  cl_device_id device_id;
  cl_device_id device_ids[MAX_DEVICES];
  char device_name[PARAM_VALUE_SIZE];
  OCL_CHECK(err, err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ACCELERATOR, MAX_DEVICES, device_ids, &devices));
  for(i = 0; i < devices; i++){
    OCL_CHECK(err, err = clGetDeviceInfo(device_ids[i], CL_DEVICE_NAME, PARAM_VALUE_SIZE, device_name, 0));    
    if(strstr(device_name, "u280")){
      device_id = device_ids[i];
      get_device_id = 1;
      break;
    }
  }
  if(get_device_id ==0){
    fprintf(stderr, "ERROR: Failed to get device ID!\n");
    fprintf(stderr, "ERROR: Please look into the file \"%s\" above line [%d]!\n", __FILE__, __LINE__);
    fprintf(stderr, "ERROR: Test failed ...!\n");   
    return EXIT_FAILURE; 
  }  
  fprintf(stdout, "INFO: We will use %s!\n", device_name);

  // Create context
  cl_context context;
  OCL_CHECK(err, context = clCreateContext(0, 1, &device_id, NULL, NULL, &err));
  
  // Create command queue
  cl_command_queue queue;
  OCL_CHECK(err, queue = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err));
  
  // Read kernel binary into memory
  char *xclbin = argv[1];
  unsigned char *binary = NULL;
  size_t binary_size;
  fprintf(stdout, "INFO: loading xclbin %s\n", xclbin);
  binary_size = (int)load_file_to_memory(xclbin, (char **) &binary);
  if (binary_size <= 0) {
    fprintf(stderr, "ERROR: Failed to load kernel from xclbin: %s\n", xclbin);
    fprintf(stderr, "ERROR: Please look into the file \"%s\" above line [%d]!\n", __FILE__, __LINE__);
    fprintf(stderr, "ERROR: Test failed ...!\n");
    return EXIT_FAILURE;
  }

  // Create program binary with kernel binary
  cl_int status;
  cl_program program;
  OCL_CHECK(err, program = clCreateProgramWithBinary(context, 1, &device_id, &binary_size, (const unsigned char **) &binary, &status, &err));
  free(binary);
  
  // Program the card with the program binary
  OCL_CHECK(err, err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL));

  // Create the kernel
  cl_kernel kernel;
  OCL_CHECK(err, kernel = clCreateKernel(program, "knl_prepare", &err));

  // Prepare device buffer
  cl_mem buffer_in_pol1;
  cl_mem buffer_in_pol2;                 
  cl_mem buffer_cal_pol1;
  cl_mem buffer_cal_pol2;
  cl_mem buffer_sky;
  cl_mem buffer_out;
  cl_mem buffer_average_pol1;
  cl_mem buffer_average_pol2;
  cl_mem pt[8];

  OCL_CHECK(err, buffer_in_pol1      = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(data_t)*ndata2, in_pol1, &err));
  OCL_CHECK(err, buffer_in_pol2      = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(data_t)*ndata2, in_pol2, &err));
  OCL_CHECK(err, buffer_sky          = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(data_t)*ndata1, sky, &err));
  OCL_CHECK(err, buffer_cal_pol1     = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(data_t)*ndata1, cal_pol1, &err));
  OCL_CHECK(err, buffer_cal_pol2     = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(data_t)*ndata1, cal_pol2, &err));
  OCL_CHECK(err, buffer_out          = clCreateBuffer(context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, sizeof(data_t)*ndata2, hw_out, &err));
  OCL_CHECK(err, buffer_average_pol1 = clCreateBuffer(context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, sizeof(data_t)*ndata1, hw_average_pol1, &err));
  OCL_CHECK(err, buffer_average_pol2 = clCreateBuffer(context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, sizeof(data_t)*ndata1, hw_average_pol2, &err));
  if (!(buffer_in_pol1&&
	buffer_in_pol2&&
	buffer_out&&
	buffer_sky&&
	buffer_cal_pol1&&
	buffer_cal_pol2&&
	buffer_average_pol1&&
	buffer_average_pol2
	)) {
    fprintf(stderr, "ERROR: Failed to allocate device memory!\n");
    fprintf(stderr, "ERROR: Please look into the file \"%s\" above line [%d]!\n", __FILE__, __LINE__);
    fprintf(stderr, "ERROR: Test failed ...!\n");
    return EXIT_FAILURE;
  }

  // Setup kernel arguments
  // To use multiple banks, this has to be before any enqueue options (e.g., clEnqueueMigrateMemObjects)
  pt[0] = buffer_in_pol1;
  pt[1] = buffer_in_pol2;
  pt[2] = buffer_cal_pol1;
  pt[3] = buffer_cal_pol2;
  pt[4] = buffer_sky;
  pt[5] = buffer_out;
  pt[6] = buffer_average_pol1;
  pt[7] = buffer_average_pol2;

  OCL_CHECK(err, err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer_in_pol1));
  OCL_CHECK(err, err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &buffer_in_pol2)); 
  OCL_CHECK(err, err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &buffer_cal_pol1));
  OCL_CHECK(err, err = clSetKernelArg(kernel, 3, sizeof(cl_mem), &buffer_cal_pol2)); 
  OCL_CHECK(err, err = clSetKernelArg(kernel, 4, sizeof(cl_mem), &buffer_sky));
  OCL_CHECK(err, err = clSetKernelArg(kernel, 5, sizeof(cl_mem), &buffer_out)); 
  OCL_CHECK(err, err = clSetKernelArg(kernel, 6, sizeof(cl_mem), &buffer_average_pol1));
  OCL_CHECK(err, err = clSetKernelArg(kernel, 7, sizeof(cl_mem), &buffer_average_pol2));
  OCL_CHECK(err, err = clSetKernelArg(kernel, 8, sizeof(cl_int), &nburst_per_time));
  OCL_CHECK(err, err = clSetKernelArg(kernel, 9, sizeof(cl_int), &ntime_per_cu));
  
  fprintf(stdout, "INFO: DONE SETUP KERNEL\n");

  // Migrate host memory to device
  cl_int inputs = 5;
  OCL_CHECK(err, err = clEnqueueMigrateMemObjects(queue, inputs, pt, 0 ,0,NULL, NULL));
  OCL_CHECK(err, err = clFinish(queue));
  fprintf(stdout, "INFO: DONE MEMCPY FROM HOST TO KERNEL\n");

  // Execute the kernel
  struct timespec device_start;
  struct timespec device_finish;
  cl_float kernel_elapsed_time;
  clock_gettime(CLOCK_REALTIME, &device_start);
  OCL_CHECK(err, err = clEnqueueTask(queue, kernel, 0, NULL, NULL));
  OCL_CHECK(err, err = clFinish(queue));
  fprintf(stdout, "INFO: DONE KERNEL EXECUTION\n");
  clock_gettime(CLOCK_REALTIME, &device_finish);
  kernel_elapsed_time = (device_finish.tv_sec - device_start.tv_sec) + (device_finish.tv_nsec - device_start.tv_nsec)/1.0E9L;

  // Migrate data from device to host
  cl_int outputs = 3;
  OCL_CHECK(err, err = clEnqueueMigrateMemObjects(queue, outputs, &pt[5], CL_MIGRATE_MEM_OBJECT_HOST, 0, NULL, NULL));
  OCL_CHECK(err, err = clFinish(queue));
  fprintf(stdout, "INFO: DONE MEMCPY FROM KERNEL TO HOST\n");

  // Check the result
  cl_int ndata3 = 0;
  data_t res = 1.0E-2;
  for(i=0;i<ndata1;i++){
    if(fabs(sw_average_pol1[i]-hw_average_pol1[i]) > fabs(sw_average_pol1[i]*res)){
      //if(sw_average_pol1[i]!=0){
      //  fprintf(stdout, "INFO: Mismatch on AVERAGE_POL1: %d\t%f\t%f\t%.0f\n", i, (float)sw_average_pol1[i], (float)hw_average_pol1[i], 100.0*fabs((sw_average_pol1[i]-hw_average_pol1[i])/sw_average_pol1[i]));
      //}
      //else{
      //  fprintf(stdout, "INFO: Mismatch on AVERAGE_POL1: %d\t%f\t%f\n", i, (float)sw_average_pol1[i], (float)hw_average_pol1[i]);
      //}
      ndata3++;
    }
  }
  fprintf(stdout, "INFO: %d from %d, %.0f%% of AVERAGE_POL1 is outside %.0f%% range\n", ndata3, ndata1, 100*ndata3/(float)ndata1, 100*(float)res);  
  ndata3 = 0;
  for(i=0;i<ndata1;i++){
    if(fabs(sw_average_pol2[i]-hw_average_pol2[i]) > fabs(sw_average_pol2[i]*res)){
      //if(sw_average_pol2[i]!=0){
      //  fprintf(stdout, "INFO: Mismatch on AVERAGE_POL2: %d\t%f\t%f\t%.0f\n", i, (float)sw_average_pol2[i], (float)hw_average_pol2[i], 100.0*fabs((sw_average_pol2[i]-hw_average_pol2[i])/sw_average_pol2[i]));
      //}
      //else{
      //  fprintf(stdout, "INFO: Mismatch on AVERAGE_POL2: %d\t%f\t%f\n", i, (float)sw_average_pol2[i], (float)hw_average_pol2[i]);
      //}
      ndata3++;
    }
  }
  
  fprintf(stdout, "INFO: %d from %d, %.0f%% of AVERAGE_POL2 is outside %.0f%% range\n", ndata3, ndata1, 100*ndata3/(float)ndata1, 100*(float)res);
  ndata3 = 0;
  for(i=0;i<ndata2;i++){
    if(fabs(sw_out[i]-hw_out[i]) > fabs(sw_out[i]*res)){
      //if(sw_out[i]!=0){
      //  fprintf(stdout, "INFO: Mismatch on OUT: %d\t%f\t%f\t%.0f\n", i, (float)sw_out[i], (float)hw_out[i], 100.0*fabs((sw_out[i]-hw_out[i])/sw_out[i]));
      //}
      //else{
      //  fprintf(stdout, "INFO: Mismatch on OUT: %d\t%f\t%f\n", i, (float)sw_out[i], (float)hw_out[i]);
      //}
      ndata3++;
    }
  }
  fprintf(stdout, "INFO: %d from %d, %.0f%% of OUT is outside %.0f%% range\n", ndata3, ndata2, 100*ndata3/(float)ndata2, 100*(float)res);
  
  fprintf(stdout, "INFO: DONE RESULT CHECK\n");
  fprintf(stdout, "INFO: Elapsed time of CPU code is %E seconds\n", cpu_elapsed_time);
  fprintf(stdout, "INFO: Elapsed time of kernel is %E seconds\n", kernel_elapsed_time);
    
  // Cleanup
  clReleaseMemObject(buffer_in_pol1);
  clReleaseMemObject(buffer_in_pol2);
  clReleaseMemObject(buffer_cal_pol1);
  clReleaseMemObject(buffer_cal_pol2);
  clReleaseMemObject(buffer_sky);
  clReleaseMemObject(buffer_out);
  clReleaseMemObject(buffer_average_pol1);
  clReleaseMemObject(buffer_average_pol2);
  
  free(in_pol1);
  free(in_pol2);
  free(sw_out);
  free(hw_out);
  free(cal_pol1);
  free(cal_pol2);
  free(sky);
  free(sw_average_pol1);
  free(sw_average_pol2);
  free(hw_average_pol1);
  free(hw_average_pol2);
  
  clReleaseProgram(program);
  clReleaseKernel(kernel);
  clReleaseCommandQueue(queue);
  clReleaseContext(context);

  fprintf(stdout, "INFO: DONE ALL\n");
  
  return EXIT_SUCCESS;
}
