/**********
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/
/*****************************************************************************************
*  GUI Flow :
*      
*  By default this example supports 1DDR execution in GUI mode for 
*  all the DSAs. To make this example to work with multi DDR DSAs
*  please follow steps mentioned below.
*
*  Note : "bandwidth" in map_connect options below is the kernel name defined in kernel.cl   
*
*  ***************************************************************************************
*  DSA  (2DDR):
*              
*  1.<SDx Project> > Properties > C/C++ Build > Settings > SDx XOCC Kernel Compiler 
*                  > Miscellaneous > Other flags
*  2.In "Other flags" box enter following
*     a. --max_memory_ports all 
*     b. --sp bandwidth_1.m_axi_gmem0:bank0
*     c. --sp bandwidth_1.m_axi_gmem1:bank1 
*  3.<SDx Project> > Properties > C/C++ Build > Settings > SDx XOCC Kernel Linker
*                  > Miscellaneous > Other flags
*  4.Repeat step 2 above
*
* *****************************************************************************************
*  DSA  (4DDR):
*              
*  1.<SDx Project> > Properties > C/C++ Build > Settings > SDx XOCC Kernel Compiler 
*                  > Miscellaneous > Other flags
*  2.In "Other flags" box enter following
*     a. --max_memory_ports all 
*     b. --sp bandwidth_1.m_axi_gmem0:bank0
*     c. --sp bandwidth_1.m_axi_gmem1:bank1 
*     d. --sp bandwidth_1.m_axi_gmem2:bank2
*     e. --sp bandwidth_1.m_axi_gmem3:bank3 
*  3.<SDx Project> > Properties > C/C++ Build > Settings > SDx XOCC Kernel Linker
*                  > Miscellaneous > Other flags
*  4.Repeat step 2 above
*  5.Define NUM_BANKS_4 macro in kernel "#define NUM_BANKS_4" at the top of kernel.cl 
* 
* *****************************************************************************************
*
*  CLI Flow:
*
*  In CLI flow makefile detects the DDR of a device and based on that
*  automatically it adds all the flags that are necessary. This example can be
*  used similar to other examples in CLI flow, extra setup is not needed.
*
*********************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include "xcl2.hpp"

#ifdef USE_4DDR
    #define DDR_BANKS 4
#elif USE_2DDR
    #define DDR_BANKS 2
#else
    #define DDR_BANKS 1
#endif

int main(int argc, char** argv) {

    if(argc != 1) {
        printf("Usage: %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    cl::Context context(device);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE);
    std::string device_name = device.getInfo<CL_DEVICE_NAME>();
    std::cout << "Found Device=" << device_name.c_str() << std::endl;

    std::string binaryFile = xcl::find_binary_file(device_name, "krnl_kernel_global");
    cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
    devices.resize(1);
    cl::Program program(context, devices, bins);

    int err;

    size_t globalbuffersize = 1024*1024*1024;    /* 1GB */

    /* Reducing the data size for emulation mode */
    char *xcl_mode = getenv("XCL_EMULATION_MODE");
    if (xcl_mode != NULL){
        globalbuffersize = 1024 * 1024;  /* 1MB */
    }

    /* Input buffer */
    unsigned char *input_host = ((unsigned char *) malloc(globalbuffersize));
    if(input_host==NULL) {
        printf("Error: Failed to allocate host side copy of OpenCL source buffer of size %zu\n",globalbuffersize);
        return EXIT_FAILURE;
    }

    for(size_t i = 0; i < globalbuffersize; i++) {
        input_host[i] = i % 256;
    }

    short ddr_banks = DDR_BANKS;

    /* Index for the ddr pointer array: 4=4, 2=2, 1=2 */
    char num_buffers = ddr_banks + (ddr_banks % 2);

    /* buffer[0] is input0
     * buffer[1] is output0
     * buffer[2] is input1
     * buffer[3] is output1 */
    cl::Buffer *buffer[num_buffers];

    cl_mem_ext_ptr_t ext_buffer[num_buffers];

    #if defined(USE_2DDR) || defined(USE_4DDR)
        unsigned xcl_bank[4] = {
            XCL_MEM_DDR_BANK0,
            XCL_MEM_DDR_BANK1,
            XCL_MEM_DDR_BANK2,
            XCL_MEM_DDR_BANK3
        };

        for (int i = 0; i < ddr_banks; i++) {
            ext_buffer[i].flags = xcl_bank[i];
            ext_buffer[i].obj = NULL;
            ext_buffer[i].param = 0;

	    buffer[i] = new cl::Buffer(context, 
				   CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX, 
				   globalbuffersize, 
				   &ext_buffer[i], 
				   &err);
            if(err != CL_SUCCESS) {
                printf("Error: Failed to allocate buffer in DDR bank %zu\n", globalbuffersize);
                return EXIT_FAILURE;
            }
        } /* End for (i < ddr_banks) */
    #else
	 buffer[0] = new cl::Buffer(context, 
 				CL_MEM_READ_WRITE, 
				globalbuffersize, 
				NULL, 
				&err);
	 buffer[1] = new cl::Buffer(context, 
				CL_MEM_READ_WRITE, 
				globalbuffersize, 
				NULL, 
				&err); 
         if(err != CL_SUCCESS) {
            printf("Error: Failed to allocate input/output_buffer0 in BANK0 of size %zu\n", globalbuffersize);
            return EXIT_FAILURE;
         }
    #endif

    cl_ulong num_blocks = globalbuffersize/64;
    double dbytes = globalbuffersize;
    double dmbytes = dbytes / (((double)1024) * ((double)1024));
    printf("Starting kernel to read/write %.0lf MB bytes from/to global memory... \n", dmbytes);

    /* Write input buffer */
    /* Map input buffer for PCIe write */
    unsigned char *map_input_buffer0;
    map_input_buffer0 = (unsigned char *) q.enqueueMapBuffer(*(buffer[0]), 
							     CL_FALSE, 
							     CL_MAP_WRITE_INVALIDATE_REGION, 
							     0, 
							     globalbuffersize, 
							     NULL, 
							     NULL, 
							     &err);
    if (err != CL_SUCCESS) {
            printf("Error: Failed to enqueueMapBuffer0 OpenCL buffer\n");
            printf("Error: Test failed\n");
            return EXIT_FAILURE;
    }
    q.finish();

    /* prepare data to be written to the device */
    for(size_t i = 0; i<globalbuffersize; i++) {
        map_input_buffer0[i] = input_host[i];
    }
    err = q.enqueueUnmapMemObject(*(buffer[0]), 
				  map_input_buffer0);
    if (err != CL_SUCCESS) {
        printf("Error: Failed to copy input dataset to OpenCL buffer\n");
        printf("Error: Test failed\n");
        return EXIT_FAILURE;
    }
    q.finish();

    #ifdef USE_4DDR
        unsigned char *map_input_buffer1;
 	map_input_buffer1 = (unsigned char *) q.enqueueMapBuffer(*(buffer[2]), 
								 CL_FALSE, 
								 CL_MAP_WRITE_INVALIDATE_REGION, 
								 0, 
								 globalbuffersize, 
								 NULL, 
								 NULL, 
								 &err);
        if (err != CL_SUCCESS) {
            printf("Error: Failed to enqueueMapBuffer1 OpenCL buffer\n");
            printf("Error: Test failed\n");
            return EXIT_FAILURE;
        }
	q.finish();

        /* Prepare data to be written to the device */
        for(size_t i = 0; i < globalbuffersize; i++) {
            map_input_buffer1[i] = input_host[i];
        }

	err = q.enqueueUnmapMemObject(*(buffer[2]), 
				      map_input_buffer1);
        if (err != CL_SUCCESS) {
            printf("Error: Failed to copy input dataset to OpenCL buffer\n");
            printf("Error: Test failed\n");
            return EXIT_FAILURE;
        }
        q.finish();
    #endif

    /* Set the kernel arguments */
    cl::Kernel krnl_global_bandwidth(program, "bandwidth");
    int arg_index = 0;
    int buffer_index = 0;

    krnl_global_bandwidth.setArg(arg_index++, *(buffer[buffer_index++]));
    krnl_global_bandwidth.setArg(arg_index++, *(buffer[buffer_index++]));
    #ifdef USE_4DDR
       krnl_global_bandwidth.setArg(arg_index++, *(buffer[buffer_index++]));
       krnl_global_bandwidth.setArg(arg_index++, *(buffer[buffer_index++]));
    #endif
    krnl_global_bandwidth.setArg(arg_index++, num_blocks);

    unsigned long nsduration;
    cl::Event event;

    /* Execute Kernel */
    q.enqueueTask(krnl_global_bandwidth, NULL, &event);
    event.wait();
    nsduration = event.getProfilingInfo<CL_PROFILING_COMMAND_END>() - event.getProfilingInfo<CL_PROFILING_COMMAND_START>(); 

    /* Copy results back from OpenCL buffer */
    unsigned char *map_output_buffer0;
    map_output_buffer0 = (unsigned char *) q.enqueueMapBuffer(*(buffer[1]), 
							      CL_FALSE, 
							      CL_MAP_READ, 
							      0, 
							      globalbuffersize, 
							      NULL, 
							      NULL, 
							      &err);
    if (err != CL_SUCCESS) {
        printf("ERROR: Failed to read output size buffer %d\n", err);
        printf("ERROR: Test failed\n");
        return EXIT_FAILURE;
    }
    q.finish();

    /* Check the results of output0 */
    for (size_t i = 0; i < globalbuffersize; i++) {
        if (map_output_buffer0[i] != input_host[i]) {
            printf("ERROR : kernel failed to copy entry %zu input %i output %i\n",i,input_host[i], map_output_buffer0[i]);
            return EXIT_FAILURE;
        }
    }
    #ifdef USE_4DDR
        unsigned char *map_output_buffer1;
  	map_output_buffer1 = (unsigned char *) q.enqueueMapBuffer(*(buffer[3]), 
								  CL_FALSE, 
								  CL_MAP_READ, 
								  0, 
								  globalbuffersize, 
								  NULL, 
								  NULL, 
								  &err);
        if (err != CL_SUCCESS) {
            printf("ERROR: Failed to read output size buffer %d\n", err);
            printf("ERROR: Test failed\n");
            return EXIT_FAILURE;
        }
        q.finish();

        /* Check the results of output1 */
        for (size_t i = 0; i < globalbuffersize; i++) {
            if (map_output_buffer1[i] != input_host[i]) {
                printf("ERROR : kernel failed to copy entry %zu input %i output %i\n", i, input_host[i], map_output_buffer1[i]);
                return EXIT_FAILURE;
            }
        }
    #endif

    #if defined(USE_2DDR) || defined(USE_4DDR)
    for(int i = 0; i < ddr_banks; i++)
     {
	delete(buffer[i]);
     }
    #else
	delete(buffer[0]);
	delete(buffer[1]);
    #endif

    /* Profiling information */
    double dnsduration = ((double)nsduration);
    double dsduration = dnsduration / ((double) 1000000000);

    double bpersec = (dbytes/dsduration);
    double mbpersec = bpersec / ((double) 1024*1024 ) * ddr_banks;

    printf("Kernel completed read/write %.0lf MB bytes from/to global memory.\n", dmbytes);
    printf("Execution time = %f (sec) \n", dsduration);
    printf("Concurrent Read and Write Throughput = %f (MB/sec) \n", mbpersec);

    printf("TEST PASSED\n");
    return EXIT_SUCCESS;
}
