//==================================================================================================================================
//  Simple Kernel: Memory Functions
//==================================================================================================================================
//
// Version 1.0
//
// Author:
//  KNNSpeed
//
// Source Code:
//  https://github.com/KNNSpeed/Simple-Kernel
//
// This file contains memory-related functions. Derived from V1.2 of https://github.com/KNNSpeed/Simple-UEFI-Bootloader
//

#include "Kernel64.h"

// AVX_memcmp and related functions in memcmp.c take care of memory comparisons now.

// TODO: malloc--it's based on ActuallyFreeAddress/ByPage

//----------------------------------------------------------------------------------------------------------------------------------
//  VerifyZeroMem: Verify Memory Is Free
//----------------------------------------------------------------------------------------------------------------------------------
//
// Return 0 if desired section of memory is zeroed (for use in "if" statements)
//

uint8_t VerifyZeroMem(uint64_t NumBytes, uint64_t BaseAddr) // BaseAddr is a 64-bit unsigned int whose value is the memory address
{
  for(uint64_t i = 0; i < NumBytes; i++)
  {
    if(*(EFI_PHYSICAL_ADDRESS*)(BaseAddr + i) != 0)
    {
      return 1;
    }
  }
  return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  ActuallyFreeAddress: Find A Free Memory Address, Bottom-Up
//----------------------------------------------------------------------------------------------------------------------------------
//
// This is meant to work in the event that AllocateAnyPages fails, but could have other uses. Returns the next EfiConventionalMemory
// area that is > the supplied OldAddress.
//

EFI_PHYSICAL_ADDRESS ActuallyFreeAddress(uint64_t pages, EFI_PHYSICAL_ADDRESS OldAddress, EFI_MEMORY_DESCRIPTOR * MemMap, UINTN MemMapSize, UINTN MemMapDescriptorSize)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the end address... which should just be the start of the next section.
  // Check for EfiConventionalMemory in the map
  for(Piece = MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((UINT8*)MemMap + MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Piece + MemMapDescriptorSize))
  {
    // Within each compatible EfiConventionalMemory, look for space
    if((Piece->Type == EfiConventionalMemory) && (Piece->NumberOfPages >= pages) && (Piece->PhysicalStart > OldAddress))
    {
      break;
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((UINT8*)MemMap + MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free addresses...\r\n");
#endif
    return -1;
  }

  return Piece->PhysicalStart;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  ActuallyFreeAddressByPage: Find A Free Memory Address, Bottom-Up, The Hard Way
//----------------------------------------------------------------------------------------------------------------------------------
//
// This is meant to work in the event that AllocateAnyPages fails, but could have other uses. Returns the next page address marked as
// free (EfiConventionalMemory) that is > the supplied OldAddress.
//

EFI_PHYSICAL_ADDRESS ActuallyFreeAddressByPage(UINT64 pages, EFI_PHYSICAL_ADDRESS OldAddress, EFI_MEMORY_DESCRIPTOR * MemMap, UINTN MemMapSize, UINTN MemMapDescriptorSize)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_PHYSICAL_ADDRESS PhysicalEnd;
  EFI_PHYSICAL_ADDRESS DiscoveredAddress;

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the end address... which should just be the start of the next section.
  // Check for EfiConventionalMemory in the map
  for(Piece = MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((UINT8*)MemMap + MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Piece + MemMapDescriptorSize))
  {
    // Within each compatible EfiConventionalMemory, look for space
    if((Piece->Type == EfiConventionalMemory) && (Piece->NumberOfPages >= pages))
    {
      PhysicalEnd = Piece->PhysicalStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT) - 1; // Get the end of this range
      // (pages*EFI_PAGE_SIZE) or (pages << EFI_PAGE_SHIFT) gives the size the kernel would take up in memory
      if((OldAddress >= Piece->PhysicalStart) && ((OldAddress + (pages << EFI_PAGE_SHIFT)) < PhysicalEnd)) // Bounds check on OldAddress
      {
        // Return the next available page's address in the range. We need to go page-by-page for the really buggy systems.
        DiscoveredAddress = OldAddress + EFI_PAGE_SIZE; // Left shift EFI_PAGE_SIZE by 1 or 2 to check every 0x10 or 0x100 pages (must also modify the above PhysicalEnd bound check)
        break;
        // If PhysicalEnd == OldAddress, we need to go to the next EfiConventionalMemory range
      }
      else if(Piece->PhysicalStart > OldAddress) // Try a new range
      {
        DiscoveredAddress = Piece->PhysicalStart;
        break;
      }
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((UINT8*)MemMap + MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free addresses by page...\r\n");
#endif
    return -1;
  }

  return DiscoveredAddress;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  print_kernel_memmap: The Ultimate Debugging Tool
//----------------------------------------------------------------------------------------------------------------------------------
//
// Get the system memory map, parse it, and print it. Print the whole thing.
//

/* Reminder of EFI_MEMORY_DESCRIPTOR format:

typedef UINT64          EFI_PHYSICAL_ADDRESS;
typedef UINT64          EFI_VIRTUAL_ADDRESS;

#define EFI_MEMORY_DESCRIPTOR_VERSION  1
typedef struct {
    UINT32                          Type;           // Field size is 32 bits followed by 32 bit pad
    UINT32                          Pad;            // There's no pad in the spec...
    EFI_PHYSICAL_ADDRESS            PhysicalStart;  // Field size is 64 bits
    EFI_VIRTUAL_ADDRESS             VirtualStart;   // Field size is 64 bits
    UINT64                          NumberOfPages;  // Field size is 64 bits
    UINT64                          Attribute;      // Field size is 64 bits
} EFI_MEMORY_DESCRIPTOR;
*/

void print_kernel_memmap(EFI_MEMORY_DESCRIPTOR * MemMap, UINTN MemMapSize, UINTN MemMapDescriptorSize)
{
  EFI_MEMORY_DESCRIPTOR * Piece;
  uint16_t line = 0;

  char * mem_types[] = {
      "EfiReservedMemoryType     ",
      "EfiLoaderCode             ",
      "EfiLoaderData             ",
      "EfiBootServicesCode       ",
      "EfiBootServicesData       ",
      "EfiRuntimeServicesCode    ",
      "EfiRuntimeServicesData    ",
      "EfiConventionalMemory     ",
      "EfiUnusableMemory         ",
      "EfiACPIReclaimMemory      ",
      "EfiACPIMemoryNVS          ",
      "EfiMemoryMappedIO         ",
      "EfiMemoryMappedIOPortSpace",
      "EfiPalCode                ",
      "EfiPersistentMemory       ",
      "EfiMaxMemoryType          "
  };

  printf("MemMapSize: %qu, MemMapDescriptorSize: %qu\r\n", MemMapSize, MemMapDescriptorSize);

  // Multiply NumOfPages by EFI_PAGE_SIZE or do (NumOfPages << EFI_PAGE_SHIFT) to get the end address... which should just be the start of the next section.
  for(Piece = MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((UINT8*)MemMap + MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Piece + MemMapDescriptorSize))
  {
    if(line%20 == 0)
    {
//      Keywait(L"\0");
      printf("#   Memory Type                Phys Addr Start   Virt Addr Start   Num Of Pages   Attr\r\n");
    }

    printf("%2hu: %s %#016qx   %#016qx %#qx %#qx\r\n", line, mem_types[Piece->Type], Piece->PhysicalStart, Piece->VirtualStart, Piece->NumberOfPages, Piece->Attribute);
    line++;
  }
}
