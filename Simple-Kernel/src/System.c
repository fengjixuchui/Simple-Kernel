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
static void set_interrupt_entry(uint64_t isr_num, uint64_t isr_addr);
static void set_trap_entry(uint64_t isr_num, uint64_t isr_addr);
static void set_unused_entry(uint64_t isr_num);
static void set_NMI_interrupt_entry(uint64_t isr_num, uint64_t isr_addr);
static void set_DF_interrupt_entry(uint64_t isr_num, uint64_t isr_addr);
static void set_MC_interrupt_entry(uint64_t isr_num, uint64_t isr_addr);
static void set_BP_interrupt_entry(uint64_t isr_num, uint64_t isr_addr);

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
  // Same with CR4.OSXMMEXCPT for SIMD errors
  uint64_t cr4 = control_register_rw(4, 0, 0);
  if(!(cr4 & (1 << 10)))
  {
    uint64_t cr4_2 = cr4 ^ (1 << 10);
    control_register_rw(4, cr4_2, 1);
    cr4_2 = control_register_rw(4, 0, 0);
    if(cr4_2 == cr4)
    {
      printf("Error setting CR4.OSXMMEXCEPT bit.\r\n");
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

// DEBUG:
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
//DEBUG: // printf("rax: %qu, rbx: %qu, rcx: %qu\r\n b/a *c: %qu\r\n", rax, rbx, rcx, (rcx * rbx)/rax);

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


//DEBUG: // printf("maxleafmask: %qu\r\n", maxleafmask);
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
__attribute__((aligned(64))) static uint64_t MinimalGDT[5] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff, 0x0080890000000067, 0};
__attribute__((aligned(64))) static TSS64_STRUCT tss64 = {0}; // This is static, so this structure can only be read by functions defined in this .c file

void Setup_MinimalGDT(void)
{
  DT_STRUCT gdt_reg_data = {0};
  uint64_t tss64_addr = (uint64_t)&tss64;

  uint16_t tss64_base1 = (uint16_t)tss64_addr;
  uint8_t  tss64_base2 = (uint8_t)(tss64_addr >> 16);
  uint8_t  tss64_base3 = (uint8_t)(tss64_addr >> 24);
  uint32_t tss64_base4 = (uint32_t)(tss64_addr >> 32);

//DEBUG: // printf("TSS Addr check: %#qx, %#hx, %#hhx, %#hhx, %#x\r\n", tss64_addr, tss64_base1, tss64_base2, tss64_base3, tss64_base4);

  gdt_reg_data.Limit = sizeof(MinimalGDT) - 1;
  // By having MinimalGDT global, the pointer will be in EfiLoaderData.
  gdt_reg_data.BaseAddress = (uint64_t)MinimalGDT;

/* // The below code is left here to explain each entry's values in the above static global MinimalGDT
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

//DEBUG: // printf("MinimalGDT: %#qx : %#qx, %#qx, %#qx, %#qx, %#qx ; reg_data: %#qx, Limit: %hu\r\n", (uint64_t)MinimalGDT, ((uint64_t*)MinimalGDT)[0], ((uint64_t*)MinimalGDT)[1], ((uint64_t*)MinimalGDT)[2], ((uint64_t*)MinimalGDT)[3], ((uint64_t*)MinimalGDT)[4], gdt_reg_data.BaseAddress, gdt_reg_data.Limit);
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

__attribute__((aligned(64))) static IDT_GATE_STRUCT IDT_data[256] = {0}; // Reserve static memory for the IDT

// Special stacks. (1 << 12) is 4 kiB
#define NMI_STACK_SIZE (1 << 12)
#define DF_STACK_SIZE (1 << 12)
#define MC_STACK_SIZE (1 << 12)
#define BP_STACK_SIZE (1 << 12)

// Ensuring 64-byte alignment for good measure
__attribute__((aligned(64))) static volatile unsigned char NMI_stack[NMI_STACK_SIZE] = {0};
__attribute__((aligned(64))) static volatile unsigned char DF_stack[DF_STACK_SIZE] = {0};
__attribute__((aligned(64))) static volatile unsigned char MC_stack[MC_STACK_SIZE] = {0};
__attribute__((aligned(64))) static volatile unsigned char BP_stack[BP_STACK_SIZE] = {0};

// TODO: IRQs from hardware (keyboard interrupts, e.g.) might need their own stack, too.

void Setup_IDT(void)
{
  DT_STRUCT idt_reg_data = {0};
  idt_reg_data.Limit = sizeof(IDT_data) - 1; // Max is 16 bytes * 256 entries = 4096, - 1 = 4095 or 0xfff
  idt_reg_data.BaseAddress = (uint64_t)IDT_data;

  // Set up TSS for special IST switches
  // Note: tss64 was defined in above Setup_MinimalGDT section.
  //
  // This is actually really important to do and not super clear in documentation:
  // Without a separate known good stack, you'll find that calling int $0x08 will trigger a general protection exception--or a divide
  // by zero error with no hander will triple fault. The IST mechanism ensures this does not happen. If calling a handler with int $(num)
  // raises a general protection fault (and it's not the GPF exception 13), it might need its own stack. This is assuming that the IDT
  // (and everything else) has been set up correctly. At the very least, it's a good idea to have separate stacks for NMI, Double Fault
  // (#DF), Machine Check (#MC), and Debug (#BP) and thus each should have a corresponding IST entry. I found that having these 4, and
  // aligning the main kernel stack to 64-bytes (in addition to aligning the 4 other stacks as per their instantiation above), solved a
  // lot of head-scratching problems.
  //
  // There is some good documentation on 64-bit stack switching in the Linux kernel docs:
  // https://www.kernel.org/doc/Documentation/x86/kernel-stacks

  tss64.IST_1_low = (uint32_t) ((uint64_t)NMI_stack);
  tss64.IST_1_high = (uint32_t) ( ((uint64_t)NMI_stack) >> 32 );

  tss64.IST_2_low = (uint32_t) ((uint64_t)DF_stack);
  tss64.IST_2_high = (uint32_t) ( ((uint64_t)DF_stack) >> 32 );

  tss64.IST_3_low = (uint32_t) ((uint64_t)MC_stack);
  tss64.IST_3_high = (uint32_t) ( ((uint64_t)MC_stack) >> 32 );

  tss64.IST_4_low = (uint32_t) ((uint64_t)BP_stack);
  tss64.IST_4_high = (uint32_t) ( ((uint64_t)BP_stack) >> 32 );

  // Set up ISRs per ISR.S layout

  //
  // Predefined System Interrupts and Exceptions
  //

  set_interrupt_entry(0, (uint64_t)avx_isr_pusher0); // Fault #DE: Divide Error (divide by 0 or not enough bits in destination)
  set_interrupt_entry(1, (uint64_t)avx_isr_pusher1); // Fault/Trap #DB: Debug Exception
  set_NMI_interrupt_entry(2, (uint64_t)avx_isr_pusher2); // NMI (Nonmaskable External Interrupt)
  // Fun fact: Hyper-V will send a watchdog timeout via an NMI if the system is halted for a while. Looks like it's supposed to crash the VM via
  // triple fault if there's no handler set up. Hpyer-V-Worker logs that the VM "has encountered a watchdog timeout and was reset" in the Windows
  // event viewer when the VM receives the NMI. Neat.
  set_BP_interrupt_entry(3, (uint64_t)avx_isr_pusher3); // Trap #BP: Breakpoint (INT3 instruction)
  set_interrupt_entry(4, (uint64_t)avx_isr_pusher4); // Trap #OF: Overflow (INTO instruction)
  set_interrupt_entry(5, (uint64_t)avx_isr_pusher5); // Fault #BR: BOUND Range Exceeded (BOUND instruction)
  set_interrupt_entry(6, (uint64_t)avx_isr_pusher6); // Fault #UD: Invalid or Undefined Opcode
  set_interrupt_entry(7, (uint64_t)avx_isr_pusher7); // Fault #NM: Device Not Available Exception

  set_DF_interrupt_entry(8, (uint64_t)avx_exc_pusher8); // Abort #DF: Double Fault (error code is always 0)

  set_interrupt_entry(9, (uint64_t)avx_isr_pusher9); // Fault (i386): Coprocessor Segment Overrun (long since obsolete, included for completeness)

  set_interrupt_entry(10, (uint64_t)avx_exc_pusher10); // Fault #TS: Invalid TSS

  set_interrupt_entry(11, (uint64_t)avx_exc_pusher11); // Fault #NP: Segment Not Present
  set_interrupt_entry(12, (uint64_t)avx_exc_pusher12); // Fault #SS: Stack Segment Fault
  set_interrupt_entry(13, (uint64_t)avx_exc_pusher13); // Fault #GP: General Protection
  set_interrupt_entry(14, (uint64_t)avx_exc_pusher14); // Fault #PF: Page Fault

  set_interrupt_entry(16, (uint64_t)avx_isr_pusher16); // Fault #MF: Math Error (x87 FPU Floating-Point Math Error)

  set_interrupt_entry(17, (uint64_t)avx_exc_pusher17); // Fault #AC: Alignment Check (error code is always 0)

  set_MC_interrupt_entry(18, (uint64_t)avx_isr_pusher18); // Abort #MC: Machine Check
  set_interrupt_entry(19, (uint64_t)avx_isr_pusher19); // Fault #XM: SIMD Floating-Point Exception (SSE instructions)
  set_interrupt_entry(20, (uint64_t)avx_isr_pusher20); // Fault #VE: Virtualization Exception

  set_interrupt_entry(30, (uint64_t)avx_exc_pusher30); // Fault #SX: Security Exception

  //
  // These are system reserved, if they trigger they will go to unhandled interrupt error
  //

  set_interrupt_entry(15, (uint64_t)avx_isr_pusher15);

  set_interrupt_entry(21, (uint64_t)avx_isr_pusher21);
  set_interrupt_entry(22, (uint64_t)avx_isr_pusher22);
  set_interrupt_entry(23, (uint64_t)avx_isr_pusher23);
  set_interrupt_entry(24, (uint64_t)avx_isr_pusher24);
  set_interrupt_entry(25, (uint64_t)avx_isr_pusher25);
  set_interrupt_entry(26, (uint64_t)avx_isr_pusher26);
  set_interrupt_entry(27, (uint64_t)avx_isr_pusher27);
  set_interrupt_entry(28, (uint64_t)avx_isr_pusher28);
  set_interrupt_entry(29, (uint64_t)avx_isr_pusher29);

  set_interrupt_entry(31, (uint64_t)avx_isr_pusher31);

  //
  // User-Defined Interrupts
  //

  // Use non-AVX ISR/EXC_MACRO if the interrupts are guaranteed not to touch AVX registers,
  // otherwise, or if in any doubt, use AVX_ISR/EXC_MACRO. By default everything is set to AVX_ISR_MACRO.

  set_interrupt_entry(32, (uint64_t)avx_isr_pusher32);
  set_interrupt_entry(33, (uint64_t)avx_isr_pusher33);
  set_interrupt_entry(34, (uint64_t)avx_isr_pusher34);
  set_interrupt_entry(35, (uint64_t)avx_isr_pusher35);
  set_interrupt_entry(36, (uint64_t)avx_isr_pusher36);
  set_interrupt_entry(37, (uint64_t)avx_isr_pusher37);
  set_interrupt_entry(38, (uint64_t)avx_isr_pusher38);
  set_interrupt_entry(39, (uint64_t)avx_isr_pusher39);
  set_interrupt_entry(40, (uint64_t)avx_isr_pusher40);
  set_interrupt_entry(41, (uint64_t)avx_isr_pusher41);
  set_interrupt_entry(42, (uint64_t)avx_isr_pusher42);
  set_interrupt_entry(43, (uint64_t)avx_isr_pusher43);
  set_interrupt_entry(44, (uint64_t)avx_isr_pusher44);
  set_interrupt_entry(45, (uint64_t)avx_isr_pusher45);
  set_interrupt_entry(46, (uint64_t)avx_isr_pusher46);
  set_interrupt_entry(47, (uint64_t)avx_isr_pusher47);
  set_interrupt_entry(48, (uint64_t)avx_isr_pusher48);
  set_interrupt_entry(49, (uint64_t)avx_isr_pusher49);
  set_interrupt_entry(50, (uint64_t)avx_isr_pusher50);
  set_interrupt_entry(51, (uint64_t)avx_isr_pusher51);
  set_interrupt_entry(52, (uint64_t)avx_isr_pusher52);
  set_interrupt_entry(53, (uint64_t)avx_isr_pusher53);
  set_interrupt_entry(54, (uint64_t)avx_isr_pusher54);
  set_interrupt_entry(55, (uint64_t)avx_isr_pusher55);
  set_interrupt_entry(56, (uint64_t)avx_isr_pusher56);
  set_interrupt_entry(57, (uint64_t)avx_isr_pusher57);
  set_interrupt_entry(58, (uint64_t)avx_isr_pusher58);
  set_interrupt_entry(59, (uint64_t)avx_isr_pusher59);
  set_interrupt_entry(60, (uint64_t)avx_isr_pusher60);
  set_interrupt_entry(61, (uint64_t)avx_isr_pusher61);
  set_interrupt_entry(62, (uint64_t)avx_isr_pusher62);
  set_interrupt_entry(63, (uint64_t)avx_isr_pusher63);
  set_interrupt_entry(64, (uint64_t)avx_isr_pusher64);
  set_interrupt_entry(65, (uint64_t)avx_isr_pusher65);
  set_interrupt_entry(66, (uint64_t)avx_isr_pusher66);
  set_interrupt_entry(67, (uint64_t)avx_isr_pusher67);
  set_interrupt_entry(68, (uint64_t)avx_isr_pusher68);
  set_interrupt_entry(69, (uint64_t)avx_isr_pusher69);
  set_interrupt_entry(70, (uint64_t)avx_isr_pusher70);
  set_interrupt_entry(71, (uint64_t)avx_isr_pusher71);
  set_interrupt_entry(72, (uint64_t)avx_isr_pusher72);
  set_interrupt_entry(73, (uint64_t)avx_isr_pusher73);
  set_interrupt_entry(74, (uint64_t)avx_isr_pusher74);
  set_interrupt_entry(75, (uint64_t)avx_isr_pusher75);
  set_interrupt_entry(76, (uint64_t)avx_isr_pusher76);
  set_interrupt_entry(77, (uint64_t)avx_isr_pusher77);
  set_interrupt_entry(78, (uint64_t)avx_isr_pusher78);
  set_interrupt_entry(79, (uint64_t)avx_isr_pusher79);
  set_interrupt_entry(80, (uint64_t)avx_isr_pusher80);
  set_interrupt_entry(81, (uint64_t)avx_isr_pusher81);
  set_interrupt_entry(82, (uint64_t)avx_isr_pusher82);
  set_interrupt_entry(83, (uint64_t)avx_isr_pusher83);
  set_interrupt_entry(84, (uint64_t)avx_isr_pusher84);
  set_interrupt_entry(85, (uint64_t)avx_isr_pusher85);
  set_interrupt_entry(86, (uint64_t)avx_isr_pusher86);
  set_interrupt_entry(87, (uint64_t)avx_isr_pusher87);
  set_interrupt_entry(88, (uint64_t)avx_isr_pusher88);
  set_interrupt_entry(89, (uint64_t)avx_isr_pusher89);
  set_interrupt_entry(90, (uint64_t)avx_isr_pusher90);
  set_interrupt_entry(91, (uint64_t)avx_isr_pusher91);
  set_interrupt_entry(92, (uint64_t)avx_isr_pusher92);
  set_interrupt_entry(93, (uint64_t)avx_isr_pusher93);
  set_interrupt_entry(94, (uint64_t)avx_isr_pusher94);
  set_interrupt_entry(95, (uint64_t)avx_isr_pusher95);
  set_interrupt_entry(96, (uint64_t)avx_isr_pusher96);
  set_interrupt_entry(97, (uint64_t)avx_isr_pusher97);
  set_interrupt_entry(98, (uint64_t)avx_isr_pusher98);
  set_interrupt_entry(99, (uint64_t)avx_isr_pusher99);
  set_interrupt_entry(100, (uint64_t)avx_isr_pusher100);
  set_interrupt_entry(101, (uint64_t)avx_isr_pusher101);
  set_interrupt_entry(102, (uint64_t)avx_isr_pusher102);
  set_interrupt_entry(103, (uint64_t)avx_isr_pusher103);
  set_interrupt_entry(104, (uint64_t)avx_isr_pusher104);
  set_interrupt_entry(105, (uint64_t)avx_isr_pusher105);
  set_interrupt_entry(106, (uint64_t)avx_isr_pusher106);
  set_interrupt_entry(107, (uint64_t)avx_isr_pusher107);
  set_interrupt_entry(108, (uint64_t)avx_isr_pusher108);
  set_interrupt_entry(109, (uint64_t)avx_isr_pusher109);
  set_interrupt_entry(110, (uint64_t)avx_isr_pusher110);
  set_interrupt_entry(111, (uint64_t)avx_isr_pusher111);
  set_interrupt_entry(112, (uint64_t)avx_isr_pusher112);
  set_interrupt_entry(113, (uint64_t)avx_isr_pusher113);
  set_interrupt_entry(114, (uint64_t)avx_isr_pusher114);
  set_interrupt_entry(115, (uint64_t)avx_isr_pusher115);
  set_interrupt_entry(116, (uint64_t)avx_isr_pusher116);
  set_interrupt_entry(117, (uint64_t)avx_isr_pusher117);
  set_interrupt_entry(118, (uint64_t)avx_isr_pusher118);
  set_interrupt_entry(119, (uint64_t)avx_isr_pusher119);
  set_interrupt_entry(120, (uint64_t)avx_isr_pusher120);
  set_interrupt_entry(121, (uint64_t)avx_isr_pusher121);
  set_interrupt_entry(122, (uint64_t)avx_isr_pusher122);
  set_interrupt_entry(123, (uint64_t)avx_isr_pusher123);
  set_interrupt_entry(124, (uint64_t)avx_isr_pusher124);
  set_interrupt_entry(125, (uint64_t)avx_isr_pusher125);
  set_interrupt_entry(126, (uint64_t)avx_isr_pusher126);
  set_interrupt_entry(127, (uint64_t)avx_isr_pusher127);
  set_interrupt_entry(128, (uint64_t)avx_isr_pusher128);
  set_interrupt_entry(129, (uint64_t)avx_isr_pusher129);
  set_interrupt_entry(130, (uint64_t)avx_isr_pusher130);
  set_interrupt_entry(131, (uint64_t)avx_isr_pusher131);
  set_interrupt_entry(132, (uint64_t)avx_isr_pusher132);
  set_interrupt_entry(133, (uint64_t)avx_isr_pusher133);
  set_interrupt_entry(134, (uint64_t)avx_isr_pusher134);
  set_interrupt_entry(135, (uint64_t)avx_isr_pusher135);
  set_interrupt_entry(136, (uint64_t)avx_isr_pusher136);
  set_interrupt_entry(137, (uint64_t)avx_isr_pusher137);
  set_interrupt_entry(138, (uint64_t)avx_isr_pusher138);
  set_interrupt_entry(139, (uint64_t)avx_isr_pusher139);
  set_interrupt_entry(140, (uint64_t)avx_isr_pusher140);
  set_interrupt_entry(141, (uint64_t)avx_isr_pusher141);
  set_interrupt_entry(142, (uint64_t)avx_isr_pusher142);
  set_interrupt_entry(143, (uint64_t)avx_isr_pusher143);
  set_interrupt_entry(144, (uint64_t)avx_isr_pusher144);
  set_interrupt_entry(145, (uint64_t)avx_isr_pusher145);
  set_interrupt_entry(146, (uint64_t)avx_isr_pusher146);
  set_interrupt_entry(147, (uint64_t)avx_isr_pusher147);
  set_interrupt_entry(148, (uint64_t)avx_isr_pusher148);
  set_interrupt_entry(149, (uint64_t)avx_isr_pusher149);
  set_interrupt_entry(150, (uint64_t)avx_isr_pusher150);
  set_interrupt_entry(151, (uint64_t)avx_isr_pusher151);
  set_interrupt_entry(152, (uint64_t)avx_isr_pusher152);
  set_interrupt_entry(153, (uint64_t)avx_isr_pusher153);
  set_interrupt_entry(154, (uint64_t)avx_isr_pusher154);
  set_interrupt_entry(155, (uint64_t)avx_isr_pusher155);
  set_interrupt_entry(156, (uint64_t)avx_isr_pusher156);
  set_interrupt_entry(157, (uint64_t)avx_isr_pusher157);
  set_interrupt_entry(158, (uint64_t)avx_isr_pusher158);
  set_interrupt_entry(159, (uint64_t)avx_isr_pusher159);
  set_interrupt_entry(160, (uint64_t)avx_isr_pusher160);
  set_interrupt_entry(161, (uint64_t)avx_isr_pusher161);
  set_interrupt_entry(162, (uint64_t)avx_isr_pusher162);
  set_interrupt_entry(163, (uint64_t)avx_isr_pusher163);
  set_interrupt_entry(164, (uint64_t)avx_isr_pusher164);
  set_interrupt_entry(165, (uint64_t)avx_isr_pusher165);
  set_interrupt_entry(166, (uint64_t)avx_isr_pusher166);
  set_interrupt_entry(167, (uint64_t)avx_isr_pusher167);
  set_interrupt_entry(168, (uint64_t)avx_isr_pusher168);
  set_interrupt_entry(169, (uint64_t)avx_isr_pusher169);
  set_interrupt_entry(170, (uint64_t)avx_isr_pusher170);
  set_interrupt_entry(171, (uint64_t)avx_isr_pusher171);
  set_interrupt_entry(172, (uint64_t)avx_isr_pusher172);
  set_interrupt_entry(173, (uint64_t)avx_isr_pusher173);
  set_interrupt_entry(174, (uint64_t)avx_isr_pusher174);
  set_interrupt_entry(175, (uint64_t)avx_isr_pusher175);
  set_interrupt_entry(176, (uint64_t)avx_isr_pusher176);
  set_interrupt_entry(177, (uint64_t)avx_isr_pusher177);
  set_interrupt_entry(178, (uint64_t)avx_isr_pusher178);
  set_interrupt_entry(179, (uint64_t)avx_isr_pusher179);
  set_interrupt_entry(180, (uint64_t)avx_isr_pusher180);
  set_interrupt_entry(181, (uint64_t)avx_isr_pusher181);
  set_interrupt_entry(182, (uint64_t)avx_isr_pusher182);
  set_interrupt_entry(183, (uint64_t)avx_isr_pusher183);
  set_interrupt_entry(184, (uint64_t)avx_isr_pusher184);
  set_interrupt_entry(185, (uint64_t)avx_isr_pusher185);
  set_interrupt_entry(186, (uint64_t)avx_isr_pusher186);
  set_interrupt_entry(187, (uint64_t)avx_isr_pusher187);
  set_interrupt_entry(188, (uint64_t)avx_isr_pusher188);
  set_interrupt_entry(189, (uint64_t)avx_isr_pusher189);
  set_interrupt_entry(190, (uint64_t)avx_isr_pusher190);
  set_interrupt_entry(191, (uint64_t)avx_isr_pusher191);
  set_interrupt_entry(192, (uint64_t)avx_isr_pusher192);
  set_interrupt_entry(193, (uint64_t)avx_isr_pusher193);
  set_interrupt_entry(194, (uint64_t)avx_isr_pusher194);
  set_interrupt_entry(195, (uint64_t)avx_isr_pusher195);
  set_interrupt_entry(196, (uint64_t)avx_isr_pusher196);
  set_interrupt_entry(197, (uint64_t)avx_isr_pusher197);
  set_interrupt_entry(198, (uint64_t)avx_isr_pusher198);
  set_interrupt_entry(199, (uint64_t)avx_isr_pusher199);
  set_interrupt_entry(200, (uint64_t)avx_isr_pusher200);
  set_interrupt_entry(201, (uint64_t)avx_isr_pusher201);
  set_interrupt_entry(202, (uint64_t)avx_isr_pusher202);
  set_interrupt_entry(203, (uint64_t)avx_isr_pusher203);
  set_interrupt_entry(204, (uint64_t)avx_isr_pusher204);
  set_interrupt_entry(205, (uint64_t)avx_isr_pusher205);
  set_interrupt_entry(206, (uint64_t)avx_isr_pusher206);
  set_interrupt_entry(207, (uint64_t)avx_isr_pusher207);
  set_interrupt_entry(208, (uint64_t)avx_isr_pusher208);
  set_interrupt_entry(209, (uint64_t)avx_isr_pusher209);
  set_interrupt_entry(210, (uint64_t)avx_isr_pusher210);
  set_interrupt_entry(211, (uint64_t)avx_isr_pusher211);
  set_interrupt_entry(212, (uint64_t)avx_isr_pusher212);
  set_interrupt_entry(213, (uint64_t)avx_isr_pusher213);
  set_interrupt_entry(214, (uint64_t)avx_isr_pusher214);
  set_interrupt_entry(215, (uint64_t)avx_isr_pusher215);
  set_interrupt_entry(216, (uint64_t)avx_isr_pusher216);
  set_interrupt_entry(217, (uint64_t)avx_isr_pusher217);
  set_interrupt_entry(218, (uint64_t)avx_isr_pusher218);
  set_interrupt_entry(219, (uint64_t)avx_isr_pusher219);
  set_interrupt_entry(220, (uint64_t)avx_isr_pusher220);
  set_interrupt_entry(221, (uint64_t)avx_isr_pusher221);
  set_interrupt_entry(222, (uint64_t)avx_isr_pusher222);
  set_interrupt_entry(223, (uint64_t)avx_isr_pusher223);
  set_interrupt_entry(224, (uint64_t)avx_isr_pusher224);
  set_interrupt_entry(225, (uint64_t)avx_isr_pusher225);
  set_interrupt_entry(226, (uint64_t)avx_isr_pusher226);
  set_interrupt_entry(227, (uint64_t)avx_isr_pusher227);
  set_interrupt_entry(228, (uint64_t)avx_isr_pusher228);
  set_interrupt_entry(229, (uint64_t)avx_isr_pusher229);
  set_interrupt_entry(230, (uint64_t)avx_isr_pusher230);
  set_interrupt_entry(231, (uint64_t)avx_isr_pusher231);
  set_interrupt_entry(232, (uint64_t)avx_isr_pusher232);
  set_interrupt_entry(233, (uint64_t)avx_isr_pusher233);
  set_interrupt_entry(234, (uint64_t)avx_isr_pusher234);
  set_interrupt_entry(235, (uint64_t)avx_isr_pusher235);
  set_interrupt_entry(236, (uint64_t)avx_isr_pusher236);
  set_interrupt_entry(237, (uint64_t)avx_isr_pusher237);
  set_interrupt_entry(238, (uint64_t)avx_isr_pusher238);
  set_interrupt_entry(239, (uint64_t)avx_isr_pusher239);
  set_interrupt_entry(240, (uint64_t)avx_isr_pusher240);
  set_interrupt_entry(241, (uint64_t)avx_isr_pusher241);
  set_interrupt_entry(242, (uint64_t)avx_isr_pusher242);
  set_interrupt_entry(243, (uint64_t)avx_isr_pusher243);
  set_interrupt_entry(244, (uint64_t)avx_isr_pusher244);
  set_interrupt_entry(245, (uint64_t)avx_isr_pusher245);
  set_interrupt_entry(246, (uint64_t)avx_isr_pusher246);
  set_interrupt_entry(247, (uint64_t)avx_isr_pusher247);
  set_interrupt_entry(248, (uint64_t)avx_isr_pusher248);
  set_interrupt_entry(249, (uint64_t)avx_isr_pusher249);
  set_interrupt_entry(250, (uint64_t)avx_isr_pusher250);
  set_interrupt_entry(251, (uint64_t)avx_isr_pusher251);
  set_interrupt_entry(252, (uint64_t)avx_isr_pusher252);
  set_interrupt_entry(253, (uint64_t)avx_isr_pusher253);
  set_interrupt_entry(254, (uint64_t)avx_isr_pusher254);
  set_interrupt_entry(255, (uint64_t)avx_isr_pusher255);

  //
  // Gotta love spreadsheet macros for this kind of stuff.
  // Use =$A$1&TEXT(B1,"00")&$C$1&TEXT(D1,"00")&$E$1 in Excel, with the below setup
  // (square brackets denote a cell):
  //
  //          A              B                C                D    E
  // 1 [set_interrupt_entry(] [32] [, (uint64_t)avx_isr_pusher] [32] [);]
  // 2                      [33]                              [33]
  // 3                      [34]                              [34]
  // ...                     ...                               ...
  // 255                    [255]                             [255]
  //
  // Drag the bottom right corner of the cell with the above macro--there should be a
  // little square there--and grin as 224 sequential interrupt functions technomagically
  // appear. :)
  //
  // Adapted from: https://superuser.com/questions/559218/increment-numbers-inside-a-string
  //

  set_idtr(idt_reg_data);
}

// Set up corresponding ISR function's IDT entry
// This is for architecturally-defined ISRs (IDT entries 0-31) and user-defined entries (32-255)
static void set_interrupt_entry(uint64_t isr_num, uint64_t isr_addr)
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
static void set_trap_entry(uint64_t isr_num, uint64_t isr_addr)
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
static void set_unused_entry(uint64_t isr_num)
{
  IDT_data[isr_num].Offset1 = 0;
  IDT_data[isr_num].SegmentSelector = 0x08;
  IDT_data[isr_num].ISTandZero = 0;
  IDT_data[isr_num].Misc = 0x0E; // P=0, DPL=0, S=0, placeholder interrupt type
  IDT_data[isr_num].Offset2 = 0;
  IDT_data[isr_num].Offset3 = 0;
  IDT_data[isr_num].Reserved = 0;
}

// These are special entries that make use of the IST mechanism for stack switching in 64-bit mode

// Nonmaskable interrupt
static void set_NMI_interrupt_entry(uint64_t isr_num, uint64_t isr_addr)
{
  uint16_t isr_base1 = (uint16_t)isr_addr;
  uint16_t isr_base2 = (uint16_t)(isr_addr >> 16);
  uint32_t isr_base3 = (uint32_t)(isr_addr >> 32);

  IDT_data[isr_num].Offset1 = isr_base1; // CS base is 0, so offset becomes the base address of ISR (interrupt service routine)
  IDT_data[isr_num].SegmentSelector = 0x08; // 64-bit code segment in GDT
  IDT_data[isr_num].ISTandZero = 1; // IST of 0 uses "modified legacy stack switch mechanism" (Intel Architecture Manual Vol. 3A, Section 6.14.4 Stack Switching in IA-32e Mode)
  // IST = 0 only matters for inter-privilege level changes that arise from interrrupts--keeping the same level doesn't switch stacks
  // IST = 1-7 would apply regardless of privilege level.
  IDT_data[isr_num].Misc = 0x8E; // 0x8E for interrupt (clears IF in RFLAGS), 0x8F for trap (does not clear IF in RFLAGS), P=1, DPL=0, S=0
  IDT_data[isr_num].Offset2 = isr_base2;
  IDT_data[isr_num].Offset3 = isr_base3;
  IDT_data[isr_num].Reserved = 0;
}

// Double fault
static void set_DF_interrupt_entry(uint64_t isr_num, uint64_t isr_addr)
{
  uint16_t isr_base1 = (uint16_t)isr_addr;
  uint16_t isr_base2 = (uint16_t)(isr_addr >> 16);
  uint32_t isr_base3 = (uint32_t)(isr_addr >> 32);

  IDT_data[isr_num].Offset1 = isr_base1; // CS base is 0, so offset becomes the base address of ISR (interrupt service routine)
  IDT_data[isr_num].SegmentSelector = 0x08; // 64-bit code segment in GDT
  IDT_data[isr_num].ISTandZero = 2; // IST of 0 uses "modified legacy stack switch mechanism" (Intel Architecture Manual Vol. 3A, Section 6.14.4 Stack Switching in IA-32e Mode)
  // IST = 0 only matters for inter-privilege level changes that arise from interrrupts--keeping the same level doesn't switch stacks
  // IST = 1-7 would apply regardless of privilege level.
  IDT_data[isr_num].Misc = 0x8E; // 0x8E for interrupt (clears IF in RFLAGS), 0x8F for trap (does not clear IF in RFLAGS), P=1, DPL=0, S=0
  IDT_data[isr_num].Offset2 = isr_base2;
  IDT_data[isr_num].Offset3 = isr_base3;
  IDT_data[isr_num].Reserved = 0;
}

// Machine Check
static void set_MC_interrupt_entry(uint64_t isr_num, uint64_t isr_addr)
{
  uint16_t isr_base1 = (uint16_t)isr_addr;
  uint16_t isr_base2 = (uint16_t)(isr_addr >> 16);
  uint32_t isr_base3 = (uint32_t)(isr_addr >> 32);

  IDT_data[isr_num].Offset1 = isr_base1; // CS base is 0, so offset becomes the base address of ISR (interrupt service routine)
  IDT_data[isr_num].SegmentSelector = 0x08; // 64-bit code segment in GDT
  IDT_data[isr_num].ISTandZero = 3; // IST of 0 uses "modified legacy stack switch mechanism" (Intel Architecture Manual Vol. 3A, Section 6.14.4 Stack Switching in IA-32e Mode)
  // IST = 0 only matters for inter-privilege level changes that arise from interrrupts--keeping the same level doesn't switch stacks
  // IST = 1-7 would apply regardless of privilege level.
  IDT_data[isr_num].Misc = 0x8E; // 0x8E for interrupt (clears IF in RFLAGS), 0x8F for trap (does not clear IF in RFLAGS), P=1, DPL=0, S=0
  IDT_data[isr_num].Offset2 = isr_base2;
  IDT_data[isr_num].Offset3 = isr_base3;
  IDT_data[isr_num].Reserved = 0;
}

// Debug (INT3)
static void set_BP_interrupt_entry(uint64_t isr_num, uint64_t isr_addr)
{
  uint16_t isr_base1 = (uint16_t)isr_addr;
  uint16_t isr_base2 = (uint16_t)(isr_addr >> 16);
  uint32_t isr_base3 = (uint32_t)(isr_addr >> 32);

  IDT_data[isr_num].Offset1 = isr_base1; // CS base is 0, so offset becomes the base address of ISR (interrupt service routine)
  IDT_data[isr_num].SegmentSelector = 0x08; // 64-bit code segment in GDT
  IDT_data[isr_num].ISTandZero = 4; // IST of 0 uses "modified legacy stack switch mechanism" (Intel Architecture Manual Vol. 3A, Section 6.14.4 Stack Switching in IA-32e Mode)
  // IST = 0 only matters for inter-privilege level changes that arise from interrrupts--keeping the same level doesn't switch stacks
  // IST = 1-7 would apply regardless of privilege level.
  IDT_data[isr_num].Misc = 0x8E; // 0x8E for interrupt (clears IF in RFLAGS), 0x8F for trap (does not clear IF in RFLAGS), P=1, DPL=0, S=0
  IDT_data[isr_num].Offset2 = isr_base2;
  IDT_data[isr_num].Offset3 = isr_base3;
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
  //TODO, can reclaim bootsrvdata after [_] this, [V] GDT, and [V] IDT
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
// Remember, the Intel Architecture Manual calls interrupts 0-31 "Exceptions" and 32-255 "Interrupts"...At least most of the time.
// This software calls interrupts without error codes "Interrupts" (labeled ISR) and with error codes "Exceptions" (labeled EXC)--this
// is what GCC does, as well. So, yes, some interrupts have a message that says "... Exception!" despite being handled in ISR code.
// Not much to be done about that.
//

//
// Interrupts (AVX, no error code)
//

// TODO: AVX handlers need to call XSAVE
// the only way to guarantee a free zeroed area without needing to use memset/AVX_memset on malloc'd memory (which would pollute AVX registers) is by making one in BSS (ultimately need one for each thread, but can be dealt with later with #define multi-function macros--like #defin num_cores 8, or something like that)
// No, need to use xsave memory type allocated like MemMap during the Enable_AVX function.
// __attribute__((aligned(64))) static XSAVE_AREA_LAYOUT * xsave_IRQ_region[XSAVE_SIZE];
// call xsave64 with asm, xcr0 mask high in edx : low in eax
// call cpuid to get info about state components (since using a static array, tbh this could happen after the xsave.. and probably needs to be anyways to know where everything is)
// do same with xrstor64:
// call xrstor64 with asm, xcr0 mask high in edx : low in eax
//

// TODO: This is temporary, 8kiB
// It can only serve one interrupt, not an interrupt interrupting an interrupt
#define XSAVE_SIZE (1 << 13)

__attribute__((aligned(64))) static volatile unsigned char xsave_space[XSAVE_SIZE] = {0};

void AVX_ISR_handler(INTERRUPT_FRAME * i_frame)
{
  uint64_t xsave_area_size = 0;
  // Leaf 0Dh, in %rbx of %rcx=1
  asm volatile ("cpuid"
                : "=b" (xsave_area_size) // Outputs
                : "a" (0x0D), "c" (0x01) // Inputs
                : "%rdx" // Clobbers
              );
  // TODO
  // General regs have been pushed to stack, get an XSAVE area -- how to ensure this does not use AVX? may need to use asm
  // Zero it with rep movsb (inline asm)

  // %rdx: Mask for xcr0 [63:32], %rax: Mask for xcr0 [31:0]
  asm volatile ("xsave64 %[area]"
                : // No outputs
                : "a" (0xE7), "d" (0x00), [area] "m" (xsave_space) // Inputs
                : "memory" // Clobbers
              );

  // OK, since xsave has been called we can now safely use AVX instructions in this interrupt--up until xrstor is called, at any rate.

  printf("xsave_area_size: %#qx\r\n", xsave_area_size);
  AVX_regdump((XSAVE_AREA_LAYOUT*)xsave_space); // TODO: put this in the right places

  switch(i_frame->isr_num)
  {

    //
    // Predefined System Interrupts and Exceptions
    //

    case 0:
      printf("Fault #DE: Divide Error! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
    case 1:
      printf("Fault/Trap #DB: Debug Exception! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
    case 2:
      printf("NMI: Nonmaskable Interrupt! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
    case 3:
      printf("Trap #BP: Breakpoint! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
    case 4:
      printf("Trap #OF: Overflow! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
    case 5:
      printf("Fault #BR: BOUND Range Exceeded! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
    case 6:
      printf("Fault #UD: Invalid or Undefined Opcode! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
    case 7:
      printf("Fault #NM: Device Not Available Exception! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
// 8 is in AVX EXC
    case 9:
      printf("Fault (i386): Coprocessor Segment Overrun! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
// 10-14 are in AVX EXC
// 15 is reserved and will invoke the default error
    case 16:
      printf("Fault #MF: x87 Math Error! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
// 17 is in AVX EXC
    case 18:
      printf("Abort #MC: Machine Check! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
    case 19:
      printf("Fault #XM: SIMD Floating-Point Exception! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
    case 20:
      printf("Fault #VE: Virtualization Exception! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
// 21-31 are reserved and will invoke the default error

    //
    // User-Defined Interrupts
    //

//    case 32: // Minimum allowed user-defined case number
//    // Case 32 code
//      break;
//    ....
//    case 255: // Maximum allowed case number
//    // Case 255 code
//      break;

    default:
      printf("AVX_ISR_handler: Unhandled Interrupt! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
  }

  // %rdx: Mask for xcr0 [63:32], %rax: Mask for xcr0 [31:0]
  asm volatile ("xrstor64 %[area]"
                : // No outputs
                : "a" (0xE7), "d" (0x00), [area] "m" (xsave_space) // Inputs
                : "memory" // Clobbers
              );

}

//
// Interrupts (no AVX, no error code)
//

void ISR_handler(INTERRUPT_FRAME * i_frame)
{
  switch(i_frame->isr_num)
  {

    // For small things that are guaranteed not to touch avx regs

    default:
      printf("ISR_handler: Unhandled Interrupt! IDT Entry: %#qu\r\n", i_frame->isr_num);
      ISR_regdump(i_frame);
      asm volatile("hlt");
      break;
  }
}

//
// Exceptions (AVX, have error code)
//

void AVX_EXC_handler(EXCEPTION_FRAME * e_frame)
{
  // TODO: get xsave area

  // %rdx: Mask for xcr0 [63:32], %rax: Mask for xcr0 [31:0]
  asm volatile ("xsave64 %[area]"
                : // No outputs
                : "a" (0xE7), "d" (0x00), [area] "m" (xsave_space) // Inputs
                : "memory" // Clobbers
              );

  switch(e_frame->isr_num)
  {

    //
    // Predefined System Interrupts and Exceptions
    //

// 0-7 are in AVX ISR
    case 8:
      printf("Abort #DF: Double Fault! IDT Entry: %#qu, Error Code (always 0): %#qx\r\n", e_frame->isr_num, e_frame->error_code);
      EXC_regdump(e_frame);
      asm volatile("hlt");
      break;
// 9 is in AVX ISR
    case 10:
      printf("Fault #TS: Invalid TSS! IDT Entry: %#qu, Error Code: %#qx\r\n", e_frame->isr_num, e_frame->error_code);
      EXC_regdump(e_frame);
      asm volatile("hlt");
      break;
    case 11:
      printf("Fault #NP: Segment Not Present! IDT Entry: %#qu, Error Code: %#qx\r\n", e_frame->isr_num, e_frame->error_code);
      EXC_regdump(e_frame);
      asm volatile("hlt");
      break;
    case 12:
      printf("Fault #SS: Stack Segment Fault! IDT Entry: %#qu, Error Code: %#qx\r\n", e_frame->isr_num, e_frame->error_code);
      EXC_regdump(e_frame);
      asm volatile("hlt");
      break;
    case 13:
      printf("Fault #GP: General Protection! IDT Entry: %#qu, Error Code: %#qx\r\n", e_frame->isr_num, e_frame->error_code);
      EXC_regdump(e_frame);
      asm volatile("hlt");
      break;
    case 14:
      printf("Fault #PF: Page Fault! IDT Entry: %#qu, Error Code: %#qx\r\n", e_frame->isr_num, e_frame->error_code);
      EXC_regdump(e_frame);
      asm volatile("hlt");
      break;
// 15 is reserved in AVX ISR
// 16 is in AVX ISR
    case 17:
      printf("Fault #AC: Alignment Check! IDT Entry: %#qu, Error Code (usually 0): %#qx\r\n", e_frame->isr_num, e_frame->error_code);
      EXC_regdump(e_frame);
      asm volatile("hlt");
      break;
// 18-20 are in AVX ISR
// 21-29 are reserved in AVX ISR
    case 30:
      printf("Fault #SX: Security Exception! IDT Entry: %#qu, Error Code: %#qx\r\n", e_frame->isr_num, e_frame->error_code);
      EXC_regdump(e_frame);
      asm volatile("hlt");
      break;
// 31 is reserved in AVX ISR
// 32-255 default to AVX ISR
    default:
      printf("AVX_EXC_handler: Unhandled Exception! IDT Entry: %#qu, Error Code: %#qx\r\n", e_frame->isr_num, e_frame->error_code);
      EXC_regdump(e_frame);
      asm volatile("hlt");
      break;
  }

  // %rdx: Mask for xcr0 [63:32], %rax: Mask for xcr0 [31:0]
  asm volatile ("xrstor64 %[area]"
                : // No outputs
                : "a" (0xE7), "d" (0x00), [area] "m" (xsave_space) // Inputs
                : "memory" // Clobbers
              );
}

//
// Exceptions (no AVX, have error code)
//

void EXC_handler(EXCEPTION_FRAME * e_frame)
{
  switch(e_frame->isr_num)
  {

    // For small things that are guaranteed not to touch avx regs

    default:
      printf("EXC_handler: Unhandled Exception! IDT Entry: %#qu, Error Code: %#qx\r\n", e_frame->isr_num, e_frame->error_code);
      EXC_regdump(e_frame);
      asm volatile("hlt");
      break;
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
// Interrupt Support Functions: Helpers for Interrupt and Exception Handlers
//----------------------------------------------------------------------------------------------------------------------------------
//
// Various interrupt and exception support functions are defined here, e.g. register dumps.
// Functions here strictly correspond to their similarly-named interrupt handlers.
//
// *Do not mix them* as the ISR and EXC data structures are different, which would put values in the wrong places.
//

//
// Interrupts (no error code)
//

void ISR_regdump(INTERRUPT_FRAME * i_frame)
{
  printf("rax: %#qx, rbx: %#qx, rcx: %#qx, rdx: %#qx, rsi: %#qx, rdi: %#qx\r\n", i_frame->rax, i_frame->rbx, i_frame->rcx, i_frame->rdx, i_frame->rsi, i_frame->rdi);
  printf("r8: %#qx, r9: %#qx, r10: %#qx, r11: %#qx, r12: %#qx, r13: %#qx\r\n", i_frame->r8, i_frame->r9, i_frame->r10, i_frame->r11, i_frame->r12, i_frame->r13);
  printf("r14: %#qx, r15: %#qx, rbp: %#qx, rip: %#qx, cs: %#qx, rflags: %#qx\r\n", i_frame->r14, i_frame->r15, i_frame->rbp, i_frame->rip, i_frame->cs, i_frame->rflags);
  printf("rsp: %#qx, ss: %#qx\r\n", i_frame->rsp, i_frame->ss);
}

//
// Exceptions (have error code)
//

void EXC_regdump(EXCEPTION_FRAME * e_frame)
{
  printf("rax: %#qx, rbx: %#qx, rcx: %#qx, rdx: %#qx, rsi: %#qx, rdi: %#qx\r\n", e_frame->rax, e_frame->rbx, e_frame->rcx, e_frame->rdx, e_frame->rsi, e_frame->rdi);
  printf("r8: %#qx, r9: %#qx, r10: %#qx, r11: %#qx, r12: %#qx, r13: %#qx\r\n", e_frame->r8, e_frame->r9, e_frame->r10, e_frame->r11, e_frame->r12, e_frame->r13);
  printf("r14: %#qx, r15: %#qx, rbp: %#qx, rip: %#qx, cs: %#qx, rflags: %#qx\r\n", e_frame->r14, e_frame->r15, e_frame->rbp, e_frame->rip, e_frame->cs, e_frame->rflags);
  printf("rsp: %#qx, ss: %#qx\r\n", e_frame->rsp, e_frame->ss);
}

//
// AVX Dump
//

void AVX_regdump(XSAVE_AREA_LAYOUT * layout_area)
{
  printf("fow: %#hx, fsw: %#hx, ftw: %#hhx, fop: %#hx, fip: %#qx, fdp: %#qx\r\n", layout_area->fcw, layout_area->fsw, layout_area->ftw, layout_area->fop, layout_area->fip, layout_area->fdp);
  printf("mxcsr: %#qx, mxcsr_mask: %#qx, xstate_bv: %#qx, xcomp_bv: %#qx\r\n", layout_area->mxcsr, layout_area->mxcsr_mask, layout_area->xstate_bv, layout_area->xcomp_bv);

#ifdef __AVX512F__

  uint64_t avx512_opmask_offset = 0;
  // Leaf 0Dh, AVX512 opmask is user state component 5
  asm volatile ("cpuid"
                : "=b" (avx512_opmask_offset) // Outputs
                : "a" (0x0D), "c" (0x05) // Inputs
                : "%rdx" // Clobbers
              );

  uint64_t avx512_zmm_hi256_offset = 0;
  // Leaf 0Dh, AVX512 ZMM_Hi256 is user state component 6
  asm volatile ("cpuid"
                : "=b" (avx512_zmm_hi256_offset) // Outputs
                : "a" (0x0D), "c" (0x06) // Inputs
                : "%rdx" // Clobbers
              );

  uint64_t avx512_hi16_zmm_offset = 0;
  // Leaf 0Dh, AVX512 Hi16_ZMM is user state component 7
  asm volatile ("cpuid"
                : "=b" (avx512_hi16_zmm_offset) // Outputs
                : "a" (0x0D), "c" (0x07) // Inputs
                : "%rdx" // Clobbers
              );

  uint64_t avx_offset = 0;
  // Leaf 0Dh, AVX is user state component 2
  asm volatile ("cpuid"
                : "=b" (avx_offset) // Outputs
                : "a" (0x0D), "c" (0x02) // Inputs
                : "%rdx" // Clobbers
              );

  // Hope screen resolution is high enough to see all this...
  // ZMM_Hi256, AVX, XMM
  printf("ZMM0: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 24), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 16), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 8), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 0), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 8), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 0), layout_area->xmm0[1], layout_area->xmm0[0]);
  printf("ZMM1: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 56), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 48), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 40), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 32), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 24), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 16), layout_area->xmm1[1], layout_area->xmm1[0]);
  printf("ZMM2: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 88), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 80), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 72), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 64), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 40), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 32), layout_area->xmm2[1], layout_area->xmm2[0]);
  printf("ZMM3: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 120), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 112), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 104), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 96), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 56), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 48), layout_area->xmm3[1], layout_area->xmm3[0]);
  printf("ZMM4: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 152), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 144), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 136), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 128), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 72), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 64), layout_area->xmm4[1], layout_area->xmm4[0]);
  printf("ZMM5: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 184), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 176), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 168), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 160), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 88), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 80), layout_area->xmm5[1], layout_area->xmm5[0]);
  printf("ZMM6: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 216), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 208), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 200), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 192), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 104), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 96), layout_area->xmm6[1], layout_area->xmm6[0]);
  printf("ZMM7: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 248), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 240), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 232), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 224), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 120), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 112), layout_area->xmm7[1], layout_area->xmm7[0]);
  printf("ZMM8: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 280), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 272), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 264), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 256), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 136), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 128), layout_area->xmm8[1], layout_area->xmm8[0]);
  printf("ZMM9: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 312), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 304), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 296), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 288), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 152), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 144), layout_area->xmm9[1], layout_area->xmm9[0]);
  printf("ZMM10: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 344), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 336), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 328), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 320), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 168), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 160), layout_area->xmm10[1], layout_area->xmm10[0]);
  printf("ZMM11: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 376), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 368), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 360), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 352), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 184), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 176), layout_area->xmm11[1], layout_area->xmm11[0]);
  printf("ZMM12: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 408), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 400), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 392), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 384), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 200), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 192), layout_area->xmm12[1], layout_area->xmm12[0]);
  printf("ZMM13: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 440), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 432), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 424), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 416), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 216), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 208), layout_area->xmm13[1], layout_area->xmm13[0]);
  printf("ZMM14: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 472), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 464), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 456), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 448), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 232), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 224), layout_area->xmm14[1], layout_area->xmm14[0]);
  printf("ZMM15: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 504), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 496), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 488), *(uint64_t*) ((uint8_t*)layout_area + avx512_zmm_hi256_offset + 480), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 248), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 240), layout_area->xmm15[1], layout_area->xmm15[0]);
  // Hi16_ZMM
  printf("ZMM16: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 56), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 48), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 40), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 32), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 24), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 16), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 8), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 0));
  printf("ZMM17: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 120), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 112), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 104), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 96), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 88), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 80), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 72), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 64));
  printf("ZMM18: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 184), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 176), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 168), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 160), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 152), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 144), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 136), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 128));
  printf("ZMM19: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 248), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 240), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 232), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 224), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 216), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 208), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 200), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 192));
  printf("ZMM20: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 312), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 304), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 296), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 288), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 280), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 272), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 264), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 256));
  printf("ZMM21: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 376), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 368), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 360), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 352), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 344), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 336), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 328), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 320));
  printf("ZMM22: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 440), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 432), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 424), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 416), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 408), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 400), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 392), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 384));
  printf("ZMM23: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 504), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 496), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 488), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 480), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 472), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 464), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 456), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 448));
  printf("ZMM24: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 568), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 560), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 552), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 544), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 536), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 528), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 520), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 512));
  printf("ZMM25: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 632), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 624), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 616), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 608), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 600), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 592), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 584), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 576));
  printf("ZMM26: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 696), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 688), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 680), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 672), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 664), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 656), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 648), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 640));
  printf("ZMM27: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 760), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 752), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 744), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 736), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 728), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 720), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 712), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 704));
  printf("ZMM28: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 824), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 816), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 808), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 800), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 792), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 784), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 776), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 768));
  printf("ZMM29: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 888), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 880), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 872), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 864), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 856), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 848), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 840), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 832));
  printf("ZMM30: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 952), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 944), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 936), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 928), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 920), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 912), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 904), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 896));
  printf("ZMM31: 0x%016qx%016qx%016qx%016qx%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 1016), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 1008), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 1000), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 992), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 984), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 976), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 968), *(uint64_t*) ((uint8_t*)layout_area + avx512_hi16_zmm_offset + 960));

  // Apparently the opmask area in the XSAVE extended region assumes 64-bit opmask registers, even though AVX512F only uses 16-bit opmask register sizes. At least, all documentation on the subject appears to say this is the case...
  printf("k0: %#qx, k1: %#qx, k2: %#qx, k3: %#qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_opmask_offset + 0), *(uint64_t*) ((uint8_t*)layout_area + avx512_opmask_offset + 8), *(uint64_t*) ((uint8_t*)layout_area + avx512_opmask_offset + 16), *(uint64_t*) ((uint8_t*)layout_area + avx512_opmask_offset + 24));
  printf("k4: %#qx, k5: %#qx, k6: %#qx, k7: %#qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx512_opmask_offset + 32), *(uint64_t*) ((uint8_t*)layout_area + avx512_opmask_offset + 40), *(uint64_t*) ((uint8_t*)layout_area + avx512_opmask_offset + 48), *(uint64_t*) ((uint8_t*)layout_area + avx512_opmask_offset + 56));

  // ...I wonder if AVX-1024 will ever be a thing.

#elif __AVX__
  uint64_t avx_offset = 0;
  // Leaf 0Dh, AVX is user state component 2
  asm volatile ("cpuid"
                : "=b" (avx_offset) // Outputs
                : "a" (0x0D), "c" (0x02) // Inputs
                : "%rdx" // Clobbers
              );

  // AVX, XMM
  printf("YMM0: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 8), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 0), layout_area->xmm0[1], layout_area->xmm0[0]);
  printf("YMM1: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 24), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 16), layout_area->xmm1[1], layout_area->xmm1[0]);
  printf("YMM2: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 40), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 32), layout_area->xmm2[1], layout_area->xmm2[0]);
  printf("YMM3: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 56), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 48), layout_area->xmm3[1], layout_area->xmm3[0]);
  printf("YMM4: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 72), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 64), layout_area->xmm4[1], layout_area->xmm4[0]);
  printf("YMM5: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 88), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 80), layout_area->xmm5[1], layout_area->xmm5[0]);
  printf("YMM6: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 104), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 96), layout_area->xmm6[1], layout_area->xmm6[0]);
  printf("YMM7: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 120), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 112), layout_area->xmm7[1], layout_area->xmm7[0]);
  printf("YMM8: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 136), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 128), layout_area->xmm8[1], layout_area->xmm8[0]);
  printf("YMM9: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 152), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 144), layout_area->xmm9[1], layout_area->xmm9[0]);
  printf("YMM10: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 168), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 160), layout_area->xmm10[1], layout_area->xmm10[0]);
  printf("YMM11: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 184), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 176), layout_area->xmm11[1], layout_area->xmm11[0]);
  printf("YMM12: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 200), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 192), layout_area->xmm12[1], layout_area->xmm12[0]);
  printf("YMM13: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 216), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 208), layout_area->xmm13[1], layout_area->xmm13[0]);
  printf("YMM14: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 232), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 224), layout_area->xmm14[1], layout_area->xmm14[0]);
  printf("YMM15: 0x%016qx%016qx%016qx%016qx\r\n", *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 248), *(uint64_t*) ((uint8_t*)layout_area + avx_offset + 240), layout_area->xmm15[1], layout_area->xmm15[0]);

#else

// SSE part is built into XSAVE area
  printf("XMM0: 0x%016qx%016qx\r\n", layout_area->xmm0[1], layout_area->xmm0[0]);
  printf("XMM1: 0x%016qx%016qx\r\n", layout_area->xmm1[1], layout_area->xmm1[0]);
  printf("XMM2: 0x%016qx%016qx\r\n", layout_area->xmm2[1], layout_area->xmm2[0]);
  printf("XMM3: 0x%016qx%016qx\r\n", layout_area->xmm3[1], layout_area->xmm3[0]);
  printf("XMM4: 0x%016qx%016qx\r\n", layout_area->xmm4[1], layout_area->xmm4[0]);
  printf("XMM5: 0x%016qx%016qx\r\n", layout_area->xmm5[1], layout_area->xmm5[0]);
  printf("XMM6: 0x%016qx%016qx\r\n", layout_area->xmm6[1], layout_area->xmm6[0]);
  printf("XMM7: 0x%016qx%016qx\r\n", layout_area->xmm7[1], layout_area->xmm7[0]);
  printf("XMM8: 0x%016qx%016qx\r\n", layout_area->xmm8[1], layout_area->xmm8[0]);
  printf("XMM9: 0x%016qx%016qx\r\n", layout_area->xmm9[1], layout_area->xmm9[0]);
  printf("XMM10: 0x%016qx%016qx\r\n", layout_area->xmm10[1], layout_area->xmm10[0]);
  printf("XMM11: 0x%016qx%016qx\r\n", layout_area->xmm11[1], layout_area->xmm11[0]);
  printf("XMM12: 0x%016qx%016qx\r\n", layout_area->xmm12[1], layout_area->xmm12[0]);
  printf("XMM13: 0x%016qx%016qx\r\n", layout_area->xmm13[1], layout_area->xmm13[0]);
  printf("XMM14: 0x%016qx%016qx\r\n", layout_area->xmm14[1], layout_area->xmm14[0]);
  printf("XMM15: 0x%016qx%016qx\r\n", layout_area->xmm15[1], layout_area->xmm15[0]);

// So is x87/MMX part

  printf("ST/MM0: 0x%016qx%016qx\r\n", layout_area->st_mm_0[1], layout_area->st_mm_0[0]);
  printf("ST/MM1: 0x%016qx%016qx\r\n", layout_area->st_mm_1[1], layout_area->st_mm_1[0]);
  printf("ST/MM2: 0x%016qx%016qx\r\n", layout_area->st_mm_2[1], layout_area->st_mm_2[0]);
  printf("ST/MM3: 0x%016qx%016qx\r\n", layout_area->st_mm_3[1], layout_area->st_mm_3[0]);
  printf("ST/MM4: 0x%016qx%016qx\r\n", layout_area->st_mm_4[1], layout_area->st_mm_4[0]);
  printf("ST/MM5: 0x%016qx%016qx\r\n", layout_area->st_mm_5[1], layout_area->st_mm_5[0]);
  printf("ST/MM6: 0x%016qx%016qx\r\n", layout_area->st_mm_6[1], layout_area->st_mm_6[0]);
  printf("ST/MM7: 0x%016qx%016qx\r\n", layout_area->st_mm_7[1], layout_area->st_mm_7[0]);
#endif

}
