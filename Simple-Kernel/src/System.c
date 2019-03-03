//==================================================================================================================================
//  Simple Kernel: System Initialization
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
// This file contains post-UEFI initialization functions, as well as register access functions, for x86-64 CPUs.
// Intel CPUs made before 2011 (i.e. earlier than Sandy Bridge) may work with most of these, but they're not officially supported.
// AMD CPUs older than Ryzen are also not officially supported, even though they may also work with most of these.
//

#include "Kernel64.h"

//----------------------------------------------------------------------------------------------------------------------------------
// System_Init: Initial Setup
//----------------------------------------------------------------------------------------------------------------------------------
//
// Initial setup after UEFI handoff
//

void System_Init(LOADER_PARAMS * LP)
{
  // This memory initialization stuff needs to go first.
  Global_Memory_Info.MemMap = LP->Memory_Map;
  Global_Memory_Info.MemMapSize = LP->Memory_Map_Size;
  Global_Memory_Info.MemMapDescriptorSize = LP->Memory_Map_Descriptor_Size;
  // Apparently some systems won't totally leave you be without setting a virtual address map (https://www.spinics.net/lists/linux-efi/msg14108.html
  // and https://mjg59.dreamwidth.org/3244.html). Well, fine; identity map it now and fuhgetaboutit.
  // This will modify the memory map (but not its size), and set Global_Memory_Info.MemMap.
  if(Set_Identity_VMAP(LP->RTServices) == NULL)
  {
    Global_Memory_Info.MemMap = LP->Memory_Map; // No virtual addressing possible, evidently.
  }
  // Don't merge any regions on the map until after SetVirtualAddressMap() has been called. After that call, it can be modified safely.

  // This function call is required to initialize printf. Set default GPU as GPU 0.
  // It can also be used to reset all global printf values and reassign a new default GPU at any time.
  Initialize_Global_Printf_Defaults(LP->GPU_Configs->GPUArray[0]);
  // Technically, printf is immediately usable now. I'd recommend waitng for AVX/SSE init just in case the compiler uses them when optimizing printf.

  Enable_AVX(); // ENABLING AVX ASAP
  // All good now.

  // I know this CR0.NE bit isn't always set by default. Set it.
  // Generate and handle exceptions in the modern way, per Intel SDM
  uint64_t cr0 = control_register_rw(0, 0, 0);
  if(!(cr0 & (1 << 5)))
  {
    uint64_t cr0_2 = cr0 ^ (1 << 5);
    control_register_rw(0, cr0_2, 1);
    cr0_2 = control_register_rw(0, 0, 0);
    if(cr0_2 == cr0)
    {
      printf("Error setting CR0.NE bit.\r\n");
    }
  }
// TODO: print memmap before and after to test
  print_system_memmap();

  // Set up the memory map for use with mallocX (X = 16, 32, 64)
  Setup_MemMap();

  // HWP
  Enable_HWP();

  // Interrupts
  // Exceptions and Non-Maskable Interrupts are always enabled.
  // Enable_Maskable_Interrupts() here

}

//----------------------------------------------------------------------------------------------------------------------------------
// get_tick: Read RDTSCP
//----------------------------------------------------------------------------------------------------------------------------------
//
// Finally, a way to tell time! Returns reference ticks since the last CPU reset.
// (Well, ok, technically UEFI has a runtime service to check the system time, this is mainly for CPU performance & cycle counting)
//

uint64_t get_tick(void)
{
    uint64_t high = 0, low = 0;
    asm volatile("rdtscp"
                  : "=a" (low), "=d" (high)
                  : // no inputs
                  : "%rcx"// clobbers
                  );
    return (high << 32 | low);
}

//----------------------------------------------------------------------------------------------------------------------------------
// Enable_AVX: Enable AVX/AVX2/AVX512
//----------------------------------------------------------------------------------------------------------------------------------
//
// Check for AVX/AVX512 support and enable it. Needed in order to use AVX functions like AVX_memmove, AVX_memcpy, AVX_memset, and
// AVX_memcmp
//

void Enable_AVX(void)
{
  // Checking CPUID means determining if bit 21 of R/EFLAGS can be toggled
  uint64_t rflags = control_register_rw('f', 0, 0);
//  printf("RFLAGS: %#qx\r\n", rflags);
  uint64_t rflags2 = rflags ^ (1 << 21);
  control_register_rw('f', rflags2, 1);
  rflags2 = control_register_rw('f', 0, 0);
  if(rflags2 == rflags)
  {
    printf("CPUID is not supported.\r\n");
  }
  else
  {
//    printf("CPUID is supported.\r\n");
// Check if OSXSAVE has already been set for some reason. Implies XSAVE support.
    uint64_t rbx = 0, rcx = 0, rdx = 0;
    asm volatile("cpuid"
                 : "=c" (rcx), "=d" (rdx) // Outputs
                 : "a" (0x01) // The value to put into %rax
                 : "%rbx" // CPUID would clobber any of the abcd registers not listed explicitly
               );

    if(rcx & (1 << 27))
    {
//      printf("AVX: OSXSAVE = 1\r\n");
// OSXSAVE has already been set, so it doesn't need to be re-set.
      if(rcx & (1 << 28))
      { // AVX is supported.
//        printf("AVX supported. Enabling AVX... ");
        uint64_t xcr0 = xcr_rw(0, 0, 0);
        xcr_rw(0, xcr0 | 0x7, 1);
        xcr0 = xcr_rw(0, 0, 0);

        if((xcr0 & 0x7) == 0x7)
        { // AVX successfully enabled, so we have that.
          // Now check AVX2 & AVX512
          asm volatile("cpuid"
                       : "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                       : "a" (0x07), "c" (0x00) // The values to put into %rax and %rcx
                       : // CPUID would clobber any of the abcd registers not listed explicitly
                    );

          // AVX512 feature check if AVX512 supported
          if(rbx & (1 << 16))
          { // AVX512 is supported
//            printf("AVX512F supported. Enabling... ");
            uint64_t xcr0 = xcr_rw(0, 0, 0);
            xcr_rw(0, xcr0 | 0xE7, 1);
            xcr0 = xcr_rw(0, 0, 0);

            if((xcr0 & 0xE7) == 0xE7)
            { // AVX512 sucessfully enabled
              Colorscreen(Global_Print_Info.defaultGPU, Global_Print_Info.background_color); // We can use SSE/AVX/AVX512-optimized functions now!
              printf("AVX512 enabled.\r\n");
              // All done here.
            }
            else
            {
              printf("Unable to set AVX512.\r\n");
            }
            printf("Checking other supported AVX512 features:\r\n");
            if(rbx & (1 << 17))
            {
              printf("AVX512DQ\r\n");
            }
            if(rbx & (1 << 21))
            {
              printf("AVX512_IFMA\r\n");
            }
            if(rbx & (1 << 26))
            {
              printf("AVX512PF\r\n");
            }
            if(rbx & (1 << 27))
            {
              printf("AVX512ER\r\n");
            }
            if(rbx & (1 << 28))
            {
              printf("AVX512CD\r\n");
            }
            if(rbx & (1 << 30))
            {
              printf("AVX512BW\r\n");
            }
            if(rbx & (1 << 31))
            {
              printf("AVX512VL\r\n");
            }
            if(rcx & (1 << 1))
            {
              printf("AVX512_VBMI\r\n");
            }
            if(rcx & (1 << 6))
            {
              printf("AVX512_VBMI2\r\n");
            }
            if(rcx & (1 << 11))
            {
              printf("AVX512VNNI\r\n");
            }
            if(rcx & (1 << 12))
            {
              printf("AVX512_BITALG\r\n");
            }
            if(rcx & (1 << 14))
            {
              printf("AVX512_VPOPCNTDQ\r\n");
            }
            if(rdx & (1 << 2))
            {
              printf("AVX512_4VNNIW\r\n");
            }
            if(rdx & (1 << 3))
            {
              printf("AVX512_4FMAPS\r\n");
            }
            printf("End of AVX512 feature check.\r\n");
          }
          else
          {
            Colorscreen(Global_Print_Info.defaultGPU, Global_Print_Info.background_color); // We can use SSE/AVX-optimized functions now! But not AVX512 ones.
            printf("AVX/AVX2 enabled.\r\n");
            printf("AVX512 not supported.\r\n");
          }
          // End AVX512 check

          if(rbx & (1 << 5))
          {
            printf("AVX2 supported.\r\n");
          }
          else
          {
            printf("AVX2 not supported.\r\n");
          }
          // Only have AVX to work with, then.
        }
        else
        {
          printf("Unable to set AVX.\r\n");
        }
      }
      else
      {
        printf("AVX not supported. Checking for latest SSE features:\r\n");
        if(rcx & (1 << 20))
        {
          printf("Up to SSE4.2 supported.\r\n");
        }
        else
        {
          if(rcx & (1 << 19))
          {
            printf("Up to SSE4.1 supported.\r\n");
          }
          else
          {
            if(rcx & (1 << 9))
            {
              printf("Up to SSSE3 supported.\r\n");
            }
            else
            {
              if(rcx & 1)
              {
                printf("Up to SSE3 supported.\r\n");
              }
              else
              {
                if(rdx & (1 << 26))
                {
                  printf("Up to SSE2 supported.\r\n");
                }
                else
                {
                  printf("This is one weird CPU to get this far. x86_64 mandates SSE2.\r\n");
                }
              }
            }
          }
        }
      }
    }
    else
    {
//      printf("AVX: OSXSAVE = 0\r\n");
// OSXSAVE has not yet been set. Set it.
      if(rcx & (1 << 26)) // XSAVE supported.
      {
//        printf("AVX: XSAVE supported. Enabling OSXSAVE... ");
        uint64_t cr4 = control_register_rw(4, 0, 0);
        control_register_rw(4, cr4 ^ (1 << 18), 1);
        cr4 = control_register_rw(4, 0, 0);

        if(cr4 & (1 << 18))
        { // OSXSAVE successfully enabled.
//          printf("OSXSAVE enabled.\r\n");

          if(rcx & (1 << 28))
          { // AVX is supported
//            printf("AVX supported. Enabling AVX... ");
            uint64_t xcr0 = xcr_rw(0, 0, 0);
            xcr_rw(0, xcr0 | 0x7, 1);
            xcr0 = xcr_rw(0, 0, 0);

            if((xcr0 & 0x7) == 0x7)
            { // AVX successfully enabled, so we have that.
              // Now check AVX2 & AVX512
              asm volatile("cpuid"
                           : "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                           : "a" (0x07), "c" (0x00) // The values to put into %rax and %rcx
                           : // CPUID would clobber any of the abcd registers not listed explicitly
                        );

              // AVX512 feature check if AVX512 supported
              if(rbx & (1 << 16))
              { // AVX512 is supported
//                printf("AVX512F supported. Enabling... ");
                uint64_t xcr0 = xcr_rw(0, 0, 0);
                xcr_rw(0, xcr0 | 0xE7, 1);
                xcr0 = xcr_rw(0, 0, 0);

                if((xcr0 & 0xE7) == 0xE7)
                { // AVX512 successfully enabled.
                  Colorscreen(Global_Print_Info.defaultGPU, Global_Print_Info.background_color); // We can use SSE/AVX/AVX512-optimized functions now!
                  printf("AVX512 enabled.\r\n");
                  // All done here.
                }
                else
                {
                  printf("Unable to set AVX512.\r\n");
                }
                printf("Checking other supported AVX512 features:\r\n");
                if(rbx & (1 << 17))
                {
                  printf("AVX512DQ\r\n");
                }
                if(rbx & (1 << 21))
                {
                  printf("AVX512_IFMA\r\n");
                }
                if(rbx & (1 << 26))
                {
                  printf("AVX512PF\r\n");
                }
                if(rbx & (1 << 27))
                {
                  printf("AVX512ER\r\n");
                }
                if(rbx & (1 << 28))
                {
                  printf("AVX512CD\r\n");
                }
                if(rbx & (1 << 30))
                {
                  printf("AVX512BW\r\n");
                }
                if(rbx & (1 << 31))
                {
                  printf("AVX512VL\r\n");
                }
                if(rcx & (1 << 1))
                {
                  printf("AVX512_VBMI\r\n");
                }
                if(rcx & (1 << 6))
                {
                  printf("AVX512_VBMI2\r\n");
                }
                if(rcx & (1 << 11))
                {
                  printf("AVX512VNNI\r\n");
                }
                if(rcx & (1 << 12))
                {
                  printf("AVX512_BITALG\r\n");
                }
                if(rcx & (1 << 14))
                {
                  printf("AVX512_VPOPCNTDQ\r\n");
                }
                if(rdx & (1 << 2))
                {
                  printf("AVX512_4VNNIW\r\n");
                }
                if(rdx & (1 << 3))
                {
                  printf("AVX512_4FMAPS\r\n");
                }
                printf("End of AVX512 feature check.\r\n");
              }
              else
              {
                Colorscreen(Global_Print_Info.defaultGPU, Global_Print_Info.background_color); // We can use SSE/AVX-optimized functions now! Just not AVX512 ones.
                printf("AVX/AVX2 enabled.\r\n");
                printf("AVX512 not supported.\r\n");
              }
              // End AVX512 check

              if(rbx & (1 << 5))
              {
                printf("AVX2 supported.\r\n");
              }
              else
              {
                printf("AVX2 not supported.\r\n");
              }
              // Only have AVX to work with, then.
            }
            else
            {
              printf("Unable to set AVX.\r\n");
            }
          }
          else
          {
            printf("AVX not supported. Checking for latest SSE features:\r\n");
            if(rcx & (1 << 20))
            {
              printf("Up to SSE4.2 supported.\r\n");
            }
            else
            {
              if(rcx & (1 << 19))
              {
                printf("Up to SSE4.1 supported.\r\n");
              }
              else
              {
                if(rcx & (1 << 9))
                {
                  printf("Up to SSSE3 supported.\r\n");
                }
                else
                {
                  if(rcx & 1)
                  {
                    printf("Up to SSE3 supported.\r\n");
                  }
                  else
                  {
                    if(rdx & (1 << 26))
                    {
                      printf("Up to SSE2 supported.\r\n");
                    }
                    else
                    {
                      printf("This is one weird CPU to get this far. x86_64 mandates SSE2.\r\n");
                    }
                  }
                }
              }
            }
          }
        }
        else
        {
          printf("Unable to set OSXSAVE in CR4.\r\n");
        }
      }
      else
      {
        printf("AVX: XSAVE not supported.\r\n");
      }
    }
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
// Enable_Maskable_Interrupts: Load Interrupt Descriptor Table and Enable Interrupts
//----------------------------------------------------------------------------------------------------------------------------------
//
// Exceptions and Non-Maskable Interrupts are always enabled.
// This is needed for things like keyboard input.
//

void Enable_Maskable_Interrupts(void)
{

  // TODO: load interrupt handler table


  uint64_t rflags = control_register_rw('f', 0, 0);
  if(rflags & (1 << 9))
  {
    // Interrupts are already enabled, do nothing.
    printf("Interrupts are already enabled.\r\n");
  }
  else
  {
    uint64_t rflags2 = rflags | (~0xFFFFFFFFFFFFFDFF); // Set bit 9

    control_register_rw('f', rflags2, 1);
    rflags2 = control_register_rw('f', 0, 0);
    if(rflags2 == rflags)
    {
      printf("Unable to enable maskable interrupts.\r\n");
    }
    else
    {
      printf("Maskable Interrupts enabled.\r\n");
    }
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
// Enable_HWP: Enable Hardware P-States
//----------------------------------------------------------------------------------------------------------------------------------
//
// Enable hardware power management (HWP) if available.
// Otherwise this will not do anything.
//
// Hardware P-States means you don't have to worry about implmenting CPU P-states transitions in software, as the CPU handles them
// autonomously. Intel introduced this feature on Skylake chips.
//

void Enable_HWP(void)
{
  uint64_t rax = 0;
  asm volatile("cpuid"
               : "=a" (rax) // Outputs
               : "a" (0x06) // The value to put into %rax
               : "%rbx", "%rcx", "%rdx" // CPUID clobbers all not-explicitly-used abcd registers
             );

  if(rax & (1 << 7)) // HWP is available
  {
    if(msr_rw(0x770, 0, 0) & 1)
    {
      printf("HWP is already enabled.\r\n");
    }
    else
    {
      msr_rw(0x770, 1, 1);
      if(msr_rw(0x770, 0, 0) & 1)
      {
        printf("HWP enabled.\r\n");
      }
      else
      {
        printf("Unable to set HWP.\r\n");
      }
    }
  }
  else
  {
    printf("HWP not supported.\r\n");
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
// Hypervisor_check: Are We Virtualized?
//----------------------------------------------------------------------------------------------------------------------------------
//
// Check a bit that Intel and AMD always set to 0. Some hypervisors like Windows 10 Hyper-V set it to 1 (don't know if all VMs do) to
// allow for this kind of check
//

uint8_t Hypervisor_check(void)
{
  // Hypervisor check
  uint64_t rcx;
  asm volatile("cpuid"
               : "=c" (rcx) // Outputs
               : "a" (0x01) // The value to put into %rax
               : "%rbx", "%rdx" // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
             );

  if(rcx & (1 << 31)) // 1 means hypervisor (i.e. in a VM), 0 means no hypervisor
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
// read_perfs_initial: Measure CPU Performance (Part 1)
//----------------------------------------------------------------------------------------------------------------------------------
//
// Takes an array of 2x uint64_t, fills first uint with aperf and 2nd with mperf.
// This will disable maskable interrupts, as it is expected that get_CPU_freq(perfs, 1) is called to process the result and re-enable
// the interrupts. The functions read_perfs_initial(perfs) and get_CPU_freq(perfs, 1) should sandwich the desired code to measure.
//
// May not work in hypervisors (definitely does not work in Windows 10 Hyper-V), but it's fine on real hardware.
//

uint8_t read_perfs_initial(uint64_t * perfs)
{
  // Check for hypervisor
  if(Hypervisor_check())
  {
    printf("Hypervisor detected. It's not safe to read CPU frequency MSRs. Returning 0...\r\n");
    return 0;
  }
  // OK, not in a hypervisor; continuing...

  // Disable maskable interrupts
  uint64_t rflags = control_register_rw('f', 0, 0);
  uint64_t rflags2 = rflags & 0xFFFFFFFFFFFFFDFF; // Clear bit 9

  control_register_rw('f', rflags2, 1);
  rflags2 = control_register_rw('f', 0, 0);
  if(rflags2 == rflags)
  {
    printf("read_perfs_initial: Unable to disable interrupts (maybe they are already disabled?). Results may be skewed.\r\n");
  }
  // Now we can safely continue without something like keyboard input messing up tests

  uint64_t turbocheck = msr_rw(0x1A0, 0, 0);
  if(turbocheck & (1 << 16))
  {
    printf("NOTE: Enhanced SpeedStep is enabled.\r\n");
  }
  if((turbocheck & (1ULL << 38)) == 0)
  {
    printf("NOTE: Turbo Boost is enabled.\r\n");
  }

  asm volatile("cpuid"
               : "=a" (turbocheck) // Outputs
               : "a" (0x06) // The value to put into %rax
               : "%rbx", "%rcx", "%rdx" // CPUID clobbers all not-explicitly-used abcd registers
             );

  if(turbocheck & (1 << 7)) // HWP is available
  {
    if(msr_rw(0x770, 0, 0) & 1)
    {
      printf("NOTE: HWP is enabled.\r\n");
    }
  }

  // Force serializing
  asm volatile("cpuid"
               : // No outputs
               : // No inputs
               : "%rax", "%rbx", "%rcx", "%rdx" // CPUID clobbers all not-explicitly-used abcd registers
             );

  perfs[0] = msr_rw(0xe8, 0, 0); // APERF
  perfs[1] = msr_rw(0xe7, 0, 0); // MPERF

  return 1; // Success
}

//----------------------------------------------------------------------------------------------------------------------------------
// get_CPU_freq: Measure CPU Performance (Part 2)
//----------------------------------------------------------------------------------------------------------------------------------
//
// Get CPU frequency in MHz
// May not work in hypervisors (definitely does not work in Windows 10 Hyper-V), but it's fine on real hardware
//
// avg_or_measure: avg freq = 0 (ignores perfs argument), measuring freq during piece of code = 1
//
// In order to use avg_or_measure = 1, perfs array *MUST* have been filled by read_perfs_initial().
// avg_or_measure = 0 has no such restriction, as it just measures from the last time aperf and mperf were 0, which is either the
// last CPU reset state or last manual reset/overflow of aperf and mperf.
//

uint64_t get_CPU_freq(uint64_t * perfs, uint8_t avg_or_measure)
{
  uint64_t rax = 0, rbx = 0, rcx = 0, maxleaf = 0;
  uint64_t aperf = 1, mperf = 1; // Don't feel like dealing with division by zero.
  uint64_t rflags = 0, rflags2 = 0;

  // Check function argument
  if(avg_or_measure == 1) // Measurement for some piece of code is being done.
  {
    // Force serializing
    asm volatile("cpuid"
                 : // No outputs
                 : // No inputs
                 : "%rax", "%rbx", "%rcx", "%rdx" // CPUID clobbers all not-explicitly-used abcd registers
               );

    uint64_t aperf2 = msr_rw(0xe8, 0, 0); // APERF
    uint64_t mperf2 = msr_rw(0xe7, 0, 0); // MPERF

    aperf = aperf2 - perfs[0]; // Adjusted APERF
    mperf = mperf2 - perfs[1]; // Adjusted MPERF
  }
  else // Avg since reset, or since aperf/mperf overflowed (once every 150-300 years at 2-4 GHz) or were manually zeroed
  {
    // Check for hypervisor
    if(Hypervisor_check())
    {
      printf("Hypervisor detected. It's not safe to read CPU frequency MSRs. Returning 0...\r\n");
      return 0;
    }
    // OK, not in a hypervisor; continuing...

    // Disable maskable interrupts
    rflags = control_register_rw('f', 0, 0);
    rflags2 = rflags & 0xFFFFFFFFFFFFFDFF; // Clear bit 9

    control_register_rw('f', rflags2, 1);
    rflags2 = control_register_rw('f', 0, 0);
    if(rflags2 == rflags)
    {
      printf("get_CPU_freq: Unable to disable interrupts (maybe they are already disabled?). Results may be skewed.\r\n");
    }
    // Now we can safely continue without something like keyboard input messing up tests

    uint64_t turbocheck = msr_rw(0x1A0, 0, 0);
    if(turbocheck & (1 << 16))
    {
      printf("NOTE: Enhanced SpeedStep is enabled.\r\n");
    }
    if((turbocheck & (1ULL << 38)) == 0)
    {
      printf("NOTE: Turbo Boost is enabled.\r\n");
    }

    asm volatile("cpuid"
                 : "=a" (turbocheck) // Outputs
                 : "a" (0x06) // The value to put into %rax
                 : "%rbx", "%rcx", "%rdx" // CPUID clobbers all not-explicitly-used abcd registers
               );

    if(turbocheck & (1 << 7)) // HWP is available
    {
      if(msr_rw(0x770, 0, 0) & 1)
      {
        printf("NOTE: HWP is enabled.\r\n");
      }
    }

    // Force serializing
    asm volatile("cpuid"
                 : // No outputs
                 : // No inputs
                 : "%rax", "%rbx", "%rcx", "%rdx" // CPUID clobbers all not-explicitly-used abcd registers
               );

    aperf = msr_rw(0xe8, 0, 0); // APERF
    mperf = msr_rw(0xe7, 0, 0); // MPERF
  }

  // This will force serializing, though we need the output from CPUID anyways.
  // 2 birds with 1 stone.
  asm volatile("cpuid"
               : "=a" (maxleaf) // Outputs
               : "a" (0x00) // The value to put into %rax
               : "%rbx", "%rcx", "%rdx" // CPUID clobbers all not-explicitly-used abcd registers
             );

  // Needed for Sandy Bridge, Ivy Bridge, and possibly Haswell, Broadwell -- They all have constant TSC found this way.
  uint64_t max_non_turbo_ratio = (msr_rw(0xCE, 0, 0) & 0x000000000000FF00) >> 8; // Max non-turbo bus multiplier is in this byte
  uint64_t tsc_frequency = max_non_turbo_ratio * 100; // 100 MHz bus for these CPUs, 133 MHz for Nehalem (but Nehalem doesn't have AVX)

// DEBUG
//  printf("aperf: %qu, mperf: %qu\r\n", aperf, mperf);
//  printf("Nonturbo-ratio: %qu, tsc MHz: %qu\r\n", max_non_turbo_ratio, tsc_frequency);

  if(maxleaf >= 0x15)
  {
    rax = 0x15;
    asm volatile("cpuid"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx) // Outputs
                 : "a" (rax) // The value to put into %rax
                 : "%rdx" // CPUID clobbers all not-explicitly-used abcd registers
               );
//DEBUG
//    printf("rax: %qu, rbx: %qu, rcx: %qu\r\n b/a *c: %qu\r\n", rax, rbx, rcx, (rcx * rbx)/rax);

    if((rcx != 0) && (rbx != 0))
    {
      // rcx is nominal frequency of clock in Hz
      // TSC freq (in Hz) from Crystal OSC = (rcx * rbx)/rax
      // Intel's CPUID reference does not make it clear that ebx/eax is TSC's *frequency* / Core crystal clock
      return ((rcx/1000000) * rbx * aperf)/(rax * mperf); // MHz
    }
    // (rbx/rax) is (TSC/core crystal clock), so rbx/rax is to nominal crystal
    // clock (in rcx in rcx != 0) as aperf/mperf is to max frequency (that is to
    // say, they're scaling factors)
    if(rcx == 0)
    {
      maxleaf = 0x01; // Don't need maxleaf any more, so reuse it for this
      asm volatile("cpuid"
                   : "=a" (maxleaf) // Outputs
                   : "a" (maxleaf) // The value to put into %rax
                   : "%rbx", "%rcx", "%rdx" // CPUID clobbers any of the abcd registers not listed explicitly
                 );

      uint64_t maxleafmask = maxleaf & 0xF0FF0;

//DEBUG
//      printf("maxleafmask: %qu\r\n", maxleafmask);
      // Intel family 0x06, models 4E, 5E, 8E, and 9E have a known clock of 24 MHz
      // See tsc.c in the Linux kernel: https://github.com/torvalds/linux/blob/master/arch/x86/kernel/tsc.c
      // It's also in Intel SDM, Vol. 3, Ch. 18.7.3
      if( (maxleafmask == 0x906E0) || (maxleafmask == 0x806E0) || (maxleafmask == 0x506E0) || (maxleafmask == 0x406E0) )
      {
        return (24 * rbx * aperf)/(rax * mperf); // MHz
      }
      // Else: Argh...
    }
  }
  // At this point CPUID won't help.

  // Freq = TSCFreq * delta(APERF)/delta(MPERF)
  uint64_t frequency = (tsc_frequency * aperf) / mperf; // CPUID didn't help, so fall back to Sandy Bridge method.

  // All done, so re-enable maskable interrupts
  // It is possible that RFLAGS changed since it was last read...
  rflags = control_register_rw('f', 0, 0);
  rflags2 = rflags | (~0xFFFFFFFFFFFFFDFF); // Set bit 9

  control_register_rw('f', rflags2, 1);
  rflags2 = control_register_rw('f', 0, 0);
  if(rflags2 == rflags)
  {
    printf("get_CPU_freq: Unable to re-enable interrupts.\r\n");
  }

  return frequency;
}

//----------------------------------------------------------------------------------------------------------------------------------
// msr_rw: Read/Write Model-Specific Registers
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read from or write to Model Specific Registers
// msr: msr address
// rw: 0 = read, 1 = write
// input data is ignored for reads
//

uint64_t msr_rw(uint64_t msr, uint64_t data, int rw)
{
  uint64_t high = 0, low = 0;

  if(rw == 1) // Write
  {
    low = ((uint32_t *)&data)[0];
    high = ((uint32_t *)&data)[1];
    asm volatile("wrmsr"
             : // No outputs
             : "a" (low), "c" (msr), "d" (high) // Input MSR into %rcx, and high (%rdx) & low (%rax) to write
             : // No clobbers
           );
  }
  else // Read
  {
    asm volatile("rdmsr"
             : "=a" (low), "=d" (high) // Outputs
             : "c" (msr) // Input MSR into %rcx
             : // No clobbers
           );
  }
  return (high << 32 | low); // For write, this will be data. Reads will be the msr's value.
}

//----------------------------------------------------------------------------------------------------------------------------------
// vmxcsr_rw: Read/Write MXCSR (Vex-Encoded)
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read from or write to the MXCSR register (VEX-encoded version)
// Use this one if using AVX instructions
// rw: 0 = read, 1 = write
// input data is ignored for reads
//

uint32_t vmxcsr_rw(uint32_t data, int rw)
{
  if(rw == 1) // Write
  {
    asm volatile("vldmxcsr %[src]"
             : // No outputs
             : [src] "m" (data) // Input 32-bit value into MXCSR
             : // No clobbers
           );
  }
  else // Read
  {
    asm volatile("vstmxcsr %[dest]"
             : [dest] "=m" (data) // Outputs 32-bit value from MXCSR
             :  // No inputs
             : // No clobbers
           );
  }
  return data;
}

//----------------------------------------------------------------------------------------------------------------------------------
// mxcsr_rw: Read/Write MXCSR (Legacy/SSE)
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read from or write to the MXCSR register (Legacy/SSE)
// rw: 0 = read, 1 = write
// input data is ignored for reads
//

uint32_t mxcsr_rw(uint32_t data, int rw)
{
  if(rw == 1) // Write
  {
    asm volatile("ldmxcsr %[src]"
             : // No outputs
             : [src] "m" (data) // Input 32-bit value into MXCSR
             : // No clobbers
           );
  }
  else // Read
  {
    asm volatile("stmxcsr %[dest]"
             : [dest] "=m" (data) // Outputs 32-bit value from MXCSR
             :  // No inputs
             : // No clobbers
           );
  }
  return data;
}

//----------------------------------------------------------------------------------------------------------------------------------
// control_register_rw: Read/Write Control Registers and RFLAGS
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read from or write to the standard system control registers (CR0-CR4 and CR8) and the RFLAGS register
// crX: an integer specifying which CR (e.g. 0 for CR0, etc.), use 'f' (with single quotes) for RFLAGS
// in_out: writes this value if rw = 1, input value ignored on reads
// rw: 0 = read, 1 = write
//

uint64_t control_register_rw(int crX, uint64_t in_out, int rw) // Read from or write to a control register
{
  if(rw == 1) // Write
  {
    switch(crX)
    {
      case(0):
        asm volatile("mov %[dest], %%cr0"
                     : // No outputs
                     : [dest] "r" (in_out) // Inputs
                     : // No clobbers
                   );
        break;
      case(1):
        asm volatile("mov %[dest], %%cr1"
                     : // No outputs
                     : [dest] "r" (in_out) // Inputs
                     : // No clobbers
                   );
        break;
      case(2):
        asm volatile("mov %[dest], %%cr2"
                     : // No outputs
                     : [dest] "r" (in_out) // Inputs
                     : // No clobbers
                   );
        break;
      case(3):
        asm volatile("mov %[dest], %%cr3"
                     : // No outputs
                     : [dest] "r" (in_out) // Inputs
                     : // No clobbers
                   );
        break;
      case(4):
        asm volatile("mov %[dest], %%cr4"
                     : // No outputs
                     : [dest] "r" (in_out) // Inputs
                     : // No clobbers
                   );
        break;
      case(8):
        asm volatile("mov %[dest], %%cr8"
                     : // No outputs
                     : [dest] "r" (in_out) // Inputs
                     : // No clobbers
                   );
        break;
      case('f'):
        asm volatile("pushq %[dest]\n\t"
                     "popfq"
                     : // No outputs
                     : [dest] "r" (in_out) // Inputs
                     : "cc" // Control codes get clobbered
                   );
        break;
      default:
        // Nothing
        break;
    }
  }
  else // Read
  {
    switch(crX)
    {
      case(0):
        asm volatile("mov %%cr0, %[dest]"
                     : [dest] "=r" (in_out) // Outputs
                     : // No inputs
                     : // No clobbers
                   );
        break;
      case(1):
        asm volatile("mov %%cr1, %[dest]"
                     : [dest] "=r" (in_out) // Outputs
                     : // No inputs
                     : // No clobbers
                   );
        break;
      case(2):
        asm volatile("mov %%cr2, %[dest]"
                     : [dest] "=r" (in_out) // Outputs
                     : // No inputs
                     : // No clobbers
                   );
        break;
      case(3):
        asm volatile("mov %%cr3, %[dest]"
                     : [dest] "=r" (in_out) // Outputs
                     : // No inputs
                     : // No clobbers
                   );
        break;
      case(4):
        asm volatile("mov %%cr4, %[dest]"
                     : [dest] "=r" (in_out) // Outputs
                     : // No inputs
                     : // No clobbers
                   );
        break;
      case(8):
        asm volatile("mov %%cr8, %[dest]"
                     : [dest] "=r" (in_out) // Outputs
                     : // No inputs
                     : // No clobbers
                   );
        break;
      case('f'):
        asm volatile("pushfq\n\t"
                     "popq %[dest]"
                     : [dest] "=r" (in_out) // Outputs
                     : // No inputs
                     : // No clobbers
                   );
        break;
      default:
        // Nothing
        break;
    }
  }

  return in_out;
}

//----------------------------------------------------------------------------------------------------------------------------------
// xcr_rw: Read/Write Extended Control Registers
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read from or write to the eXtended Control Registers
// XCR0 is used to enable AVX/AVX512/SSE extended registers
// xcrX: an integer for specifying which XCR (0 for XCR0, etc.)
// rw: 0 = read, 1 = write
// data is ignored for reads
//

uint64_t xcr_rw(uint64_t xcrX, uint64_t data, int rw)
{
  uint64_t high = 0, low = 0;

  if(rw == 1) // Write
  {
    low = ((uint32_t *)&data)[0];
    high = ((uint32_t *)&data)[1];
    asm volatile("xsetbv"
             : // No outputs
             : "a" (low), "c" (xcrX), "d" (high) // Input XCR# into %rcx, and high (%rdx) & low (%rax) to write
             : // No clobbers
           );
  }
  else // Read
  {
    asm volatile("xgetbv"
             : "=a" (low), "=d" (high) // Outputs
             : "c" (xcrX) // Input XCR# into %rcx
             : // No clobbers
           );
  }
  return (high << 32 | low); // For write, this will be data. Reads will be the msr's value.
}

//----------------------------------------------------------------------------------------------------------------------------------
// read_cs: Read %CS Register
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read the %CS (code segement) register
// Useful for checking if in 64-bit mode
//

uint64_t read_cs(void)
{
  uint64_t output = 0;
  asm volatile("mov %%cs, %[dest]"
                : [dest] "=r" (output)
                : // no inputs
                : // no clobbers
                );
  return output;
}

//----------------------------------------------------------------------------------------------------------------------------------
// get_gdtr: Read Global Descriptor Table Register
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read the Global Descriptor Table Register
// The %CS register contains an index for use with the address retrieved from this, in order to point to which GDT entry is relevant
// to the current code segment.
//

DT_STRUCT get_gdtr(void)
{
  DT_STRUCT gdtr_data = {0};
  asm volatile("sgdt %[dest]"
           : [dest] "=m" (gdtr_data) // Outputs
           : // No inputs
           : // No clobbers
         );

  return gdtr_data;
}

//----------------------------------------------------------------------------------------------------------------------------------
// get_idtr: Read Interrupt Descriptor Table Register
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read the Interrupt Descriptor Table Register
//

DT_STRUCT get_idtr(void)
{
  DT_STRUCT idtr_data = {0};
  asm volatile("sidt %[dest]"
           : [dest] "=m" (idtr_data) // Outputs
           : // No inputs
           : // No clobbers
         );

  return idtr_data;
}

//----------------------------------------------------------------------------------------------------------------------------------
// portio_rw: Read/Write I/O Ports
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read from or write to x86 port addresses
// port_address: address of the port
// size: 1, 2, or 4 bytes
// rw: 0 = read, 1 = write
// input data is ignored on reads
//

uint32_t portio_rw(uint16_t port_address, uint32_t data, int size, int rw)
{
  if(size == 1)
  {
    if(rw == 1) // Write
    {
      asm volatile("outb %[value], %[address]" // GAS syntax (src, dest) is opposite Intel syntax (dest, src)
                    : // No outputs
                    : [value] "a" ((uint8_t)data), [address] "d" (port_address)
                    : // No clobbers
                  );
    }
    else // Read
    {
      asm volatile("inb %[address], %[value]"
                    : // No outputs
                    : [value] "a" ((uint8_t)data), [address] "d" (port_address)
                    : // No clobbers
                  );
    }
  }
  else if(size == 2)
  {
    if(rw == 1) // Write
    {
      asm volatile("outw %[value], %[address]"
                    : // No outputs
                    : [value] "a" ((uint16_t)data), [address] "d" (port_address)
                    : // No clobbers
                  );
    }
    else // Read
    {
      asm volatile("inw %[address], %[value]"
                    : // No outputs
                    : [value] "a" ((uint16_t)data), [address] "d" (port_address)
                    : // No clobbers
                  );
    }
  }
  else if(size == 4)
  {
    if(rw == 1) // Write
    {
      asm volatile("outl %[value], %[address]"
                    : // No outputs
                    : [value] "a" (data), [address] "d" (port_address)
                    : // No clobbers
                  );
    }
    else // Read
    {
      asm volatile("inl %[address], %[value]"
                    : // No outputs
                    : [value] "a" (data), [address] "d" (port_address)
                    : // No clobbers
                  );
    }
  }
  else
  {
    printf("Invalid port i/o size.\r\n");
  }

  return data;
}

//----------------------------------------------------------------------------------------------------------------------------------
// Get_Brandstring: Read CPU Brand String
//----------------------------------------------------------------------------------------------------------------------------------
//
// Get the 48-byte system brandstring (something like "Intel(R) Core(TM) i7-7700HQ CPU @ 2.80GHz")
//
// "brandstring" must be a 48-byte array
//

char * Get_Brandstring(uint32_t * brandstring)
{
  uint64_t rax_value = 0x80000000;
  uint64_t rax = 0, rbx = 0, rcx = 0, rdx = 0;

  asm volatile("cpuid"
               : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
               : "a" (rax_value) // The value to put into %rax
               : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
            );
  if(rax >= 0x80000004)
  {
    rax_value = 0x80000002;
    asm volatile("cpuid"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                 : "a" (rax_value) // The value to put into %rax
                 : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
              );

    brandstring[0] = ((uint32_t *)&rax)[0];
    brandstring[1] = ((uint32_t *)&rbx)[0];
    brandstring[2] = ((uint32_t *)&rcx)[0];
    brandstring[3] = ((uint32_t *)&rdx)[0];

    rax_value = 0x80000003;
    asm volatile("cpuid"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                 : "a" (rax_value) // The value to put into %rax
                 : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
              );

    brandstring[4] = ((uint32_t *)&rax)[0];
    brandstring[5] = ((uint32_t *)&rbx)[0];
    brandstring[6] = ((uint32_t *)&rcx)[0];
    brandstring[7] = ((uint32_t *)&rdx)[0];

    rax_value = 0x80000004;
    asm volatile("cpuid"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                 : "a" (rax_value) // The value to put into %rax
                 : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
              );

    brandstring[8] = ((uint32_t *)&rax)[0];
    brandstring[9] = ((uint32_t *)&rbx)[0];
    brandstring[10] = ((uint32_t *)&rcx)[0];
    brandstring[11] = ((uint32_t *)&rdx)[0];

     // Brandstrings are [supposed to be] null-terminated.

    return (char*)brandstring;
  }
  else
  {
    printf("Brand string not supported\r\n");
    return NULL;
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
// Get_Manufacturer_ID: Read CPU Manufacturer ID
//----------------------------------------------------------------------------------------------------------------------------------
//
// Get CPU manufacturer identifier (something like "GenuineIntel" or "AuthenticAMD")
// Useful to verify CPU authenticity, supposedly
//
// "Manufacturer_ID" must be a 13-byte array
//

char * Get_Manufacturer_ID(char * Manufacturer_ID)
{
  uint64_t rbx = 0, rcx = 0, rdx = 0;

  asm volatile("cpuid"
               : "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
               : "a" (0x00) // The value to put into %rax
               : // CPUID would clobber any of the abcd registers not listed explicitly
             );

  Manufacturer_ID[0] = ((char *)&rbx)[0];
  Manufacturer_ID[1] = ((char *)&rbx)[1];
  Manufacturer_ID[2] = ((char *)&rbx)[2];
  Manufacturer_ID[3] = ((char *)&rbx)[3];

  Manufacturer_ID[4] = ((char *)&rdx)[0];
  Manufacturer_ID[5] = ((char *)&rdx)[1];
  Manufacturer_ID[6] = ((char *)&rdx)[2];
  Manufacturer_ID[7] = ((char *)&rdx)[3];

  Manufacturer_ID[8] = ((char *)&rcx)[0];
  Manufacturer_ID[9] = ((char *)&rcx)[1];
  Manufacturer_ID[10] = ((char *)&rcx)[2];
  Manufacturer_ID[11] = ((char *)&rcx)[3];

  Manufacturer_ID[12] = '\0';

//  printf("%c%c%c%c%c%c%c%c%c%c%c%c\r\n",
//  ((char *)&rbx)[0], ((char *)&rbx)[1], ((char *)&rbx)[2], ((char *)&rbx)[3],
//  ((char *)&rdx)[0], ((char *)&rdx)[1], ((char *)&rdx)[2], ((char *)&rdx)[3],
//  ((char *)&rcx)[0], ((char *)&rcx)[1], ((char *)&rcx)[2], ((char *)&rcx)[3]);

  return Manufacturer_ID;
}

//----------------------------------------------------------------------------------------------------------------------------------
// cpu_features: Read CPUID
//----------------------------------------------------------------------------------------------------------------------------------
//
// Query CPUID with the specified RAX and RCX (formerly EAX and ECX on 32-bit)
// Contains some feature checks already.
//
// rax_value: value to put into %rax before calling CPUID (leaf number)
// rcx_value: value to put into %rcx before calling CPUID (subleaf number, if applicable. Set it to 0 if there are no subleaves for the given rax_value)
//

void cpu_features(uint64_t rax_value, uint64_t rcx_value)
{
  // x86 does not memory-map control registers, unlike, for example, STM32 chips
  // System control registers like CR0, CR1, CR2, CR3, CR4, CR8, and EFLAGS
  // are accessed via special asm instructions.
  printf("CPUID input rax: %#qx, rcx: %#qx\r\n\n", rax_value, rcx_value);

  uint64_t rax = 0, rbx = 0, rcx = 0, rdx = 0;

  if(rax_value == 0)
  {
    asm volatile("cpuid"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                 : "a" (rax_value) // The value to put into %rax
                 : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
               );

    printf("rax: %#qx\r\n%c%c%c%c%c%c%c%c%c%c%c%c\r\n", rax,
    ((char *)&rbx)[0], ((char *)&rbx)[1], ((char *)&rbx)[2], ((char *)&rbx)[3],
    ((char *)&rdx)[0], ((char *)&rdx)[1], ((char *)&rdx)[2], ((char *)&rdx)[3],
    ((char *)&rcx)[0], ((char *)&rcx)[1], ((char *)&rcx)[2], ((char *)&rcx)[3]);
    // GenuineIntel
    // AuthenticAMD
  }
  else if(rax_value == 1)
  {
    asm volatile("cpuid"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                 : "a" (rax_value) // The value to put into %rax
                 : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
               );

    printf("rax: %#qx\r\nrbx: %#qx\r\nrcx: %#qx\r\nrdx: %#qx\r\n", rax, rbx, rcx, rdx);
    if(rcx & (1 << 31)) // 1 means hypervisor (i.e. in a VM), 0 means no hypervisor
    {
      printf("You're in a hypervisor!\r\n");
    }

    if(rcx & (1 << 12))
    {
      printf("FMA supported.\r\n");
    }
    else
    {
      printf("FMA not supported.\r\n");
    }

    if(rcx & (1 << 1))
    {
      if(rcx & (1 << 25))
      {
        printf("AESNI + PCLMULQDQ supported.\r\n");
      }
      else
      {
        printf("PCLMULQDQ supported (but not AESNI).\r\n");
      }
    }

    if(rcx & (1 << 27))
    {
      printf("AVX: OSXSAVE = 1\r\n");
    }
    else
    {
      printf("AVX: OSXSAVE = 0\r\n");
    }

    if(rcx & (1 << 26))
    {
      printf("AVX: XSAVE supported.\r\n");
    }
    else
    {
      printf("AVX: XSAVE not supported.\r\n");
    }

    if(rcx & (1 << 28))
    {
      printf("AVX supported.\r\n");
    }
    else
    {
      printf("AVX not supported. Checking for latest SSE features:\r\n");
      if(rcx & (1 << 20))
      {
        printf("Up to SSE4.2 supported.\r\n");
      }
      else
      {
        if(rcx & (1 << 19))
        {
          printf("Up to SSE4.1 supported.\r\n");
        }
        else
        {
          if(rcx & (1 << 9))
          {
            printf("Up to SSSE3 supported.\r\n");
          }
          else
          {
            if(rcx & 1)
            {
              printf("Up to SSE3 supported.\r\n");
            }
            else
            {
              if(rdx & (1 << 26))
              {
                printf("Up to SSE2 supported.\r\n");
              }
              else
              {
                printf("This is one weird CPU to get this far. x86_64 mandates SSE2.\r\n");
              }
            }
          }
        }
      }
    }

    if(rcx & (1 << 29))
    {
      printf("F16C supported.\r\n");
    }
    if(rdx & (1 << 22))
    {
      printf("ACPI via MSR supported.\r\n");
    }
    else
    {
      printf("ACPI via MSR not supported.\r\n");
    }
    if(rdx & (1 << 24))
    {
      printf("FXSR supported.\r\n");
    }
  }
  else if(rax_value == 7 && rcx_value == 0)
  {
    asm volatile("cpuid"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                 : "a" (rax_value), "c" (rcx_value) // The values to put into %rax and %rcx
                 : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
              );

    printf("rax: %#qx\r\nrbx: %#qx\r\nrcx: %#qx\r\nrdx: %#qx\r\n", rax, rbx, rcx, rdx);
    if(rbx & (1 << 5))
    {
      printf("AVX2 supported.\r\n");
    }
    else
    {
      printf("AVX2 not supported.\r\n");
    }
    // AVX512 feature check if AVX512 supported
    if(rbx & (1 << 16))
    {
      printf("AVX512F supported.\r\n");
      printf("Checking other supported AVX512 features:\r\n");
      if(rbx & (1 << 17))
      {
        printf("AVX512DQ\r\n");
      }
      if(rbx & (1 << 21))
      {
        printf("AVX512_IFMA\r\n");
      }
      if(rbx & (1 << 26))
      {
        printf("AVX512PF\r\n");
      }
      if(rbx & (1 << 27))
      {
        printf("AVX512ER\r\n");
      }
      if(rbx & (1 << 28))
      {
        printf("AVX512CD\r\n");
      }
      if(rbx & (1 << 30))
      {
        printf("AVX512BW\r\n");
      }
      if(rbx & (1 << 31))
      {
        printf("AVX512VL\r\n");
      }
      if(rcx & (1 << 1))
      {
        printf("AVX512_VBMI\r\n");
      }
      if(rcx & (1 << 6))
      {
        printf("AVX512_VBMI2\r\n");
      }
      if(rcx & (1 << 11))
      {
        printf("AVX512VNNI\r\n");
      }
      if(rcx & (1 << 12))
      {
        printf("AVX512_BITALG\r\n");
      }
      if(rcx & (1 << 14))
      {
        printf("AVX512_VPOPCNTDQ\r\n");
      }
      if(rdx & (1 << 2))
      {
        printf("AVX512_4VNNIW\r\n");
      }
      if(rdx & (1 << 3))
      {
        printf("AVX512_4FMAPS\r\n");
      }
      printf("End of AVX512 feature check.\r\n");
    }
    else
    {
      printf("AVX512 not supported.\r\n");
    }
    // End AVX512 check
    if(rcx & (1 << 8))
    {
      printf("GFNI Supported\r\n");
    }
    if(rcx & (1 << 9))
    {
      printf("VAES Supported\r\n");
    }
    if(rcx & (1 << 10))
    {
      printf("VPCLMULQDQ Supported\r\n");
    }
    if(rcx & (1 << 27))
    {
      printf("MOVDIRI Supported\r\n");
    }
    if(rcx & (1 << 28))
    {
      printf("MOVDIR64B Supported\r\n");
    }
  }
  else if(rax_value == 0x80000000)
  {
    asm volatile("cpuid"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                 : "a" (rax_value) // The value to put into %rax
                 : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
              );
    if(rax >= 0x80000004)
    {
      uint32_t brandstring[12] = {0};

      rax_value = 0x80000002;
      asm volatile("cpuid"
                   : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                   : "a" (rax_value) // The value to put into %rax
                   : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
                );

      brandstring[0] = ((uint32_t *)&rax)[0];
      brandstring[1] = ((uint32_t *)&rbx)[0];
      brandstring[2] = ((uint32_t *)&rcx)[0];
      brandstring[3] = ((uint32_t *)&rdx)[0];

      rax_value = 0x80000003;
      asm volatile("cpuid"
                   : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                   : "a" (rax_value) // The value to put into %rax
                   : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
                );

      brandstring[4] = ((uint32_t *)&rax)[0];
      brandstring[5] = ((uint32_t *)&rbx)[0];
      brandstring[6] = ((uint32_t *)&rcx)[0];
      brandstring[7] = ((uint32_t *)&rdx)[0];

      rax_value = 0x80000004;
      asm volatile("cpuid"
                   : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                   : "a" (rax_value) // The value to put into %rax
                   : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
                );

      brandstring[8] = ((uint32_t *)&rax)[0];
      brandstring[9] = ((uint32_t *)&rbx)[0];
      brandstring[10] = ((uint32_t *)&rcx)[0];
      brandstring[11] = ((uint32_t *)&rdx)[0];

      // Brandstrings are [supposed to be] null-terminated.

      printf("Brand String: %.48s\r\n", (char *)brandstring);
    }
    else
    {
      printf("Brand string not supported\r\n");
    }
  }
  else if(rax_value == 0x80000001)
  {
    asm volatile("cpuid"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                 : "a" (rax_value) // The value to put into %rax
                 : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
               );

    printf("rax: %#qx\r\nrbx: %#qx\r\nrcx: %#qx\r\nrdx: %#qx\r\n", rax, rbx, rcx, rdx);
    if(rdx & (1 << 26))
    {
        printf("1 GB pages are available.\r\n");
    }
    if(rdx & (1 << 29))
    {
        printf("Long Mode supported. (*Phew*)\r\n");
    }
  }
  else
  {
    asm volatile("cpuid"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx) // Outputs
                 : "a" (rax_value) // The value to put into %rax
                 : // CPUID would clobber any of the abcd registers not listed explicitly (all are here, though)
               );

    printf("rax: %#qx\r\nrbx: %#qx\r\nrcx: %#qx\r\nrdx: %#qx\r\n", rax, rbx, rcx, rdx);
  }
}
// END cpu_features
