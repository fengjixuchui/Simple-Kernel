//==================================================================================================================================
//  Simple Kernel: Kernel Entrypoint
//==================================================================================================================================
//
// Version 0.z
//
// Author:
//  KNNSpeed
//
// Source Code:
//  https://github.com/KNNSpeed/Simple-Kernel
//
// This program is a small x86-64 program for use with the Simple UEFI Bootloader: https://github.com/KNNSpeed/Simple-UEFI-Bootloader.
// It contains some functions that might prove useful in development of other bare-metal programs, and showcases some of the features
// provided by the bootloader (e.g. Multi-GPU framebuffer support).
//
// The main function of this program, defined as "kernel_main" in the accompanying compile scripts, is passed a pointer to the following
// structure from the bootloader:
/*
  typedef struct {
    UINT16                  Bootloader_MajorVersion;        // The major version of the bootloader
    UINT16                  Bootloader_MinorVersion;        // The minor version of the bootloader

    UINT32                  Memory_Map_Descriptor_Version;  // The memory descriptor version
    UINTN                   Memory_Map_Descriptor_Size;     // The size of an individual memory descriptor
    EFI_MEMORY_DESCRIPTOR  *Memory_Map;                     // The system memory map as an array of EFI_MEMORY_DESCRIPTOR structs
    UINTN                   Memory_Map_Size;                // The total size of the system memory map

    EFI_PHYSICAL_ADDRESS    Kernel_BaseAddress;             // The base memory address of the loaded kernel file
    UINTN                   Kernel_Pages;                   // The number of pages (1 page == 4096 bytes) allocated for the kernel file

    CHAR16                 *ESP_Root_Device_Path;           // A UTF-16 string containing the drive root of the EFI System Partition as converted from UEFI device path format
    UINT64                  ESP_Root_Size;                  // The size (in bytes) of the above ESP root string
    CHAR16                 *Kernel_Path;                    // A UTF-16 string containing the kernel's file path relative to the EFI System Partition root (it's the first line of Kernel64.txt)
    UINT64                  Kernel_Path_Size;               // The size (in bytes) of the above kernel file path
    CHAR16                 *Kernel_Options;                 // A UTF-16 string containing various load options (it's the second line of Kernel64.txt)
    UINT64                  Kernel_Options_Size;            // The size (in bytes) of the above load options string

    EFI_RUNTIME_SERVICES   *RTServices;                     // UEFI Runtime Services
    GPU_CONFIG             *GPU_Configs;                    // Information about available graphics output devices; see below GPU_CONFIG struct for details
    EFI_FILE_INFO          *FileMeta;                       // Kernel file metadata
    void                   *RSDP;                           // A pointer to the RSDP ACPI table
  } LOADER_PARAMS;
*/
//
// GPU_CONFIG is a custom structure that is defined as follows:
//
/*
  typedef struct {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  *GPUArray;             // An array of EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE structs defining each available framebuffer
    UINT64                              NumberOfFrameBuffers; // The number of structs in the array (== the number of available framebuffers)
  } GPU_CONFIG;
*/
//
// The header file, Kernel64.h, contains definitions for the data types of each pointer in the above structures.
//
// Note: As mentioned in the bootloader's embedded documentation, GPU_Configs provides access to linear framebuffer addresses for directly
// drawing to connected screens: specifically one for each active display per GPU. Typically there is one active display per GPU, but it is
// up to the GPU firmware maker to deterrmine that. See "12.10 Rules for PCI/AGP Devices" in the UEFI Specification 2.7 Errata A for more
// details: http://www.uefi.org/specifications
//

#include "Kernel64.h"

// Stack size defined in number of bytes, e.g. (1 << 12) is 4kiB, (1 << 20) is 1MiB
#define STACK_SIZE (1 << 20)

// This might allow for occasional slight performance increases. Not guaranteed to always happen, but aligning this to 64 bytes increases the probability.
__attribute__((aligned(64))) static volatile unsigned char kernel_stack[STACK_SIZE] = {0};

// The character print function can draw raw single-color bitmaps formatted like this, given appropriate height and width values
static const unsigned char load_image[48] = {
    0x00, 0x3F, 0x80, 0x00, // ........ ..@@@@@@ @....... ........
    0x01, 0x80, 0x30, 0x00, // .......@ @....... ..@@.... ........
    0x0C, 0x00, 0x06, 0x00, // ....@@.. ........ .....@@. ........
    0x30, 0x1E, 0xE1, 0x80, // ..@@.... ...@@@@. @@@....@ @.......
    0x60, 0x61, 0xC0, 0xC0, // .@@..... .@@....@ @@...... @@......
    0xC0, 0xC0, 0xC0, 0x60, // @@...... @@...... @@...... .@@.....
    0xC0, 0xC0, 0xE0, 0x60, // @@...... @@...... @@@..... .@@.....
    0x60, 0x61, 0xB0, 0xC0, // .@@..... .@@....@ @.@@.... @@......
    0x30, 0x1E, 0x1F, 0x80, // ..@@.... ...@@@@. ...@@@@@ @.......
    0x0C, 0x00, 0x00, 0x00, // ....@@.. ........ ........ ........
    0x01, 0x80, 0x3C, 0x00, // .......@ @....... ..@@@@.. ........
    0x00, 0x3F, 0x80, 0x00  // ........ ..@@@@@@ @....... ........
}; // Width = 27 bits, height = 12 bytes

// load_image2 is what actually looks like load_image's ascii art when rendered
static const unsigned char load_image2[96] = {
    0x00, 0x3F, 0x80, 0x00, // ........ ..@@@@@@ @....... ........
    0x00, 0x3F, 0x80, 0x00, // ........ ..@@@@@@ @....... ........
    0x01, 0x80, 0x30, 0x00, // .......@ @....... ..@@.... ........
    0x01, 0x80, 0x30, 0x00, // .......@ @....... ..@@.... ........
    0x0C, 0x00, 0x06, 0x00, // ....@@.. ........ .....@@. ........
    0x0C, 0x00, 0x06, 0x00, // ....@@.. ........ .....@@. ........
    0x30, 0x1E, 0xE1, 0x80, // ..@@.... ...@@@@. @@@....@ @.......
    0x30, 0x1E, 0xE1, 0x80, // ..@@.... ...@@@@. @@@....@ @.......
    0x60, 0x61, 0xC0, 0xC0, // .@@..... .@@....@ @@...... @@......
    0x60, 0x61, 0xC0, 0xC0, // .@@..... .@@....@ @@...... @@......
    0xC0, 0xC0, 0xC0, 0x60, // @@...... @@...... @@...... .@@.....
    0xC0, 0xC0, 0xC0, 0x60, // @@...... @@...... @@...... .@@.....
    0xC0, 0xC0, 0xE0, 0x60, // @@...... @@...... @@@..... .@@.....
    0xC0, 0xC0, 0xE0, 0x60, // @@...... @@...... @@@..... .@@.....
    0x60, 0x61, 0xB0, 0xC0, // .@@..... .@@....@ @.@@.... @@......
    0x60, 0x61, 0xB0, 0xC0, // .@@..... .@@....@ @.@@.... @@......
    0x30, 0x1E, 0x1F, 0x80, // ..@@.... ...@@@@. ...@@@@@ @.......
    0x30, 0x1E, 0x1F, 0x80, // ..@@.... ...@@@@. ...@@@@@ @.......
    0x0C, 0x00, 0x00, 0x00, // ....@@.. ........ ........ ........
    0x0C, 0x00, 0x00, 0x00, // ....@@.. ........ ........ ........
    0x01, 0x80, 0x3C, 0x00, // .......@ @....... ..@@@@.. ........
    0x01, 0x80, 0x3C, 0x00, // .......@ @....... ..@@@@.. ........
    0x00, 0x3F, 0x80, 0x00, // ........ ..@@@@@@ @....... ........
    0x00, 0x3F, 0x80, 0x00  // ........ ..@@@@@@ @....... ........
}; // Width = 27 bits, height = 24 bytes

static const unsigned char load_image3[144] = {
    0x00, 0x3F, 0x80, 0x00, // ........ ..@@@@@@ @....... ........
    0x00, 0x3F, 0x80, 0x00, // ........ ..@@@@@@ @....... ........
    0x00, 0x3F, 0x80, 0x00, // ........ ..@@@@@@ @....... ........
    0x01, 0x80, 0x30, 0x00, // .......@ @....... ..@@.... ........
    0x01, 0x80, 0x30, 0x00, // .......@ @....... ..@@.... ........
    0x01, 0x80, 0x30, 0x00, // .......@ @....... ..@@.... ........
    0x0C, 0x00, 0x06, 0x00, // ....@@.. ........ .....@@. ........
    0x0C, 0x00, 0x06, 0x00, // ....@@.. ........ .....@@. ........
    0x0C, 0x00, 0x06, 0x00, // ....@@.. ........ .....@@. ........
    0x30, 0x1E, 0xE1, 0x80, // ..@@.... ...@@@@. @@@....@ @.......
    0x30, 0x1E, 0xE1, 0x80, // ..@@.... ...@@@@. @@@....@ @.......
    0x30, 0x1E, 0xE1, 0x80, // ..@@.... ...@@@@. @@@....@ @.......
    0x60, 0x61, 0xC0, 0xC0, // .@@..... .@@....@ @@...... @@......
    0x60, 0x61, 0xC0, 0xC0, // .@@..... .@@....@ @@...... @@......
    0x60, 0x61, 0xC0, 0xC0, // .@@..... .@@....@ @@...... @@......
    0xC0, 0xC0, 0xC0, 0x60, // @@...... @@...... @@...... .@@.....
    0xC0, 0xC0, 0xC0, 0x60, // @@...... @@...... @@...... .@@.....
    0xC0, 0xC0, 0xC0, 0x60, // @@...... @@...... @@...... .@@.....
    0xC0, 0xC0, 0xE0, 0x60, // @@...... @@...... @@@..... .@@.....
    0xC0, 0xC0, 0xE0, 0x60, // @@...... @@...... @@@..... .@@.....
    0xC0, 0xC0, 0xE0, 0x60, // @@...... @@...... @@@..... .@@.....
    0x60, 0x61, 0xB0, 0xC0, // .@@..... .@@....@ @.@@.... @@......
    0x60, 0x61, 0xB0, 0xC0, // .@@..... .@@....@ @.@@.... @@......
    0x60, 0x61, 0xB0, 0xC0, // .@@..... .@@....@ @.@@.... @@......
    0x30, 0x1E, 0x1F, 0x80, // ..@@.... ...@@@@. ...@@@@@ @.......
    0x30, 0x1E, 0x1F, 0x80, // ..@@.... ...@@@@. ...@@@@@ @.......
    0x30, 0x1E, 0x1F, 0x80, // ..@@.... ...@@@@. ...@@@@@ @.......
    0x0C, 0x00, 0x00, 0x00, // ....@@.. ........ ........ ........
    0x0C, 0x00, 0x00, 0x00, // ....@@.. ........ ........ ........
    0x0C, 0x00, 0x00, 0x00, // ....@@.. ........ ........ ........
    0x01, 0x80, 0x3C, 0x00, // .......@ @....... ..@@@@.. ........
    0x01, 0x80, 0x3C, 0x00, // .......@ @....... ..@@@@.. ........
    0x01, 0x80, 0x3C, 0x00, // .......@ @....... ..@@@@.. ........
    0x00, 0x3F, 0x80, 0x00, // ........ ..@@@@@@ @....... ........
    0x00, 0x3F, 0x80, 0x00, // ........ ..@@@@@@ @....... ........
    0x00, 0x3F, 0x80, 0x00  // ........ ..@@@@@@ @....... ........
}; // Width = 27 bits, height = 36 bytes
// Output_render_text will ignore the last 5 bits of zeros in each row if width is specified as 27.

// To print text requires a bitmap font.
// NOTE: Using Output_render_bitmap() instead of Output_render_text() technically allows any arbitrary font to be used as long as it is stored the same way as the included font8x8.
// A character would need to be passed as otherfont['a'] instead of just 'a' in this case.

//----------------------------------------------------------------------------------------------------------------------------------
// kernel_main: Main Function
//----------------------------------------------------------------------------------------------------------------------------------
//
// The main entry point of the kernel/program/OS and what the bootloader hands off to.
//

// Can't use local variables in a naked function (at least, the compiler won't allow it, though it can be done with assembly)
// Make them static globals instead, which still keeps them essentially local (local to this file, at any rate).
// They'll just take up some program RAM instead of stack space.
static unsigned char swapped_image[sizeof(load_image2)] = {0};
static char brandstring[48] = {0};
static char Manufacturer_ID[13] = {0};

__attribute__((naked)) void kernel_main(LOADER_PARAMS * LP) // Loader Parameters
{
  // First things first, set up a stack.
  // This is why having a naked function is important, otherwise the kernel would still be using the UEFI's stack as part of the function prolog (or prologue, if you must).
  asm volatile ("leaq %[new_stack_base], %%rbp\n\t"
                "leaq %[new_stack_end], %%rsp \n\t"
                : // No outputs
                : [new_stack_base] "m" (kernel_stack[0]), [new_stack_end] "m" (kernel_stack[STACK_SIZE]) // Inputs; %rsp is decremented before use, so STACK_SIZE is used instead of STACK_SIZE - 1
                : // No clobbers
              );

  // Now initialize the system (Virtual mappings (identity-map), printf, AVX, any straggling control registers, HWP, maskable interrupts)
  System_Init(LP); // See System.c for what this does. One step is involves calling a function that can re-assign printf to a different GPU.

  // Main Body Start

//  bitmap_bitswap(load_image, 12, 27, swapped_image);
//  char swapped_image2[sizeof(load_image)];
//  bitmap_bytemirror(swapped_image, 12, 27, swapped_image2);
  bitmap_bitreverse(load_image2, 24, 27, swapped_image);
  for(UINT64 k = 0; k < LP->GPU_Configs->NumberOfFrameBuffers; k++) // Multi-GPU support!
  {
    bitmap_anywhere_scaled(LP->GPU_Configs->GPUArray[k], swapped_image, 24, 27, 0x0000FFFF, 0xFF000000, ((LP->GPU_Configs->GPUArray[k].Info->HorizontalResolution - 5*27) >>  1), ((LP->GPU_Configs->GPUArray[k].Info->VerticalResolution - 5*24) >> 1), 5);
  }


  Print_Loader_Params(LP);
  Print_Segment_Registers();

  Get_Brandstring((uint32_t*)brandstring); // Returns a char* pointer to brandstring. Don't need it here, though.
  printf("%.48s\r\n", brandstring);

  Get_Manufacturer_ID(Manufacturer_ID); // Returns a char* pointer to Manufacturer_ID. Don't need it here, though.
  printf("%s\r\n\n", Manufacturer_ID);

  print_system_memmap();
/*
  printf("Avg CPU freq: %qu\r\n", get_CPU_freq(NULL, 0));
  uint64_t perfcounters[2] = {1, 1};
  read_perfs_initial(perfcounters);

  // Code to measure the CPU frequency of
  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "PerfLoop:\n\t"
                "addl $1, %%eax\n\t"
                "jnz PerfLoop\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );
  // end of code to measure the CPU frequency of

  printf("Perfcounter CPU freq: %qu\r\n", get_CPU_freq(perfcounters, 1));
  printf("Avg CPU freq: %qu\r\n", get_CPU_freq(NULL, 0));
  printf("\r\n\r\n");
*/

//  Print_All_CRs_and_Some_Major_CPU_Features(); // The output from this will fill up a 768 vertical resolution screen with an 8 height font set to scale factor 1.

  // ASM so that GCC doesn't mess with this loop. This is about as optimized this can get.
  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "Loop0:\n\t"
                "addl $1, %%eax\n\t"
                "testl %%eax, %%eax\n\t"
                "jne Loop0\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );

  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "Loop7:\n\t"
                "addl $1, %%eax\n\t"
                "testl %%eax, %%eax\n\t"
                "jne Loop7\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );

  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "Loop8:\n\t"
                "addl $1, %%eax\n\t"
                "testl %%eax, %%eax\n\t"
                "jne Loop8\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );

  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "Loop9:\n\t"
                "addl $1, %%eax\n\t"
                "testl %%eax, %%eax\n\t"
                "jne Loop9\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );

  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "Loop10:\n\t"
                "addl $1, %%eax\n\t"
                "testl %%eax, %%eax\n\t"
                "jne Loop10\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );

  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "Loop11:\n\t"
                "addl $1, %%eax\n\t"
                "testl %%eax, %%eax\n\t"
                "jne Loop11\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );

  Colorscreen(LP->GPU_Configs->GPUArray[0], 0x000000FF); // Blue in BGRX (X = reserved, technically an "empty alpha channel" for 32-bit memory alignment)
  single_char(LP->GPU_Configs->GPUArray[0], '?', 8, 8, 0x00FFFFFF, 0x00000000);
  single_char_anywhere(LP->GPU_Configs->GPUArray[0], '!', 8, 8, 0x00FFFFFF, 0xFF000000, (LP->GPU_Configs->GPUArray[0].Info->HorizontalResolution >> 2), LP->GPU_Configs->GPUArray[0].Info->VerticalResolution/3);
  single_char_anywhere_scaled(LP->GPU_Configs->GPUArray[0], 'H', 8, 8, 0x00FFFFFF, 0xFF000000, 10, 10, 5);
  string_anywhere_scaled(LP->GPU_Configs->GPUArray[0], "Is it soup?", 8, 8, 0x00FFFFFF, 0x00000000, 10, 10, 1);

  // ASM so that GCC doesn't mess with this loop. This is about as optimized this can get.
  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "Loop1:\n\t"
                "addl $1, %%eax\n\t"
                "jnz Loop1\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );

  Colorscreen(LP->GPU_Configs->GPUArray[0], 0x0000FF00); // Green in BGRX (X = reserved, technically an "empty alpha channel" for 32-bit memory alignment)
  single_char(LP->GPU_Configs->GPUArray[0], 'A', 8, 8, 0x00FFFFFF, 0x00000000);
  single_char_anywhere(LP->GPU_Configs->GPUArray[0], '!', 8, 8, 0x00FFFFFF, 0xFF000000, (LP->GPU_Configs->GPUArray[0].Info->HorizontalResolution >> 2), LP->GPU_Configs->GPUArray[0].Info->VerticalResolution/3);
  string_anywhere_scaled(LP->GPU_Configs->GPUArray[0], "Is it really soup?", 8, 8, 0x00FFFFFF, 0x00000000, 50, 50, 3);

  // ASM so that GCC doesn't mess with this loop. This is about as optimized this can get.
  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "Loop2:\n\t"
                "addl $1, %%eax\n\t"
                "jnz Loop2\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );

  Colorscreen(LP->GPU_Configs->GPUArray[0], 0x00FF0000); // Red in BGRX (X = reserved, technically an "empty alpha channel" for 32-bit memory alignment)
  printf("PRINTF!! 0x%qx", LP->GPU_Configs->GPUArray[0].FrameBufferBase);
  printf("Whup %s\r\nOh.\r\n", "Yo%%nk");

  Global_Print_Info.scale = 4; // Output scale for systemfont used by printf
  Global_Print_Info.textscrollmode = Global_Print_Info.height*Global_Print_Info.scale; // Quick scrolling

  printf("Hello this is a sentence how far does it go before it wraps around?\nA\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ\nYAY");
  printf("Hello this is a sentence how far does it go before it wraps around?\nA\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ\nYAY");
  printf("Hello this is a sentence how far does it go before it wraps around?\nA\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ\nYAY");
  printf("Hello this is a sentence how far does it go before it wraps around?\nA\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ\nYAY");
  formatted_string_anywhere_scaled(LP->GPU_Configs->GPUArray[0], 8, 8, 0x00FFFFFF, 0x00000000, 0,  LP->GPU_Configs->GPUArray[0].Info->VerticalResolution/2, 2, "FORMATTED STRING!! %#x", Global_Print_Info.index);
  formatted_string_anywhere_scaled(LP->GPU_Configs->GPUArray[0], 8, 8, 0x00FFFFFF, 0x00000000, 0,  LP->GPU_Configs->GPUArray[0].Info->VerticalResolution/4, 2, "FORMATTED %s STRING!! %s", "Heyo!", "Heyz!");
  printf("This printf shouldn't move due to formatted string invocation.");
  single_char(LP->GPU_Configs->GPUArray[0], '2', 8, 8, 0x00FFFFFF, 0xFF000000);

  // ASM so that GCC doesn't mess with this loop. This is about as optimized this can get.
  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "Loop3:\n\t"
                "addl $1, %%eax\n\t"
                "jnz Loop3\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );

  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "Loop5:\n\t"
                "addl $1, %%eax\n\t"
                "jnz Loop5\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );

  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "Loop6:\n\t"
                "addl $1, %%eax\n\t"
                "jnz Loop6\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );

  Blackscreen(LP->GPU_Configs->GPUArray[0]); // X in BGRX (X = reserved, technically an "empty alpha channel" for 32-bit memory alignment)
  single_pixel(LP->GPU_Configs->GPUArray[0], LP->GPU_Configs->GPUArray[0].Info->HorizontalResolution >> 2, LP->GPU_Configs->GPUArray[0].Info->VerticalResolution >> 2, 0x00FFFFFF);
  single_char(LP->GPU_Configs->GPUArray[0], '@', 8, 8, 0x00FFFFFF, 0x00000000);
  single_char_anywhere(LP->GPU_Configs->GPUArray[0], '!', 8, 8, 0x00FFFFFF, 0xFF000000, 512, 512);
  single_char_anywhere_scaled(LP->GPU_Configs->GPUArray[0], 'I', 8, 8, 0x00FFFFFF, 0xFF000000, 10, 10, 2);
  string_anywhere_scaled(LP->GPU_Configs->GPUArray[0], "OMG it's actually soup! I don't believe it!!", 8, 8, 0x00FFFFFF, 0x00000000, 0, LP->GPU_Configs->GPUArray[0].Info->VerticalResolution/2, 2);

  // ASM so that GCC doesn't mess with this loop. This is about as optimized this can get.
  asm volatile("movl $1, %%eax\n\t" // Loop ends on overflow
                "Loop4:\n\t"
                "addl $1, %%eax\n\t"
                "jnz Loop4\n\t" // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
                : // no outputs
                : // no inputs
                : // no clobbers
              );

  LP->RTServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL); // Shutdown the system
}
// END MAIN

//----------------------------------------------------------------------------------------------------------------------------------
// Print_All_CRs_and_Some_Major_CPU_Features: Print Common CPU Parameters of Interest
//----------------------------------------------------------------------------------------------------------------------------------
//
// Prints the status of all non-reserved control registers, in addition to querying CPUID for many common features and points of interest.
//
// The output from this will fill up a 768 vertical resolution screen with an 8 height font set to scale factor 1.
//

void Print_All_CRs_and_Some_Major_CPU_Features(void)
{
  uint64_t cr0 = control_register_rw(0, 0, 0);
  printf("CR0: %#qx\r\n", cr0);
//  printf("CR1: %#qx\r\n", control_register_rw(1, 0, 0)); // Reserved, will crash without exception handlers
  uint64_t cr2 = control_register_rw(2, 0, 0);
  printf("CR2: %#qx\r\n", cr2);
  uint64_t cr3 = control_register_rw(3, 0, 0);
  printf("CR3: %#qx\r\n", cr3);
  uint64_t cr4 = control_register_rw(4, 0, 0);
  printf("CR4: %#qx\r\n", cr4);
  uint64_t cr8 = control_register_rw(8, 0, 0);
  printf("CR8: %#qx\r\n", cr8);
  uint64_t efer = msr_rw(0xC0000080, 0, 0);
  printf("IA32_EFER: %#qx\r\n", efer);
  uint64_t rflags = control_register_rw('f', 0, 0);
  printf("RFLAGS: %#qx\r\n", rflags);
  // Checking CPUID means determining if bit 21 of FLAGS can be toggled
  uint64_t rflags2 = rflags ^ (1 << 21);
  control_register_rw('f', rflags2, 1);
  rflags2 = control_register_rw('f', 0, 0);
  // Reading CS to get GDT entry to check for 64-bit mode
  uint64_t cs = read_cs();
  printf("CS: %#qx\r\n", cs);

  // Decode some of the results from the above hex
  printf("\r\n");
  if(cr0 & 0x01)
  {
    printf("Protected mode is enabled. (CR0.PE = 1)\r\n");
  }
  if(cr0 & (1 << 31))
  {
    printf("Paging is enabled. (CR0.PG = 1)\r\n");
  }
  if(cr0 & (1 << 1))
  {
    printf("SSE: CR0.MP = 1\r\n");
  }
  else
  {
    printf("SSE: CR0.MP = 0, need to enable\r\n");
  }
  if(cr0 & (1 << 2))
  {
    printf("SSE: CR0.EM = 1, need to disable\r\n");
  }
  else
  {
    printf("SSE: CR0.EM = 0\r\n");
  }
  if(cr0 & (1 << 3))
  {
    printf("SSE: CR0.TS = 1, need to disable\r\n");
  }
  else
  {
    printf("SSE: CR0.TS = 0\r\n");
  }
  if(cr4 & (1 << 5))
  {
    printf("PAE is enabled. (CR4.PAE = 1)\r\n");
  }
  if(cr4 & (1 << 9))
  {
    printf("SSE: CR4.OSFXSR = 1\r\n");
  }
  else
  {
    printf("SSE: CR4.OSFXSR = 0\r\n");
  }
  if(cr4 & (1 << 10))
  {
    printf("SSE: CR4.OSXMMEXCPT = 1\r\n");
  }
  else
  {
    printf("SSE: CR4.OSXMMEXCPT = 0\r\n");
  }
  if(cr4 & (1 << 18))
  {
    printf("SSE/AVX: CR4.OSXSAVE = 1\r\n");
  }
  else
  {
    printf("SSE/AVX: CR4.OSXSAVE = 0\r\n");
  }
  // Verify we're in long mode (UEFI by default should have put us there)
  if((efer & 0x500) == 0x500)
  {
    printf("Long mode is enabled and active. (IA32e.LME = 1 & IA32e.LMA = 1)\r\n");
  }
  else
  {
    printf("For some reason long mode is not enabled and active.\r\n");
  }
  if(rflags & (1 << 9))
  {
    printf("Interrupts are enabled. (IF = 1)\r\n");
  }
  else
  {
    printf("Interrupts are disabled. (IF = 0)\r\n");
  }

  uint16_t gdt_index = cs >> 3; // This index is how many GDT_ENTRY_STRUCTs above GDT BaseAddress the current code segment is
  // Check if 64-bit mode's all set to go.
  DT_STRUCT gdt = get_gdtr(); // GDT is up to 64k from base addr, but CS points to max index in x86_64
  printf("GDTR addr: %#qx, limit: %#hx\r\n", gdt.BaseAddress, gdt.Limit);

  printf("CS GDT Entry: %#qx\r\n", ((GDT_ENTRY_STRUCT *)gdt.BaseAddress)[gdt_index]);
//  printf("CS GDT Entry: %#qx\r\n", *((GDT_ENTRY_STRUCT *)(gdt.BaseAddress + 8*gdt_index)) ); // Same thing

  if( (((GDT_ENTRY_STRUCT *)gdt.BaseAddress)[gdt_index].SegmentLimit2andMisc2 & (1 << 6)) == 0 ) // CS.D = 0 means "not in 32-bit mode" (either 16- or 64-bit mode)
  {
    if( ((GDT_ENTRY_STRUCT *)gdt.BaseAddress)[gdt_index].SegmentLimit2andMisc2 & (1 << 5) ) // CS.L = 1 means 64-bit mode if CS.D = 0.
    {
      printf("All good: 64-bit mode enabled. (CS.D = 0, CS.L = 1)\r\n");
    }
  }

  if(rflags2 == rflags)
  {
    printf("CPUID is not supported.\r\n");
  }
  else
  {
    printf("CPUID is supported.\r\n");
    printf("\r\n");
    cpu_features(0, 0);
    printf("\r\n");
    cpu_features(1, 0);
    printf("\r\n");
    cpu_features(7, 0);
    printf("\r\n");
    cpu_features(0x80000000, 0);
    printf("\r\n");
    cpu_features(0x0D, 0);
    printf("\r\n");
    cpu_features(0x0D, 1);
    printf("\r\n");
    cpu_features(0x80000001, 0);
    printf("\r\n");
    cpu_features(0x80000006, 0);
    printf("\r\n");
    cpu_features(0x80000008, 0);
    printf("\r\n");
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
// Print_Loader_Params: Print Loader Parameter Block Values
//----------------------------------------------------------------------------------------------------------------------------------
//
// Prints the values and addresses contained within the loader parameter block
//

void Print_Loader_Params(LOADER_PARAMS * LP)
{
  printf("Loader_Params check:\r\n Bootloader Version: %hu.%hu\r\n MemMap Desc Ver: %u, MemMap Desc Size: %llu, MemMap Addr: %#qx, MemMap Size: %llu\r\n Kernel Base: %#qx, Kernel Pages: %llu\r\n",
  LP->Bootloader_MajorVersion,
  LP->Bootloader_MinorVersion,

  LP->Memory_Map_Descriptor_Version,
  LP->Memory_Map_Descriptor_Size,
  LP->Memory_Map,
  LP->Memory_Map_Size,

  LP->Kernel_BaseAddress,
  LP->Kernel_Pages
  );

  printf(" ESP Root Path: ");
  print_utf16_as_utf8(LP->ESP_Root_Device_Path, LP->ESP_Root_Size);
  printf(", ESP Root Size: %llu\r\n Kernel Path: ", LP->ESP_Root_Size);
  print_utf16_as_utf8(LP->Kernel_Path, LP->Kernel_Path_Size);
  printf(", Kernel Path Size: %llu\r\n Kernel Options: ", LP->Kernel_Path_Size);
  print_utf16_as_utf8(LP->Kernel_Options, LP->Kernel_Options_Size);
  printf(", Kernel Options Size: %llu\r\n", LP->Kernel_Options_Size);

  printf(" RTServices Addr: %#qx, GPU_Configs Addr: %#qx, FileMeta Addr: %#qx, RSDP Addr: %#qx\r\n",
  LP->RTServices,
  LP->GPU_Configs,
  LP->FileMeta,
  LP->RSDP
  );
}

//----------------------------------------------------------------------------------------------------------------------------------
// Print_Segment_Registers: Print Segment Register Values
//----------------------------------------------------------------------------------------------------------------------------------
//
// Prints the values and addresses contained within the segment registers (GDTR, IDTR, LDTR, TSR)
//

void Print_Segment_Registers(void)
{
  uint64_t cr3 = control_register_rw(3, 0, 0); // CR3 has the page directory base (bottom 12 bits of address are assumed 0)
  printf("CR3: %#qx\r\n", cr3);

  DT_STRUCT gdt = get_gdtr();
  printf("GDTR addr: %#qx, limit: %#hx\r\n", gdt.BaseAddress, gdt.Limit);

  DT_STRUCT idt = get_idtr(); // IDT can have up to 256 interrupt descriptors to account for. (IRQs 16-255 can be handled by APIC, 0-255 can be handled by INTR pin)
  printf("IDTR addr: %#qx, limit: %#hx\r\n", idt.BaseAddress, idt.Limit);

  uint16_t ldt_ss = get_ldtr();
  printf("LDTR Seg Sel: %#hx\r\n", ldt_ss);

  uint16_t tsr_ss = get_tsr();
  printf("TSR Seg Sel: %#hx\r\n", tsr_ss);

  uint64_t cs = read_cs();
  printf("CS: %#qx\r\n", cs);


  // TODO: remove this
  __m256i_u whaty = _mm256_set1_epi32(0x17181920);
  __m256i_u what2 = _mm256_set1_epi64x(0x1718192011223344);
  __m256i_u what3 = _mm256_set1_epi32(0x18);
  __m256i_u what9 = _mm256_set1_epi32(0x180019);

  asm volatile("vmovdqu %[what], %%ymm1" : : [what] "m" (whaty) :);
  asm volatile("vmovdqu %[what], %%ymm2" : : [what] "m" (what2) :);
  asm volatile("vmovdqu %[what], %%ymm3" : : [what] "m" (what3) :);
  asm volatile("vmovdqu %[what], %%ymm4" : : [what] "m" (whaty) :);
  asm volatile("vmovdqu %[what], %%ymm5" : : [what] "m" (whaty) :);
  asm volatile("vmovdqu %[what], %%ymm6" : : [what] "m" (whaty) :);
  asm volatile("vmovdqu %[what], %%ymm7" : : [what] "m" (whaty) :);

  asm volatile("vmovdqu %[what], %%ymm15" : : [what] "m" (what9) :);

  volatile __m256i output = _mm256_bsrli_epi128(what2, 1);
//  asm volatile ("int3");
//  asm volatile ("int $32");

  volatile uint64_t c = cs / (cs >> 10); // TODO: remove this lol
}

////////////////////////////////////////////////////

// TODO: keyboard driver (PS/2 for starters, then USB)

/*
Note:

typedef enum {
  EfiResetCold, // Power cycle (hard off/on)
  EfiResetWarm, //
  EfiResetShutdown // Uh, normal shutdown.
} EFI_RESET_TYPE;

typedef
VOID
ResetSystem (IN EFI_RESET_TYPE ResetType, IN EFI_STATUS ResetStatus, IN UINTN DataSize, IN VOID *ResetData OPTIONAL);

RT->ResetSystem (EfiResetShutdown, EFI_SUCCESS, 0, NULL); // Shutdown the system
RT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL); // Hard reboot the system - the familiar restart
RT->ResetSystem (EfiResetWarm, EFI_SUCCESS, 0, NULL); // Soft reboot the system -

There is also EfiResetPlatformSpecific, but that's not really important. (Takes a standard 128-bit EFI_GUID as ResetData for a custom reset type)
*/
