/*
 * FtAudioFIRFilter.c
 * Hamilton Kibbe
 * Copyright 2012 Hamilton Kibbe
 */

#include "FtAudioFIRFilter.h"
#include "FtAudioDsp.h"
#include "FtAudioUtilities.h"
#include "FtAudioFFT.h"
#include <stdlib.h>
#include <stdio.h>

typedef enum _ConvolutionMode
{
    DIRECT  = 0,
    FFT     = 1
} ConvolutionMode_t;

/* FtAudioFIRFilter ***********************************************************/
struct FtAudioFIRFilter
{
    float * kernel;
    const float * kernel_end;
    float * overlap;
    unsigned kernel_length;
    unsigned overlap_length;
    ConvolutionMode_t conv_mode;
    FtAudioFFTConfig *fft_config;
    float * fft_kernel;
    unsigned fft_length;
};


/* FtAudioFIRFilterInit *******************************************************/
FtAudioFIRFilter* 
FtAudioFIRFilterInit(const float*   filter_kernel,
                     unsigned		length)
{

    // Array lengths and sizes
    unsigned kernel_length = length;            // IN SAMPLES!
    unsigned overlap_length = kernel_length - 1;        // IN SAMPLES!
    
    
    
    // Allocate Memory
    FtAudioFIRFilter* filter = (FtAudioFIRFilter*)malloc(sizeof(FtAudioFIRFilter));
    float* kernel = (float*)malloc(kernel_length * sizeof(float));
    float* overlap = (float*)malloc(overlap_length * sizeof(float));
   
    // Initialize Buffers
    FtAudioCopyBuffer(kernel, filter_kernel, kernel_length);
    FtAudioFillBuffer(overlap, overlap_length, 0.0);

    // Set up the struct
    filter->kernel = kernel;
    filter->kernel_end = filter_kernel + (kernel_length - 1);
    filter->overlap = overlap;
    filter->kernel_length = kernel_length;
    filter->overlap_length = overlap_length;
    filter->fft_config = NULL;
    filter->fft_kernel = NULL;
    
    if (kernel_length < USE_FFT_CONVOLUTION_LENGTH)
    {
        filter->conv_mode = DIRECT;
    }

    else
    {
        filter->conv_mode = FFT;
    }
    
    return filter;
}


/* FtAudioFIRFilterFree *******************************************************/
FtAudioError_t 
FtAudioFIRFilterFree(FtAudioFIRFilter * filter)
{
    free(filter->kernel);
    free(filter->overlap);
    if (filter->fft_config)
        FtAudioFFTFree(filter->fft_config);
    if (filter->fft_kernel)
        free(filter->fft_kernel);
    free(filter);
    return FT_NOERR;
}

/* FtAudioFIRFilterFlush ******************************************************/
FtAudioError_t
FtAudioFIRFilterFlush(FtAudioFIRFilter* filter)
{
    // The only stateful part of this is the overlap buffer, so this just 
    //zeros it out
    FtAudioFillBuffer(filter->overlap, filter->overlap_length, 0.0);
    return FT_NOERR;
}


/* FtAudioFIRFilterProcess ****************************************************/
FtAudioError_t
FtAudioFIRFilterProcess(FtAudioFIRFilter* filter,
                        float*  outBuffer, 
                        const float* inBuffer, 
                        unsigned n_samples)
{

    // Do direct convolution
    if (filter->conv_mode == DIRECT)
    {
        unsigned resultLength = n_samples + (filter->kernel_length - 1);
    
        // Temporary buffer to store full result of filtering..
        float buffer[resultLength];
    
        FtAudioConvolve((float*)inBuffer, n_samples, 
                        filter->kernel, filter->kernel_length, buffer);
        // Add in the overlap from the last block
        FtAudioBufferAdd(buffer, filter->overlap, buffer, filter->overlap_length);
        FtAudioCopyBuffer(filter->overlap, buffer + n_samples, filter->overlap_length);
        FtAudioCopyBuffer(outBuffer, buffer, n_samples);
    }
    
    // Otherwise do FFT Convolution
    else
    {
        // Configure the FFT on the first run, that way we can figure out how
        // long the input blocks are going to be. This makes the filter more 
        // complicated internally in order to make the convolution transparent.
        // Calculate length of FFT
        int fft_length = next_pow2(n_samples + filter->kernel_length - 1);
        printf("\n\n\nFFT LENGTH: %d!!!!\n\n\n", fft_length);
        if(!filter->fft_config)
            filter->fft_config = FtAudioFFTInit(fft_length);
        filter->fft_length = fft_length;
        
        // fft kernel buffers
        float padded_kernel[fft_length];

                    
        // Write zero padded kernel to buffer
        FtAudioFillBuffer(padded_kernel, fft_length, 0.0);
        FtAudioCopyBuffer(padded_kernel + ((fft_length / 2) - (unsigned)(filter->kernel_length / 2)-1), filter->kernel, filter->kernel_length );
                       
        // Calculate FFT of filter kernel
        //FtAudioFFTForwardInterleaved(filter->fft_config, (DSPComplex*)paddedkernelcomplex, (DSPComplex*)fft_kernel);
       // filter->fft_kernel = fft_kernel;

        

        
        // Buffer for transformed input
        float buffer[filter->fft_length];
        FtAudioFillBuffer(buffer, filter->fft_length, 0.0);
        FtAudioCopyBuffer(buffer, inBuffer, n_samples);
        
        // Convolve
        FtAudioFFTConvolve(filter->fft_config, buffer, filter->fft_length, padded_kernel, filter->fft_length, buffer);
      
        // Add in the overlap from the last block
        FtAudioBufferAdd(buffer, filter->overlap, buffer, filter->overlap_length);
        FtAudioCopyBuffer(filter->overlap, buffer + n_samples, filter->overlap_length);
        FtAudioCopyBuffer(outBuffer, buffer, n_samples);
    }
        
    return FT_NOERR;
}


/* FtAudioFIRFilterUpdateKernel ***********************************************/
FtAudioError_t
FtAudioFIRFilterUpdateKernel(FtAudioFIRFilter*  filter, const float* filter_kernel)
{
    // Copy the new kernel into the filter
    FtAudioCopyBuffer(filter->kernel, filter_kernel, filter->kernel_length);
    return FT_NOERR;
}

