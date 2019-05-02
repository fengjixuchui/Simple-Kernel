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

static void cs_update(void);
static void set_interrupt_ISR(uint64_t isr_num, uint64_t isr_addr);
static void set_trap_ISR(uint64_t isr_num, uint64_t isr_addr);
static void set_unused_ISR(uint64_t isr_num);

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
  Global_Memory_Info.MemMapDescriptorVersion = LP->Memory_Map_Descriptor_Version;
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

  // Make a replacement GDT since the UEFI one is in EFI Boot Services Memory.
  // Don't need anything fancy, just need one to exist somewhere (preferably in EfiLoaderData, which won't change with this software)
  Setup_MinimalGDT();

  // Set up IDT for interrupts
  Setup_IDT();

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
//          printf("AVX ON");
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
// read_perfs_initial: Measure CPU Performance (Part 1 of 2)
//----------------------------------------------------------------------------------------------------------------------------------
//
// Takes an array of 2x uint64_t (e.g. uint64_t perfcounters[2] = {1, 1}; ), fills first uint64 with aperf and 2nd with mperf.
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
// get_CPU_freq: Measure CPU Performance (Part 2 of 2)
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
// portio_rw: Read/Write I/O Ports
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read from or write to x86 port addresses
//
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
// msr_rw: Read/Write Model-Specific Registers
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read from or write to Model Specific Registers
//
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
// Use this function instead of mxcsr_rw() if using AVX instructions
//
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
//
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
//
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
//
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
// When used in conjunction with get_gdtr(), this is useful for checking if in 64-bit mode
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
//
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
// set_gdtr: Set Global Descriptor Table Register
//----------------------------------------------------------------------------------------------------------------------------------
//
// Set the Global Descriptor Table Register
//

void set_gdtr(DT_STRUCT gdtr_data)
{
  asm volatile("lgdt %[src]"
           : // No outputs
           : [src] "m" (gdtr_data) // Inputs
           : // No clobbers
         );
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
// set_idtr: Set Interrupt Descriptor Table Register
//----------------------------------------------------------------------------------------------------------------------------------
//
// Set the Interrupt Descriptor Table Register
//

void set_idtr(DT_STRUCT idtr_data)
{
  asm volatile("lidt %[src]"
           : // No outputs
           : [src] "m" (idtr_data) // Inputs
           : // No clobbers
         );
}

//----------------------------------------------------------------------------------------------------------------------------------
// get_ldtr: Read Local Descriptor Table Register (Segment Selector)
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read the Local Descriptor Table Register (reads segment selector only; the selector points to the LDT entry in the GDT, as this
// entry contains all the info pertaining to the rest of the LDTR)
//

uint16_t get_ldtr(void)
{
  uint16_t ldtr_data = 0;
  asm volatile("sldt %[dest]"
           : [dest] "=m" (ldtr_data) // Outputs
           : // No inputs
           : // No clobbers
         );

  return ldtr_data;
}

//----------------------------------------------------------------------------------------------------------------------------------
// set_ldtr: Set Local Descriptor Table Register (Segment Selector)
//----------------------------------------------------------------------------------------------------------------------------------
//
// Set the Local Descriptor Table Register (reads segment selector only; the selector points to the LDT entry in the GDT, as this
// entry contains all the info pertaining to the rest of the LDTR). Use this in conjunction with set_gdtr() to describe LDT(s).
//

void set_ldtr(uint16_t ldtr_data)
{
  asm volatile("lldt %[src]"
           : // No outputs
           : [src] "m" (ldtr_data) // Inputs
           : // No clobbers
         );
}

//----------------------------------------------------------------------------------------------------------------------------------
// get_tsr: Read Task State Register (Segment Selector)
//----------------------------------------------------------------------------------------------------------------------------------
//
// Read the Task Register (reads segment selector only; the selector points to the Task State Segment (TSS) entry in the GDT, as this
// entry contains all the info pertaining to the rest of the TSR)
//

uint16_t get_tsr(void)
{
  uint16_t tsr_data = 0;
  asm volatile("str %[dest]"
           : [dest] "=m" (tsr_data) // Outputs
           : // No inputs
           : // No clobbers
         );

  return tsr_data;
}

//----------------------------------------------------------------------------------------------------------------------------------
// set_tsr: Set Task State Register (Segment Selector)
//----------------------------------------------------------------------------------------------------------------------------------
//
// Set the Task Register (reads segment selector only; the selector points to the Task State Segment (TSS) entry in the GDT, as this
// entry contains all the info pertaining to the rest of the TSR). Use this in conjunction with set_gdtr() to describe TSR(s).
//

void set_tsr(uint16_t tsr_data)
{
  asm volatile("ltr %[src]"
           : // No outputs
           : [src] "m" (tsr_data) // Inputs
           : // No clobbers
         );
}

//----------------------------------------------------------------------------------------------------------------------------------
// Setup_MinimalGDT: Set Up a Minimal Global Descriptor Table
//----------------------------------------------------------------------------------------------------------------------------------
//
// Prepare a minimal GDT for the system and set the Global Descriptor Table Register. UEFI makes a descriptor table and stores it in
// EFI Boot Services memory. Making a static table embedded in the executable itself will ensure a valid GDT is always in EfiLoaderData
// and won't get reclaimed as free memory.
//
// The cs_update() function is part of this GDT set up process, and it is used to update the %CS sgement selector in addition to the
// %DS, %ES, %FS, %GS, and %SS selectors.
//

// This is the whole GDT. See the commented code in Setup_MinimalGDT() for specific details.
static uint64_t MinimalGDT[5] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff, 0x0080890000000067, 0};
static volatile TSS64_STRUCT tss64 = {0}; // This is static, so this structure can only be read by functions defined in this .c file

void Setup_MinimalGDT(void)
{
  DT_STRUCT gdt_reg_data = {0};
  uint64_t tss64_addr = (uint64_t)&tss64;

  uint16_t tss64_base1 = (uint16_t)tss64_addr;
  uint8_t  tss64_base2 = (uint8_t)(tss64_addr >> 16);
  uint8_t  tss64_base3 = (uint8_t)(tss64_addr >> 24);
  uint32_t tss64_base4 = (uint32_t)(tss64_addr >> 32);

//  printf("TSS Addr check: %#qx, %#hx, %#hhx, %#hhx, %#x\r\n", tss64_addr, tss64_base1, tss64_base2, tss64_base3, tss64_base4);

  gdt_reg_data.Limit = sizeof(MinimalGDT) - 1;
  // By having MinimalGDT global, the pointer will be in EfiLoaderData.
  gdt_reg_data.BaseAddress = (uint64_t)MinimalGDT;

/* // The below code was originally written for GDT_ENTRY_STRUCT MinimalGDT[] instead of the static array above, and is left here to explain each entry's values
  // 4 entries: Null, code, data, TSS

  // Null
  ((uint64_t*)MinimalGDT)[0] = 0;

  // x86-64 Code (64-bit code segment)
  MinimalGDT[1].SegmentLimit1 = 0xffff;
  MinimalGDT[1].BaseAddress1 = 0;
  MinimalGDT[1].BaseAddress2 = 0;
  MinimalGDT[1].Misc1 = 0x9a; // P=1, DPL=0, S=1, Exec/Read
  MinimalGDT[1].SegmentLimit2andMisc2 = 0xaf; // G=1, D=0, L=1, AVL=0
  MinimalGDT[1].BaseAddress3 = 0;
  // Note the 'L' bit is specifically for x86-64 code segments, not data segments
  // Data segments need the 32-bit treatment instead

  // x86-64 Data (64- & 32-bit data segment)
  MinimalGDT[2].SegmentLimit1 = 0xffff;
  MinimalGDT[2].BaseAddress1 = 0;
  MinimalGDT[2].BaseAddress2 = 0;
  MinimalGDT[2].Misc1 = 0x92; // P=1, DPL=0, S=1, Read/Write
  MinimalGDT[2].SegmentLimit2andMisc2 = 0xcf; // G=1, D=1, L=0, AVL=0
  MinimalGDT[2].BaseAddress3 = 0;

  // Task Segment Entry (64-bit needs one, even though task-switching isn't supported)
  MinimalGDT[3].SegmentLimit1 = 0x67; // TSS struct is 104 bytes, so limit is 103 (0x67)
  MinimalGDT[3].BaseAddress1 = tss64_base1;
  MinimalGDT[3].BaseAddress2 = tss64_base2;
  MinimalGDT[3].Misc1 = 0x89; // P=1, DPL=0, S=0, TSS Type
  MinimalGDT[3].SegmentLimit2andMisc2 = 0x80; // G=1, D=0, L=0, AVL=0
  MinimalGDT[3].BaseAddress3 = tss64_base3;
  ((uint64_t*)MinimalGDT)[4] = (uint64_t)tss64_base4; // TSS is a double-sized entry
*/
  // The only non-constant in the GDT is the base address of the TSS struct. So let's add it in.
  ( (TSS_LDT_ENTRY_STRUCT*) &((GDT_ENTRY_STRUCT*)MinimalGDT)[3] )->BaseAddress1 = tss64_base1;
  ( (TSS_LDT_ENTRY_STRUCT*) &((GDT_ENTRY_STRUCT*)MinimalGDT)[3] )->BaseAddress2 = tss64_base2;
  ( (TSS_LDT_ENTRY_STRUCT*) &((GDT_ENTRY_STRUCT*)MinimalGDT)[3] )->BaseAddress3 = tss64_base3;
  ( (TSS_LDT_ENTRY_STRUCT*) &((GDT_ENTRY_STRUCT*)MinimalGDT)[3] )->BaseAddress4 = tss64_base4; // TSS is a double-sized entry
  // Dang that looks pretty nuts.

//  printf("MinimalGDT: %#qx : %#qx, %#qx, %#qx, %#qx, %#qx ; reg_data: %#qx, Limit: %hu\r\n", (uint64_t)MinimalGDT, ((uint64_t*)MinimalGDT)[0], ((uint64_t*)MinimalGDT)[1], ((uint64_t*)MinimalGDT)[2], ((uint64_t*)MinimalGDT)[3], ((uint64_t*)MinimalGDT)[4], gdt_reg_data.BaseAddress, gdt_reg_data.Limit);
  set_gdtr(gdt_reg_data);
  set_tsr(0x18); // TSS segment is at index 3, and 0x18 >> 3 is 3. 0x18 is 24 in decimal.
  cs_update();
}

static const uint64_t ret_data = 0xE1FF; // "jmp *%rcx", flipped for little endian -- this jumps to the address stored in %rcx
// static const uint64_t ret_data = 0xC3;
// This also works (0xC3 is 'retq'), provided that there is no ASM code emitted between the end of the asm block and the end of the cs_update() function
// (true of GCC -O3, for example, but not GCC -O0). Better & safer to use the jump to an address stored in %rcx. Thankfully x86 is little endian only.

// See the bottom of this function for details about what exactly it's doing
static void cs_update(void)
{
  // Code segment is at index 1, and 0x08 >> 3 is 1. 0x08 is 8 in decimal.
  // Data segment is at index 2, and 0x10 >> 3 is 2. 0x10 is 16 in decimal.
  asm volatile("mov $16, %%ax \n\t"
               "mov %%ax, %%ds \n\t"
               "mov %%ax, %%es \n\t"
               "mov %%ax, %%fs \n\t"
               "mov %%ax, %%gs \n\t"
               "mov %%ax, %%ss \n\t"
               // Store RIP offset, pointing to right after 'lretq'
               "leaq 18(%%rip), %%rcx \n\t" // This is hardcoded to the size of the rest of this ASM block. %rip points to the next MOV instruction, +18 bytes puts it right after 'lretq'
               "movq $8, %%rdx \n\t"
               "leaq %[update_cs], %%rax \n\t"
               "pushq %%rdx \n\t"
               "pushq %%rax \n\t"
               "lretq \n\t" // NOTE: lretq and retfq are the same. lretq is supported by both GCC and Clang, while retfq works only with GCC. Either way, opcode is 0x48CB.
               // The address loaded into %rcx points here (right after 'lretq'), so execution returns to this point without breaking compiler compatibility
               : // No outputs
               : [update_cs] "m" (ret_data)// Inputs
               : // No clobbers
              );

  //
  // NOTE: Yes, this function is more than a little weird.
  //
  // cs_update() will have a 'ret'/'retq' (and maybe some other things) after the asm 'retfq'/'lretq'. It's
  // fine, though, because "ret_data" contains a hardcoded jmp to get back to it. Why not just push an asm
  // label located right after 'lretq' that's been preloaded into %rax (so that the label address gets loaded
  // into the instruction pointer on 'lretq')? Well, it turns out that will load an address relative to the
  // kernel file image base in such a way that the address won't get relocated by the boot loader. Mysterious
  // crashes ensue. This solves that.
  //
  // That stated, I think the insanity here speaks for itself, especially since all this is *necessary* to
  // modify the %cs register in 64-bit mode using 'retfq'. Could far jumps and far calls be used? Yes, but not
  // very easily in inline ASM (far jumps in 64-bit mode are very limited: only memory-indirect absolute far
  // jumps can change %cs). Also, using the 'lretq' trick maintains quadword alignment on the stack, which is
  // nice. So really we just have to deal with making farquad returns behave in a very controlled manner...
  // which includes accepting any resulting movie references from 2001. But hey, it works, and it shouldn't
  // cause issues with kernels loaded above 4GB RAM. It works great between Clang and GCC, too.
  //
  // The only issue with this method really is it'll screw up CPU return prediction for a tiny bit (for a
  // small number of subsequent function calls, then it fixes itself). Only need this nonsense once, though!
  //
}

//----------------------------------------------------------------------------------------------------------------------------------
// Setup_IDT: Set Up Interrupt Descriptor Table
//----------------------------------------------------------------------------------------------------------------------------------
//
// UEFI makes its own IDT in Boot Services Memory, but it's not super useful outside of what it needed interrupts for (and boot
// services memory is supposed to be freeable memory after ExitBootServices() is called). This function sets up an interrupt table for
// more intricate use, as a properly set up IDT is necessary in order to use interrupts. x86(-64) is a highly interrrupt-driven
// architecture, so this is a pretty important step.
//

static IDT_GATE_STRUCT IDT_data[256] = {0}; // Reserve static memory for the IDT

void Setup_IDT(void)
{
  DT_STRUCT idt_reg_data = {0};
  idt_reg_data.Limit = sizeof(IDT_data) - 1; // Max is 16 bytes * 256 entries = 4096, - 1 = 4095 or 0xfff
  idt_reg_data.BaseAddress = (uint64_t)IDT_data;


  printf("isr_pusher0: %#qx\r\n", (uint64_t)isr_pusher0);
  printf("avx_isr_pusher0: %#qx\r\n", (uint64_t)avx_isr_pusher0);
  // Set up ISRs per ISR.S layout
  set_interrupt_ISR(0, (uint64_t)isr_pusher0); // Divide by zero
  // TODO
  // Also make a test interrupt using 32, don't forget to change unused_isr_min

  uint64_t unused_isr_min = 32; // IDT entries from this number to 255 will be set as non-present.

  // Set P = 0 for unused entries; need 256 entries. CPU will triple-fault otherwise on receipt of an unused ISR.
  for(uint64_t i = unused_isr_min; i < 256; i++)
  {
    set_unused_ISR(i);
  }

  set_idtr(idt_reg_data);
}

// Set up corresponding ISR function's IDT entry
// This is for architecturally-defined ISRs (IDT entries 0-31) and user-defined entries (32-255)
static void set_interrupt_ISR(uint64_t isr_num, uint64_t isr_addr)
{
  uint16_t isr_base1 = (uint16_t)isr_addr;
  uint16_t isr_base2 = (uint16_t)(isr_addr >> 16);
  uint32_t isr_base3 = (uint32_t)(isr_addr >> 32);

  IDT_data[isr_num].Offset1 = isr_base1; // CS base is 0, so offset becomes the base address of ISR (interrupt service routine)
  IDT_data[isr_num].SegmentSelector = 0x08; // 64-bit code segment in GDT
  IDT_data[isr_num].ISTandZero = 0; // IST of 0 uses "modified legacy stack switch mechanism" (Intel Architecture Manual Vol. 3A, Section 6.14.4 Stack Switching in IA-32e Mode)
  // IST = 0 only matters for inter-privilege level changes that arise from interrrupts--keeping the same level doesn't switch stacks
  // IST = 1-7 would apply regardless of privilege level.
  IDT_data[isr_num].Misc = 0x8E; // 0x8E for interrupt (clears IF in RFLAGS), 0x8F for trap (does not clear IF in RFLAGS), P=1, DPL=0, S=0
  IDT_data[isr_num].Offset2 = isr_base2;
  IDT_data[isr_num].Offset3 = isr_base3;
  IDT_data[isr_num].Reserved = 0;
}

// This is to set up a trap gate in the IDT for a given ISR
static void set_trap_ISR(uint64_t isr_num, uint64_t isr_addr)
{
  uint16_t isr_base1 = (uint16_t)isr_addr;
  uint16_t isr_base2 = (uint16_t)(isr_addr >> 16);
  uint32_t isr_base3 = (uint32_t)(isr_addr >> 32);

  IDT_data[isr_num].Offset1 = isr_base1; // CS base is 0, so offset becomes the base address of ISR (interrupt service routine)
  IDT_data[isr_num].SegmentSelector = 0x08; // 64-bit code segment in GDT
  IDT_data[isr_num].ISTandZero = 0; // IST of 0 uses "modified legacy stack switch mechanism" (Intel Architecture Manual Vol. 3A, Section 6.14.4 Stack Switching in IA-32e Mode)
  // IST = 0 only matters for inter-privilege level changes that arise from interrrupts--keeping the same level doesn't switch stacks
  // IST = 1-7 would apply regardless of privilege level.
  IDT_data[isr_num].Misc = 0x8F; // 0x8E for interrupt (clears IF in RFLAGS), 0x8F for trap (does not clear IF in RFLAGS), P=1, DPL=0, S=0
  IDT_data[isr_num].Offset2 = isr_base2;
  IDT_data[isr_num].Offset3 = isr_base3;
  IDT_data[isr_num].Reserved = 0;
}

// This is for unused ISRs. They need to be populated otherwise the CPU will triple fault.
static void set_unused_ISR(uint64_t isr_num)
{
  IDT_data[isr_num].Offset1 = 0;
  IDT_data[isr_num].SegmentSelector = 0x08;
  IDT_data[isr_num].ISTandZero = 0;
  IDT_data[isr_num].Misc = 0x0E; // P=0, DPL=0, S=0, placeholder interrupt type
  IDT_data[isr_num].Offset2 = 0;
  IDT_data[isr_num].Offset3 = 0;
  IDT_data[isr_num].Reserved = 0;
}

//----------------------------------------------------------------------------------------------------------------------------------
// Setup_Paging: Set Up Paging Structures
//----------------------------------------------------------------------------------------------------------------------------------
//
// UEFI sets up paging structures in EFI Boot Services Memory. Since that memory is to be reclaimed as free memory, there needs to be
// valid paging structures elsewhere doing the job. This function takes care of that and sets up paging structures, ensuring that UEFI
// data is not relied on.
//

void Setup_Paging(void)
{
  //TODO, can reclaim bootsrvdata after [_] this, [V] GDT, and [_] IDT
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

//----------------------------------------------------------------------------------------------------------------------------------
// Interrupt Handlers: Handlers for System Interrupts and Exceptions
//----------------------------------------------------------------------------------------------------------------------------------
//
// Various interrupts and exception handlers are defined here
//

// TODO
void AVX_ISR_handler(AVX_INTERRUPT_FRAME * i_frame)
{
  printf("AVX Interrupt! Entry: %#qu\r\n", i_frame->isr_num);
  printf("rax: %#qx, rbx: %#qx, rcx: %#qx, rdx: %#qx, rsi: %#qx, rdi: %#qx\r\n", i_frame->rax, i_frame->rbx, i_frame->rcx, i_frame->rdx, i_frame->rsi, i_frame->rdi);
  printf("r8: %#qx, r9: %#qx, r10: %#qx, r11: %#qx, r12: %#qx, r13: %#qx\r\n", i_frame->r8, i_frame->r9, i_frame->r10, i_frame->r11, i_frame->r12, i_frame->r13);
  printf("r14: %#qx, r15: %#qx, rbp: %#qx, rip: %#qx, cs: %#qx, rflags: %#qx\r\n", i_frame->r14, i_frame->r15, i_frame->rbp, i_frame->rip, i_frame->cs, i_frame->rflags);
  printf("rsp: %#qx, ss: %#qx\r\n", i_frame->rsp, i_frame->ss);
#ifdef __AVX512F__ // Hope screen resolution is high enough to see all this... Would need font scale 1, 8x8 font on 1280px-wide screen. 1024x768 can't really fit it horizontally: need at least 1032 px wide at font scale 1, 8x8 font.
  printf("ZMM0: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm0[0], i_frame->zmm0[1], i_frame->zmm0[2], i_frame->zmm0[3], i_frame->zmm0[4], i_frame->zmm0[5], i_frame->zmm0[6], i_frame->zmm0[7]);
  printf("ZMM1: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm1[0], i_frame->zmm1[1], i_frame->zmm1[2], i_frame->zmm1[3], i_frame->zmm1[4], i_frame->zmm1[5], i_frame->zmm1[6], i_frame->zmm1[7]);
  printf("ZMM2: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm2[0], i_frame->zmm2[1], i_frame->zmm2[2], i_frame->zmm2[3], i_frame->zmm2[4], i_frame->zmm2[5], i_frame->zmm2[6], i_frame->zmm2[7]);
  printf("ZMM3: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm3[0], i_frame->zmm3[1], i_frame->zmm3[2], i_frame->zmm3[3], i_frame->zmm3[4], i_frame->zmm3[5], i_frame->zmm3[6], i_frame->zmm3[7]);
  printf("ZMM4: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm4[0], i_frame->zmm4[1], i_frame->zmm4[2], i_frame->zmm4[3], i_frame->zmm4[4], i_frame->zmm4[5], i_frame->zmm4[6], i_frame->zmm4[7]);
  printf("ZMM5: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm5[0], i_frame->zmm5[1], i_frame->zmm5[2], i_frame->zmm5[3], i_frame->zmm5[4], i_frame->zmm5[5], i_frame->zmm5[6], i_frame->zmm5[7]);
  printf("ZMM6: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm6[0], i_frame->zmm6[1], i_frame->zmm6[2], i_frame->zmm6[3], i_frame->zmm6[4], i_frame->zmm6[5], i_frame->zmm6[6], i_frame->zmm6[7]);
  printf("ZMM7: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm7[0], i_frame->zmm7[1], i_frame->zmm7[2], i_frame->zmm7[3], i_frame->zmm7[4], i_frame->zmm7[5], i_frame->zmm7[6], i_frame->zmm7[7]);
  printf("ZMM8: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm8[0], i_frame->zmm8[1], i_frame->zmm8[2], i_frame->zmm8[3], i_frame->zmm8[4], i_frame->zmm8[5], i_frame->zmm8[6], i_frame->zmm8[7]);
  printf("ZMM9: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm9[0], i_frame->zmm9[1], i_frame->zmm9[2], i_frame->zmm9[3], i_frame->zmm9[4], i_frame->zmm9[5], i_frame->zmm9[6], i_frame->zmm9[7]);
  printf("ZMM10: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm10[0], i_frame->zmm10[1], i_frame->zmm10[2], i_frame->zmm10[3], i_frame->zmm10[4], i_frame->zmm10[5], i_frame->zmm10[6], i_frame->zmm10[7]);
  printf("ZMM11: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm11[0], i_frame->zmm11[1], i_frame->zmm11[2], i_frame->zmm11[3], i_frame->zmm11[4], i_frame->zmm11[5], i_frame->zmm11[6], i_frame->zmm11[7]);
  printf("ZMM12: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm12[0], i_frame->zmm12[1], i_frame->zmm12[2], i_frame->zmm12[3], i_frame->zmm12[4], i_frame->zmm12[5], i_frame->zmm12[6], i_frame->zmm12[7]);
  printf("ZMM13: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm13[0], i_frame->zmm13[1], i_frame->zmm13[2], i_frame->zmm13[3], i_frame->zmm13[4], i_frame->zmm13[5], i_frame->zmm13[6], i_frame->zmm13[7]);
  printf("ZMM14: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm14[0], i_frame->zmm14[1], i_frame->zmm14[2], i_frame->zmm14[3], i_frame->zmm14[4], i_frame->zmm14[5], i_frame->zmm14[6], i_frame->zmm14[7]);
  printf("ZMM15: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm15[0], i_frame->zmm15[1], i_frame->zmm15[2], i_frame->zmm15[3], i_frame->zmm15[4], i_frame->zmm15[5], i_frame->zmm15[6], i_frame->zmm15[7]);
  printf("ZMM16: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm16[0], i_frame->zmm16[1], i_frame->zmm16[2], i_frame->zmm16[3], i_frame->zmm16[4], i_frame->zmm16[5], i_frame->zmm16[6], i_frame->zmm16[7]);
  printf("ZMM17: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm17[0], i_frame->zmm17[1], i_frame->zmm17[2], i_frame->zmm17[3], i_frame->zmm17[4], i_frame->zmm17[5], i_frame->zmm17[6], i_frame->zmm17[7]);
  printf("ZMM18: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm18[0], i_frame->zmm18[1], i_frame->zmm18[2], i_frame->zmm18[3], i_frame->zmm18[4], i_frame->zmm18[5], i_frame->zmm18[6], i_frame->zmm18[7]);
  printf("ZMM19: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm19[0], i_frame->zmm19[1], i_frame->zmm19[2], i_frame->zmm19[3], i_frame->zmm19[4], i_frame->zmm19[5], i_frame->zmm19[6], i_frame->zmm19[7]);
  printf("ZMM20: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm20[0], i_frame->zmm20[1], i_frame->zmm20[2], i_frame->zmm20[3], i_frame->zmm20[4], i_frame->zmm20[5], i_frame->zmm20[6], i_frame->zmm20[7]);
  printf("ZMM21: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm21[0], i_frame->zmm21[1], i_frame->zmm21[2], i_frame->zmm21[3], i_frame->zmm21[4], i_frame->zmm21[5], i_frame->zmm21[6], i_frame->zmm21[7]);
  printf("ZMM22: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm22[0], i_frame->zmm22[1], i_frame->zmm22[2], i_frame->zmm22[3], i_frame->zmm22[4], i_frame->zmm22[5], i_frame->zmm22[6], i_frame->zmm22[7]);
  printf("ZMM23: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm23[0], i_frame->zmm23[1], i_frame->zmm23[2], i_frame->zmm23[3], i_frame->zmm23[4], i_frame->zmm23[5], i_frame->zmm23[6], i_frame->zmm23[7]);
  printf("ZMM24: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm24[0], i_frame->zmm24[1], i_frame->zmm24[2], i_frame->zmm24[3], i_frame->zmm24[4], i_frame->zmm24[5], i_frame->zmm24[6], i_frame->zmm24[7]);
  printf("ZMM25: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm25[0], i_frame->zmm25[1], i_frame->zmm25[2], i_frame->zmm25[3], i_frame->zmm25[4], i_frame->zmm25[5], i_frame->zmm25[6], i_frame->zmm25[7]);
  printf("ZMM26: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm26[0], i_frame->zmm26[1], i_frame->zmm26[2], i_frame->zmm26[3], i_frame->zmm26[4], i_frame->zmm26[5], i_frame->zmm26[6], i_frame->zmm26[7]);
  printf("ZMM27: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm27[0], i_frame->zmm27[1], i_frame->zmm27[2], i_frame->zmm27[3], i_frame->zmm27[4], i_frame->zmm27[5], i_frame->zmm27[6], i_frame->zmm27[7]);
  printf("ZMM28: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm28[0], i_frame->zmm28[1], i_frame->zmm28[2], i_frame->zmm28[3], i_frame->zmm28[4], i_frame->zmm28[5], i_frame->zmm28[6], i_frame->zmm28[7]);
  printf("ZMM29: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm29[0], i_frame->zmm29[1], i_frame->zmm29[2], i_frame->zmm29[3], i_frame->zmm29[4], i_frame->zmm29[5], i_frame->zmm29[6], i_frame->zmm29[7]);
  printf("ZMM30: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm30[0], i_frame->zmm30[1], i_frame->zmm30[2], i_frame->zmm30[3], i_frame->zmm30[4], i_frame->zmm30[5], i_frame->zmm30[6], i_frame->zmm30[7]);
  printf("ZMM31: %#016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", i_frame->zmm31[0], i_frame->zmm31[1], i_frame->zmm31[2], i_frame->zmm31[3], i_frame->zmm31[4], i_frame->zmm31[5], i_frame->zmm31[6], i_frame->zmm31[7]);
  #ifdef __AVX512BW__
    printf("k0: %#qx, k1: %#qx, k2: %#qx, k3: %#qx\r\n", i_frame->k0, i_frame->k1, i_frame->k2, i_frame->k3);
    printf("k4: %#qx, k5: %#qx, k6: %#qx, k7: %#qx\r\n", i_frame->k4, i_frame->k5, i_frame->k6, i_frame->k7);
  #else
    printf("k0: %#hx, k1: %#hx, k2: %#hx, k3: %#hx\r\n", i_frame->k0, i_frame->k1, i_frame->k2, i_frame->k3);
    printf("k4: %#hx, k5: %#hx, k6: %#hx, k7: %#hx\r\n", i_frame->k4, i_frame->k5, i_frame->k6, i_frame->k7);
  #endif
#elif __AVX__
  printf("YMM0: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm0[0], i_frame->ymm0[1], i_frame->ymm0[2], i_frame->ymm0[3]);
  printf("YMM1: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm1[0], i_frame->ymm1[1], i_frame->ymm1[2], i_frame->ymm1[3]);
  printf("YMM2: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm2[0], i_frame->ymm2[1], i_frame->ymm2[2], i_frame->ymm2[3]);
  printf("YMM3: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm3[0], i_frame->ymm3[1], i_frame->ymm3[2], i_frame->ymm3[3]);
  printf("YMM4: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm4[0], i_frame->ymm4[1], i_frame->ymm4[2], i_frame->ymm4[3]);
  printf("YMM5: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm5[0], i_frame->ymm5[1], i_frame->ymm5[2], i_frame->ymm5[3]);
  printf("YMM6: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm6[0], i_frame->ymm6[1], i_frame->ymm6[2], i_frame->ymm6[3]);
  printf("YMM7: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm7[0], i_frame->ymm7[1], i_frame->ymm7[2], i_frame->ymm7[3]);
  printf("YMM8: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm8[0], i_frame->ymm8[1], i_frame->ymm8[2], i_frame->ymm8[3]);
  printf("YMM9: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm9[0], i_frame->ymm9[1], i_frame->ymm9[2], i_frame->ymm9[3]);
  printf("YMM10: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm10[0], i_frame->ymm10[1], i_frame->ymm10[2], i_frame->ymm10[3]);
  printf("YMM11: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm11[0], i_frame->ymm11[1], i_frame->ymm11[2], i_frame->ymm11[3]);
  printf("YMM12: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm12[0], i_frame->ymm12[1], i_frame->ymm12[2], i_frame->ymm12[3]);
  printf("YMM13: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm13[0], i_frame->ymm13[1], i_frame->ymm13[2], i_frame->ymm13[3]);
  printf("YMM14: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm14[0], i_frame->ymm14[1], i_frame->ymm14[2], i_frame->ymm14[3]);
  printf("YMM15: %#016qx%016qx%016qx%016qx\r\n", i_frame->ymm15[0], i_frame->ymm15[1], i_frame->ymm15[2], i_frame->ymm15[3]);
#else // SSE
  printf("XMM0: %#016qx%016qx\r\n", i_frame->xmm0[0], i_frame->xmm0[1]);
  printf("XMM1: %#016qx%016qx\r\n", i_frame->xmm1[0], i_frame->xmm1[1]);
  printf("XMM2: %#016qx%016qx\r\n", i_frame->xmm2[0], i_frame->xmm2[1]);
  printf("XMM3: %#016qx%016qx\r\n", i_frame->xmm3[0], i_frame->xmm3[1]);
  printf("XMM4: %#016qx%016qx\r\n", i_frame->xmm4[0], i_frame->xmm4[1]);
  printf("XMM5: %#016qx%016qx\r\n", i_frame->xmm5[0], i_frame->xmm5[1]);
  printf("XMM6: %#016qx%016qx\r\n", i_frame->xmm6[0], i_frame->xmm6[1]);
  printf("XMM7: %#016qx%016qx\r\n", i_frame->xmm7[0], i_frame->xmm7[1]);
  printf("XMM8: %#016qx%016qx\r\n", i_frame->xmm8[0], i_frame->xmm8[1]);
  printf("XMM9: %#016qx%016qx\r\n", i_frame->xmm9[0], i_frame->xmm9[1]);
  printf("XMM10: %#016qx%016qx\r\n", i_frame->xmm10[0], i_frame->xmm10[1]);
  printf("XMM11: %#016qx%016qx\r\n", i_frame->xmm11[0], i_frame->xmm11[1]);
  printf("XMM12: %#016qx%016qx\r\n", i_frame->xmm12[0], i_frame->xmm12[1]);
  printf("XMM13: %#016qx%016qx\r\n", i_frame->xmm13[0], i_frame->xmm13[1]);
  printf("XMM14: %#016qx%016qx\r\n", i_frame->xmm14[0], i_frame->xmm14[1]);
  printf("XMM15: %#016qx%016qx\r\n", i_frame->xmm15[0], i_frame->xmm15[1]);
#endif

  asm volatile ("hlt");
}

void ISR_handler(INTERRUPT_FRAME * i_frame)
{
  switch(i_frame->isr_num)
  {
    case 0:
      printf("Fault #DE: Divide by Zero! IDT Entry: %#qu\r\n", i_frame->isr_num);
      printf("rax: %#qx, rbx: %#qx, rcx: %#qx, rdx: %#qx, rsi: %#qx, rdi: %#qx\r\n", i_frame->rax, i_frame->rbx, i_frame->rcx, i_frame->rdx, i_frame->rsi, i_frame->rdi);
      printf("r8: %#qx, r9: %#qx, r10: %#qx, r11: %#qx, r12: %#qx, r13: %#qx\r\n", i_frame->r8, i_frame->r9, i_frame->r10, i_frame->r11, i_frame->r12, i_frame->r13);
      printf("r14: %#qx, r15: %#qx, rbp: %#qx, rip: %#qx, cs: %#qx, rflags: %#qx\r\n", i_frame->r14, i_frame->r15, i_frame->rbp, i_frame->rip, i_frame->cs, i_frame->rflags);
      printf("rsp: %#qx, ss: %#qx\r\n", i_frame->rsp, i_frame->ss);
      asm volatile("hlt");
      break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 9:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:

    // 15, 21-31 are reserved
    //TODO rest of cases and exc handler (for cases 8, 10-14)

    default:
      printf("Unhandled interrupt entry: %#qx\r\n", i_frame->isr_num);
      printf("rax: %#qx, rbx: %#qx, rcx: %#qx, rdx: %#qx, rsi: %#qx, rdi: %#qx\r\n", i_frame->rax, i_frame->rbx, i_frame->rcx, i_frame->rdx, i_frame->rsi, i_frame->rdi);
      printf("r8: %#qx, r9: %#qx, r10: %#qx, r11: %#qx, r12: %#qx, r13: %#qx\r\n", i_frame->r8, i_frame->r9, i_frame->r10, i_frame->r11, i_frame->r12, i_frame->r13);
      printf("r14: %#qx, r15: %#qx, rbp: %#qx, rip: %#qx, cs: %#qx, rflags: %#qx\r\n", i_frame->r14, i_frame->r15, i_frame->rbp, i_frame->rip, i_frame->cs, i_frame->rflags);
      printf("rsp: %#qx, ss: %#qx\r\n", i_frame->rsp, i_frame->ss);
      asm volatile("hlt");
      break;
  }
}

void AVX_EXC_handler(AVX_EXCEPTION_FRAME * e_frame)
{
  printf("AVX Exception!\r\n");
}

void EXC_handler(EXCEPTION_FRAME * e_frame)
{
  printf("Exception!\r\n");
  switch(e_frame->isr_num)
  {
    case 8:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    default:
      break;
  }
}
