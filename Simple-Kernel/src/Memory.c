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
// This file contains memory-related functions. Derived from V1.4 of https://github.com/KNNSpeed/Simple-UEFI-Bootloader
//

#include "Kernel64.h"

// AVX_memcmp and related functions in memcmp.c take care of memory comparisons now.
// AVX_memset zeroes things.

// TODO: malloc--it's based on ActuallyFreeAddress/ByPage
// Also: calloc, realloc, and (free, freepages, vfree, vfreepages)

__attribute__((malloc)) void * malloc(size_t numbytes)
{
  if(numbytes <= 16)
  {
    return malloc16(numbytes); // 216-byte aligned
  }
  else if(numbytes <= 32)
  {
    return malloc32(numbytes); // 32-byte aligned
  }
  else if(numbytes < 4096)
  {
    return malloc64(numbytes); // 64-byte aligned
  }
  else // >= 4096
  {
    size_t numbytes_to_pages = numbytes >> EFI_PAGE_SHIFT;
    if(numbytes & EFI_PAGE_MASK)
    {
      numbytes_to_pages++;
    }
    return malloc4k(numbytes_to_pages);
  }
}

__attribute__((malloc)) void * Vmalloc(size_t numbytes)
{
  if(numbytes <= 16)
  {
    return Vmalloc16(numbytes); // 216-byte aligned
  }
  else if(numbytes <= 32)
  {
    return Vmalloc32(numbytes); // 32-byte aligned
  }
  else if(numbytes < 4096)
  {
    return Vmalloc64(numbytes); // 64-byte aligned
  }
  else // >= 4096
  {
    size_t numbytes_to_pages = numbytes >> EFI_PAGE_SHIFT;
    if(numbytes & EFI_PAGE_MASK)
    {
      numbytes_to_pages++;
    }
    return malloc4k(numbytes_to_pages);
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
//  mallocX: Allocate Physical Memory Aligned to X Bytes
//----------------------------------------------------------------------------------------------------------------------------------
//
// Each of these allocate bytes at physical addresses aligned on X-byte boundaries (X in mallocX).
//
// Note that the 16, 32, and 64-byte alignment versions take the total number of bytes to allocate, but the 4k version requires the
// number of sets of 4k bytes (a.k.a. the number of pages, in UEFI parlance).
//

__attribute__((malloc)) void * malloc16(size_t numbytes)
{
  EFI_PHYSICAL_ADDRESS new_buffer = 0; // Make this 0x100000000 to only operate above 4GB

  new_buffer = AllocateFreeAddressBy16Bytes(numbytes, new_buffer);

  return (void*)new_buffer;
}

__attribute__((malloc)) void * malloc32(size_t numbytes)
{
  EFI_PHYSICAL_ADDRESS new_buffer = 0; // Make this 0x100000000 to only operate above 4GB

  new_buffer = AllocateFreeAddressBy32Bytes(numbytes, new_buffer);

  return (void*)new_buffer;
}

__attribute__((malloc)) void * malloc64(size_t numbytes)
{
  EFI_PHYSICAL_ADDRESS new_buffer = 0; // Make this 0x100000000 to only operate above 4GB

  new_buffer = AllocateFreeAddressBy64Bytes(numbytes, new_buffer);

  return (void*)new_buffer;
}

__attribute__((malloc)) void * malloc4k(size_t pages)
{
  EFI_PHYSICAL_ADDRESS new_buffer = 0; // Make this 0x100000000 to only operate above 4GB

  new_buffer = AllocateFreeAddressByPage(pages, new_buffer);

  return (void*)new_buffer;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  VmallocX: Allocate Virtual Memory Aligned to X Bytes
//----------------------------------------------------------------------------------------------------------------------------------
//
// Each of these allocate bytes at virtual addresses aligned on X-byte boundaries (X in VmallocX).
//
// Note that the 16, 32, and 64-byte alignment versions take the total number of bytes to allocate, but the 4k version requires the
// number of sets of 4k bytes (a.k.a. the number of pages, in UEFI parlance).
//

__attribute__((malloc)) void * Vmalloc16(size_t numbytes)
{
  EFI_VIRTUAL_ADDRESS new_buffer = 0; // Make this 0x100000000 to only operate above 4GB

  new_buffer = VAllocateFreeAddressBy16Bytes(numbytes, new_buffer);

  return (void*)new_buffer;
}

__attribute__((malloc)) void * Vmalloc32(size_t numbytes)
{
  EFI_VIRTUAL_ADDRESS new_buffer = 0; // Make this 0x100000000 to only operate above 4GB

  new_buffer = VAllocateFreeAddressBy32Bytes(numbytes, new_buffer);

  return (void*)new_buffer;
}

__attribute__((malloc)) void * Vmalloc64(size_t numbytes)
{
  EFI_VIRTUAL_ADDRESS new_buffer = 0; // Make this 0x100000000 to only operate above 4GB

  new_buffer = VAllocateFreeAddressBy64Bytes(numbytes, new_buffer);

  return (void*)new_buffer;
}

__attribute__((malloc)) void * Vmalloc4k(size_t pages)
{
  EFI_VIRTUAL_ADDRESS new_buffer = 0; // Make this 0x100000000 to only operate above 4GB

  new_buffer = VAllocateFreeAddressByPage(pages, new_buffer);

  return (void*)new_buffer;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  VerifyZeroMem: Verify Memory Is Free
//----------------------------------------------------------------------------------------------------------------------------------
//
// Return 0 if desired section of memory is zeroed (for use in "if" statements)
//

uint8_t VerifyZeroMem(size_t NumBytes, uint64_t BaseAddr) // BaseAddr is a 64-bit unsigned int whose value is the memory address
{
  for(size_t i = 0; i < NumBytes; i++)
  {
    if(*(uint8_t*)(BaseAddr + i) != 0)
    {
      return 1;
    }
  }
  return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  print_system_memmap: The Ultimate Debugging Tool
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

// This array should match the EFI_MEMORY_TYPE enum in EfiTypes.h. If it doesn't, maybe the spec changed and this needs to be updated.
// This is a global variable, which lets it be declared static. This prevents a stack overflow that could arise if it were local to its function of use.
// Static arrays defined like this can actually be made very large.
static const char mem_types[19][27] = {
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
    "EfiMaxMemoryType          ",
    "malloc                    ", // EfiMaxMemoryType + 1
    "vmalloc                   ", // EfiMaxMemoryType + 2
    "MemMap                    "  // EfiMaxMemoryType + 3
};

void print_system_memmap(void)
{
  EFI_MEMORY_DESCRIPTOR * Piece;
  uint16_t line = 0;

  printf("MemMap %#qx, MemMapSize: %qu, MemMapDescriptorSize: %qu\r\n", Global_Memory_Info.MemMap, Global_Memory_Info.MemMapSize, Global_Memory_Info.MemMapDescriptorSize);

  // Multiply NumOfPages by EFI_PAGE_SIZE or do (NumOfPages << EFI_PAGE_SHIFT) to get the end address... which should just be the start of the next section.
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
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

//----------------------------------------------------------------------------------------------------------------------------------
//  Set_Identity_VMAP: Set Virtual Address Map to Identity Mapping
//----------------------------------------------------------------------------------------------------------------------------------
//
// Get the system memory map, parse it, identity map it, and set the virtual address map accordingly.
// Identity mapping means Physical Address == Virtual Address, also called a 1:1 (one-to-one) map
//

EFI_MEMORY_DESCRIPTOR * Set_Identity_VMAP(EFI_RUNTIME_SERVICES * RTServices)
{
  EFI_MEMORY_DESCRIPTOR * Piece;

  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    Piece->VirtualStart = Piece->PhysicalStart;
  }

  if(EFI_ERROR(RTServices->SetVirtualAddressMap(Global_Memory_Info.MemMapSize, Global_Memory_Info.MemMapDescriptorSize, EFI_MEMORY_DESCRIPTOR_VERSION, Global_Memory_Info.MemMap)))
  {
//    printf("Error setting VMAP. Returning NULL.\r\n");
    return NULL;
  }

  return Global_Memory_Info.MemMap;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  Setup_MemMap: Prepare the Memory Map for Use with Allocators
//----------------------------------------------------------------------------------------------------------------------------------
//
// Take UEFI's memory map and modify it to include the memory map's location. This prepares it for use with mallocX.
//

void Setup_MemMap(void)
{
  // Make a new memory map with the location of the map itself, which is needed to use malloc().
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_PHYSICAL_ADDRESS PhysicalEnd;
  size_t numpages = (Global_Memory_Info.MemMapSize + Global_Memory_Info.MemMapDescriptorSize) >> EFI_PAGE_SHIFT; // Need enough space to contain the map + one additional descriptor (for the map itself)
  if((Global_Memory_Info.MemMapSize + Global_Memory_Info.MemMapDescriptorSize) & EFI_PAGE_MASK)
  { // Need one more page to contain the whole map
    numpages++;
  }

  // Map's gettin' evicted, gotta relocate.
  EFI_PHYSICAL_ADDRESS new_MemMap_base_address = ActuallyFreeAddress(numpages, 0); // This will only give addresses at the base of a chunk of EfiConventionalMemory
  if(new_MemMap_base_address == -1)
  {
    printf("Can't move MemMap for enlargement: malloc not usable.\r\n");
  }
  else
  {
    EFI_MEMORY_DESCRIPTOR * new_MemMap = (EFI_MEMORY_DESCRIPTOR*)new_MemMap_base_address;
    // Zero out the new memmap destination
    AVX_memset(new_MemMap, 0, numpages << EFI_PAGE_SHIFT);

    // Move (copy) the map from MemMap to new_MemMap
    AVX_memmove(new_MemMap, Global_Memory_Info.MemMap, Global_Memory_Info.MemMapSize);
    // Zero out the old one
    AVX_memset(Global_Memory_Info.MemMap, 0, Global_Memory_Info.MemMapSize);

    // Update Global_Memory_Info with new values
    Global_Memory_Info.MemMap = new_MemMap;
    Global_Memory_Info.MemMapSize += Global_Memory_Info.MemMapDescriptorSize;

    // Get a piece pointer for the new location of the map (scan the map to find it)
    for(Piece = Global_Memory_Info.MemMap; (uint8_t*)Piece < ((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
    {
      if((Piece->Type == EfiConventionalMemory) && (Piece->NumberOfPages >= numpages)) // The new map's in EfiConventionalMemory
      {
        PhysicalEnd = Piece->PhysicalStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT); // Get the end of this range for bounds checking (this value might be the start address of the next range)

        if(
            ((uint8_t*)Global_Memory_Info.MemMap >= (uint8_t*)Piece->PhysicalStart)
            &&
            (((uint8_t*)Global_Memory_Info.MemMap + (numpages << EFI_PAGE_SHIFT)) <= (uint8_t*)PhysicalEnd)
          ) // Bounds check
        {
          break; // Found it, Piece holds the spot now. Also, we know it's at the start of Piece->PhysicalStart because we put it there with ActuallyFreeAddress.
        }
      }
    }
    if((uint8_t*)Piece == ((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
    {
      printf("MemMap not found.\r\n");
    }
    else
    {
      // Mark the new area as memmap
      if(Piece->NumberOfPages == numpages) // Trivial case: The new space descriptor is just the right size and needs no splitting; saves a memory descriptor
      {
        Piece->Type = EfiMaxMemoryType + 3; // Special memmap type
      }
      else // Need to insert a memmap descriptor. Thanks to the way ActuallyFreeAddress works we know the map's new area is at the base of the EfiConventionalMemory descriptor
      {
        // Make a temporary descriptor to hold current piece's values, but modified for memmap
        EFI_MEMORY_DESCRIPTOR new_descriptor_temp;
        new_descriptor_temp.Type = EfiMaxMemoryType + 3; // Special memmap type
        new_descriptor_temp.Pad = Piece->Pad;
        new_descriptor_temp.PhysicalStart = Piece->PhysicalStart;
        new_descriptor_temp.VirtualStart = Piece->VirtualStart;
        new_descriptor_temp.NumberOfPages = numpages;
        new_descriptor_temp.Attribute = Piece->Attribute;

        // Modify this descriptor (shrink it) to reflect its new values
        Piece->PhysicalStart += (numpages << EFI_PAGE_SHIFT);
        Piece->VirtualStart += (numpages << EFI_PAGE_SHIFT);
        Piece->NumberOfPages -= numpages;

        // Move (copy) the whole memmap that's above this piece (including this freshly modified piece) from this piece to one MemMapDescriptorSize over
        AVX_memmove((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize, Piece, ((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize) - (uint8_t*)Piece); // Pointer math to get size

        // Insert the new piece (by overwriting the now-duplicated entry with new values)
        // I.e. turn this piece into what was stored in the temporary descriptor above
        Piece->Type = new_descriptor_temp.Type;
        Piece->Pad = new_descriptor_temp.Pad;
        Piece->PhysicalStart = new_descriptor_temp.PhysicalStart;
        Piece->VirtualStart = new_descriptor_temp.VirtualStart;
        Piece->NumberOfPages = new_descriptor_temp.NumberOfPages;
        Piece->Attribute = new_descriptor_temp.Attribute;
      }
      // Done modifying new map.

      // TODO: Call mergecontiguousconventionalmemory here or something
      // Each allocate function should call an update memmap with address, size, memtype
      // Every free call should contain a call to mergecontiguousconventionalmemory
    }
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
//  ActuallyFreeAddress: Find A Free Physical Memory Address, Bottom-Up
//----------------------------------------------------------------------------------------------------------------------------------
//
// Returns the next EfiConventionalMemory area that is > the supplied OldAddress.
//
// pages: number of pages needed
// OldAddress: An baseline address to search bottom-up from
//

EFI_PHYSICAL_ADDRESS ActuallyFreeAddress(size_t pages, EFI_PHYSICAL_ADDRESS OldAddress)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the end address... which should just be the start of the next section.
  // Check for EfiConventionalMemory in the map
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    // Within each compatible EfiConventionalMemory, look for space
    if((Piece->Type == EfiConventionalMemory) && (Piece->NumberOfPages >= pages) && (Piece->PhysicalStart > OldAddress))
    {
      break;
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free physical addresses...\r\n");
#endif
    return -1;
  }

  return Piece->PhysicalStart;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  ActuallyFreeAddressByPage: Find A Free Physical Memory Address, Bottom-Up, The Hard Way
//----------------------------------------------------------------------------------------------------------------------------------
//
// Returns the next 4kB page address marked as available (EfiConventionalMemory) that is > the supplied OldAddress.
//
// pages: number of pages needed
// OldAddress: An baseline address to search bottom-up from
//

EFI_PHYSICAL_ADDRESS ActuallyFreeAddressByPage(size_t pages, EFI_PHYSICAL_ADDRESS OldAddress)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_PHYSICAL_ADDRESS PhysicalEnd;
  EFI_PHYSICAL_ADDRESS DiscoveredAddress;

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the end address... which should just be the start of the next section.
  // Check for EfiConventionalMemory in the map
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    // Within each compatible EfiConventionalMemory, look for space
    if((Piece->Type == EfiConventionalMemory) && (Piece->NumberOfPages >= pages))
    {
      PhysicalEnd = Piece->PhysicalStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT) - EFI_PAGE_MASK; // Get the end of this range, and use it to set a bound on the range (define a max returnable address)
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
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free physical addresses by 4kB page...\r\n");
#endif
    return -1;
  }

  return DiscoveredAddress;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  AllocateFreeAddressByPage: Allocate A Free Physical Memory Address, Bottom-Up, The Hard Way
//----------------------------------------------------------------------------------------------------------------------------------
//
// Returns the next 4kB page address marked as available (EfiConventionalMemory) that is > the supplied OldAddress.
//
// pages: number of pages needed
// OldAddress: An baseline address to search bottom-up from
//

EFI_PHYSICAL_ADDRESS AllocateFreeAddressByPage(size_t pages, EFI_PHYSICAL_ADDRESS OldAddress)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_PHYSICAL_ADDRESS PhysicalEnd;
  EFI_PHYSICAL_ADDRESS DiscoveredAddress;

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the end address... which should just be the start of the next section.
  // Check for EfiConventionalMemory in the map
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
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

        // TODO: this part
        break;
        // If PhysicalEnd == OldAddress, we need to go to the next EfiConventionalMemory range
      }
      else if(Piece->PhysicalStart > OldAddress) // Try a new range
      {
        // Found an address
        DiscoveredAddress = Piece->PhysicalStart;


        // Now make room for allocation
        // Check if we even need to make room...
        if(Piece->NumberOfPages - pages == 0)
        { // Nope. Just change this chunk to malloc type.
          Piece->Type = EfiMaxMemoryType + 1;
          // Done here.
        }
        else
        {
/*
          // Make a new descriptor, which contains an entry to insert
          EFI_MEMORY_DESCRIPTOR new_descriptor;
          new_descriptor.Type = EfiMaxMemoryType + 1;
          new_descriptor.Pad = 0;
          new_descriptor.PhysicalStart = Piece->PhysicalStart; // new descriptor goes here
          new_descriptor.VirtualStart = Piece->VirtualStart;
          new_descriptor.NumberOfPages = pages;
          new_descriptor.Attribute = Piece->Attribute; // This probably doesn't need to change (Free memory is 0xF, or UC | WC | WT | WB per EfiTypes.h)

          // Current piece type stays the same
          // Pad stays the same
          Piece->PhysicalStart += pages << EFI_PAGE_SHIFT; // Move start addresses
          Piece->VirtualStart += pages << EFI_PAGE_SHIFT;
          Piece->NumberOfPages = Piece->NumberOfPages - pages; // Update size
          // Attribute doesn't need to change

          // Now that the map has been modified, need space to make the new map
          size_t size_of_new_map = Global_Memory_Info.MemMapSize + Global_Memory_Info.MemMapDescriptorSize; // One for new descriptor
// TODO

          if((Global_Memory_Info.MemMapSize & EFI_PAGE_MASK) != 0)
          { // Residual data, need an extra page

          }
          else
          ActuallyFreeAddressByPage();

          //TODO: insert memory descriptor
          // Need to make a new map. What's awesome about this is that we have the memory map, so we can put it anywhere we want that has sufficient size.
          // We can thus appropriate some conventional memory for this new map. We just need some available memory of MemMapSize + Memory_Map_Descriptor_Size.
          // This we can find with a call to ActuallyFreeAddressByPage (the old version of this function)

// Possible idea: instead of one map, could have a main map and a "malloc map" -- No because this won't help for adresses plonked in the middle of a conventional memory area
// Since this big map can only do pages, the "malloc map" will have to be the zoomed-in view of the malloc area (I guess that makes it the heap-view?)

// AVX_memmove will be needed to split a memmap to insert the new_descriptor

            // Or should malloc entries be added to the end? No.
            */
        }



        break;
      }
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free physical addresses by 4kB page...\r\n");
#endif
    return -1;
  }

  return DiscoveredAddress;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  AllocateFreeAddressBy16Bytes: Allocate A Free Physical Memory Address, Bottom-Up, 16-Byte Aligned
//----------------------------------------------------------------------------------------------------------------------------------
//
// Returns the next 16-byte aligned address marked as available (EfiConventionalMemory) that is > the supplied OldAddress.
//
// numbytes: total number of bytes needed
// OldAddress: An baseline address to search bottom-up from
//

EFI_PHYSICAL_ADDRESS AllocateFreeAddressBy16Bytes(size_t numbytes, EFI_PHYSICAL_ADDRESS OldAddress)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_PHYSICAL_ADDRESS PhysicalEnd;
  EFI_PHYSICAL_ADDRESS DiscoveredAddress;
  size_t num_16_bytes = numbytes >> 4;
  if(numbytes & 0xF)
  { // This might "waste" up to 15 bytes, but alignment makes it worthwhile.
    num_16_bytes++;
  }

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the size of each piece... which should just give the start of the next section when added to PhysicalStart.
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    // Check for EfiConventionalMemory in the map, and if it's big enough
    if((Piece->Type == EfiConventionalMemory) && ((Piece->NumberOfPages << EFI_PAGE_SHIFT) >= num_16_bytes))
    { // Found a range big enough.
      // Get the end of this range for bounds checking
      PhysicalEnd = Piece->PhysicalStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT) - 1;
      // num_x_bytes * x gives total size to allocate for array
      if((OldAddress >= Piece->PhysicalStart) && ((OldAddress + (num_16_bytes << 4)) < PhysicalEnd)) // Bounds check on OldAddress
      {
        // OldAddress + offset is [still] in-bounds, so return the next available x-byte aligned address in the range.
        DiscoveredAddress = OldAddress + num_16_bytes; // Left shift num_x_bytes by 1 or 2 to check every 0x10 or 0x100 sets of bytes (must also modify the above PhysicalEnd bound check)
        break;
        // If we would run over PhysicalEnd, we need to go to the next EfiConventionalMemory range
      }
      else if(Piece->PhysicalStart > OldAddress) // Turns out the nearest compatible PhysicalStart is > OldAddress. Use that, then.
      {
        DiscoveredAddress = Piece->PhysicalStart;
        break;
      }
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free physical addresses by 16 bytes...\r\n");
#endif
    return -1;
  }

  return DiscoveredAddress;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  AllocateFreeAddressBy32Bytes: Allocate A Free Physical Memory Address, Bottom-Up, 32-Byte Aligned
//----------------------------------------------------------------------------------------------------------------------------------
//
// Returns the next 32-byte aligned address marked as available (EfiConventionalMemory) that is > the supplied OldAddress.
//
// numbytes: total number of bytes needed
// OldAddress: An baseline address to search bottom-up from
//

EFI_PHYSICAL_ADDRESS AllocateFreeAddressBy32Bytes(size_t numbytes, EFI_PHYSICAL_ADDRESS OldAddress)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_PHYSICAL_ADDRESS PhysicalEnd;
  EFI_PHYSICAL_ADDRESS DiscoveredAddress;
  size_t num_32_bytes = numbytes >> 5;
  if(numbytes & 0x1F)
  { // This might "waste" up to 31 bytes, but alignment makes it worthwhile.
    num_32_bytes++;
  }

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the size of each piece... which should just give the start of the next section when added to PhysicalStart.
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    // Check for EfiConventionalMemory in the map, and if it's big enough
    if((Piece->Type == EfiConventionalMemory) && ((Piece->NumberOfPages << EFI_PAGE_SHIFT) >= num_32_bytes))
    { // Found a range big enough.
      // Get the end of this range for bounds checking
      PhysicalEnd = Piece->PhysicalStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT) - 1;
      // num_x_bytes * x gives total size to allocate for array
      if((OldAddress >= Piece->PhysicalStart) && ((OldAddress + (num_32_bytes << 5)) < PhysicalEnd)) // Bounds check on OldAddress
      {
        // OldAddress + offset is [still] in-bounds, so return the next available x-byte aligned address in the range.
        DiscoveredAddress = OldAddress + num_32_bytes; // Left shift num_x_bytes by 1 or 2 to check every 0x10 or 0x100 sets of bytes (must also modify the above PhysicalEnd bound check)
        break;
        // If we would run over PhysicalEnd, we need to go to the next EfiConventionalMemory range
      }
      else if(Piece->PhysicalStart > OldAddress) // Turns out the nearest compatible PhysicalStart is > OldAddress. Use that, then.
      {
        DiscoveredAddress = Piece->PhysicalStart;
        break;
      }
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free physical addresses by 32 bytes...\r\n");
#endif
    return -1;
  }

  return DiscoveredAddress;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  AllocateFreeAddressBy64Bytes: Allocate A Free Physical Memory Address, Bottom-Up, 64-Byte Aligned
//----------------------------------------------------------------------------------------------------------------------------------
//
// Returns the next 64-byte aligned address marked as available (EfiConventionalMemory) that is > the supplied OldAddress.
//
// numbytes: total number of bytes needed
// OldAddress: An baseline address to search bottom-up from
//

EFI_PHYSICAL_ADDRESS AllocateFreeAddressBy64Bytes(size_t numbytes, EFI_PHYSICAL_ADDRESS OldAddress)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_PHYSICAL_ADDRESS PhysicalEnd;
  EFI_PHYSICAL_ADDRESS DiscoveredAddress;
  size_t num_64_bytes = numbytes >> 6;
  if(numbytes & 0x3F)
  { // This might "waste" up to 63 bytes, but alignment makes it worthwhile.
    num_64_bytes++;
  }

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the size of each piece... which should just give the start of the next section when added to PhysicalStart.
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    // Check for EfiConventionalMemory in the map, and if it's big enough
    if((Piece->Type == EfiConventionalMemory) && ((Piece->NumberOfPages << EFI_PAGE_SHIFT) >= num_64_bytes))
    { // Found a range big enough.
      // Get the end of this range for bounds checking
      PhysicalEnd = Piece->PhysicalStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT) - 1;
      // num_x_bytes * x gives total size to allocate for array
      if((OldAddress >= Piece->PhysicalStart) && ((OldAddress + (num_64_bytes << 6)) < PhysicalEnd)) // Bounds check on OldAddress
      {
        // OldAddress + offset is [still] in-bounds, so return the next available x-byte aligned address in the range.
        DiscoveredAddress = OldAddress + num_64_bytes; // Left shift num_x_bytes by 1 or 2 to check every 0x10 or 0x100 sets of bytes (must also modify the above PhysicalEnd bound check)
        break;
        // If we would run over PhysicalEnd, we need to go to the next EfiConventionalMemory range
      }
      else if(Piece->PhysicalStart > OldAddress) // Turns out the nearest compatible PhysicalStart is > OldAddress. Use that, then.
      {
        DiscoveredAddress = Piece->PhysicalStart;
        break;
      }
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free physical addresses by 64 bytes...\r\n");
#endif
    return -1;
  }

  return DiscoveredAddress;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  VActuallyFreeAddress: Find A Free Virtual Memory Address, Bottom-Up
//----------------------------------------------------------------------------------------------------------------------------------
//
// Returns the next EfiConventionalMemory area that is > the supplied OldAddress.
//
// pages: number of pages needed
// OldAddress: An baseline address to search bottom-up from
//

EFI_VIRTUAL_ADDRESS VActuallyFreeAddress(size_t pages, EFI_VIRTUAL_ADDRESS OldAddress)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the end address... which should just be the start of the next section.
  // Check for EfiConventionalMemory in the map
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    // Within each compatible EfiConventionalMemory, look for space
    if((Piece->Type == EfiConventionalMemory) && (Piece->NumberOfPages >= pages) && (Piece->VirtualStart > OldAddress))
    {
      break;
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free virtual addresses...\r\n");
#endif
    return -1;
  }

  return Piece->VirtualStart;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  VActuallyFreeAddressByPage: Find A Free Virtual Memory Address, Bottom-Up, The Hard Way
//----------------------------------------------------------------------------------------------------------------------------------
//
// Returns the next 4kB page address marked as available (EfiConventionalMemory) that is > the supplied OldAddress.
//
// pages: number of pages needed
// OldAddress: An baseline address to search bottom-up from
//

EFI_VIRTUAL_ADDRESS VActuallyFreeAddressByPage(size_t pages, EFI_VIRTUAL_ADDRESS OldAddress)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_VIRTUAL_ADDRESS VirtualEnd;
  EFI_VIRTUAL_ADDRESS DiscoveredAddress;

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the end address... which should just be the start of the next section.
  // Check for EfiConventionalMemory in the map
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    // Within each compatible EfiConventionalMemory, look for space
    if((Piece->Type == EfiConventionalMemory) && (Piece->NumberOfPages >= pages))
    {
      VirtualEnd = Piece->VirtualStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT) - 1; // Get the end of this range
      // (pages*EFI_PAGE_SIZE) or (pages << EFI_PAGE_SHIFT) gives the size the kernel would take up in memory
      if((OldAddress >= Piece->VirtualStart) && ((OldAddress + (pages << EFI_PAGE_SHIFT)) < VirtualEnd)) // Bounds check on OldAddress
      {
        // Return the next available page's address in the range. We need to go page-by-page for the really buggy systems.
        DiscoveredAddress = OldAddress + EFI_PAGE_SIZE; // Left shift EFI_PAGE_SIZE by 1 or 2 to check every 0x10 or 0x100 pages (must also modify the above VirtualEnd bound check)
        break;
        // If VirtualEnd == OldAddress, we need to go to the next EfiConventionalMemory range
      }
      else if(Piece->VirtualStart > OldAddress) // Try a new range
      {
        DiscoveredAddress = Piece->VirtualStart;
        break;
      }
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free virtual addresses by 4kB page...\r\n");
#endif
    return -1;
  }

  return DiscoveredAddress;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  VAllocateFreeAddressByPage: Allocate A Free Virtual Memory Address, Bottom-Up, The Hard Way
//----------------------------------------------------------------------------------------------------------------------------------
//
// Returns the next 4kB page address marked as available (EfiConventionalMemory) that is > the supplied OldAddress.
//
// pages: number of pages needed
// OldAddress: An baseline address to search bottom-up from
//

EFI_VIRTUAL_ADDRESS VAllocateFreeAddressByPage(size_t pages, EFI_VIRTUAL_ADDRESS OldAddress)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_VIRTUAL_ADDRESS VirtualEnd;
  EFI_VIRTUAL_ADDRESS DiscoveredAddress;

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the end address... which should just be the start of the next section.
  // Check for EfiConventionalMemory in the map
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    // Within each compatible EfiConventionalMemory, look for space
    if((Piece->Type == EfiConventionalMemory) && (Piece->NumberOfPages >= pages))
    {
      VirtualEnd = Piece->VirtualStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT) - 1; // Get the end of this range
      // (pages*EFI_PAGE_SIZE) or (pages << EFI_PAGE_SHIFT) gives the size the kernel would take up in memory
      if((OldAddress >= Piece->VirtualStart) && ((OldAddress + (pages << EFI_PAGE_SHIFT)) < VirtualEnd)) // Bounds check on OldAddress
      {
        // Return the next available page's address in the range. We need to go page-by-page for the really buggy systems.
        DiscoveredAddress = OldAddress + EFI_PAGE_SIZE; // Left shift EFI_PAGE_SIZE by 1 or 2 to check every 0x10 or 0x100 pages (must also modify the above VirtualEnd bound check)
        break;
        // If VirtualEnd == OldAddress, we need to go to the next EfiConventionalMemory range
      }
      else if(Piece->VirtualStart > OldAddress) // Try a new range
      {
        DiscoveredAddress = Piece->VirtualStart;
        break;
      }
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free virtual addresses by 4kB page...\r\n");
#endif
    return -1;
  }

  return DiscoveredAddress;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  VAllocateFreeAddressBy16Bytes: Allocate A Free Virtual Memory Address, Bottom-Up, 16-Byte Aligned
//----------------------------------------------------------------------------------------------------------------------------------
//
// Returns the next 16-byte aligned address marked as available (EfiConventionalMemory) that is > the supplied OldAddress.
//
// numbytes: total number of bytes needed
// OldAddress: An baseline address to search bottom-up from
//

EFI_VIRTUAL_ADDRESS VAllocateFreeAddressBy16Bytes(size_t numbytes, EFI_VIRTUAL_ADDRESS OldAddress)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_VIRTUAL_ADDRESS VirtualEnd;
  EFI_VIRTUAL_ADDRESS DiscoveredAddress;
  size_t num_16_bytes = numbytes >> 4;
  if(numbytes & 0xF)
  { // This might "waste" up to 15 bytes, but alignment makes it worthwhile.
    num_16_bytes++;
  }

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the size of each piece... which should just give the start of the next section when added to VirtualStart.
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    // Check for EfiConventionalMemory in the map, and if it's big enough
    if((Piece->Type == EfiConventionalMemory) && ((Piece->NumberOfPages << EFI_PAGE_SHIFT) >= num_16_bytes))
    { // Found a range big enough.
      // Get the end of this range for bounds checking
      VirtualEnd = Piece->VirtualStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT) - 1;
      // num_x_bytes * x gives total size to allocate for array
      if((OldAddress >= Piece->VirtualStart) && ((OldAddress + (num_16_bytes << 4)) < VirtualEnd)) // Bounds check on OldAddress
      {
        // OldAddress + offset is [still] in-bounds, so return the next available x-byte aligned address in the range.
        DiscoveredAddress = OldAddress + num_16_bytes; // Left shift num_x_bytes by 1 or 2 to check every 0x10 or 0x100 sets of bytes (must also modify the above VirtualEnd bound check)
        break;
        // If we would run over VirtualEnd, we need to go to the next EfiConventionalMemory range
      }
      else if(Piece->VirtualStart > OldAddress) // Turns out the nearest compatible VirtualStart is > OldAddress. Use that, then.
      {
        DiscoveredAddress = Piece->VirtualStart;
        break;
      }
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free virtual addresses by 16 bytes...\r\n");
#endif
    return -1;
  }

  return DiscoveredAddress;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  VAllocateFreeAddressBy32Bytes: Allocate A Free Virtual Memory Address, Bottom-Up, 32-Byte Aligned
//----------------------------------------------------------------------------------------------------------------------------------
//
// Returns the next 32-byte aligned address marked as available (EfiConventionalMemory) that is > the supplied OldAddress.
//
// numbytes: total number of bytes needed
// OldAddress: An baseline address to search bottom-up from
//

EFI_VIRTUAL_ADDRESS VAllocateFreeAddressBy32Bytes(size_t numbytes, EFI_VIRTUAL_ADDRESS OldAddress)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_VIRTUAL_ADDRESS VirtualEnd;
  EFI_VIRTUAL_ADDRESS DiscoveredAddress;
  size_t num_32_bytes = numbytes >> 5;
  if(numbytes & 0x1F)
  { // This might "waste" up to 31 bytes, but alignment makes it worthwhile.
    num_32_bytes++;
  }

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the size of each piece... which should just give the start of the next section when added to VirtualStart.
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    // Check for EfiConventionalMemory in the map, and if it's big enough
    if((Piece->Type == EfiConventionalMemory) && ((Piece->NumberOfPages << EFI_PAGE_SHIFT) >= num_32_bytes))
    { // Found a range big enough.
      // Get the end of this range for bounds checking
      VirtualEnd = Piece->VirtualStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT) - 1;
      // num_x_bytes * x gives total size to allocate for array
      if((OldAddress >= Piece->VirtualStart) && ((OldAddress + (num_32_bytes << 5)) < VirtualEnd)) // Bounds check on OldAddress
      {
        // OldAddress + offset is [still] in-bounds, so return the next available x-byte aligned address in the range.
        DiscoveredAddress = OldAddress + num_32_bytes; // Left shift num_x_bytes by 1 or 2 to check every 0x10 or 0x100 sets of bytes (must also modify the above VirtualEnd bound check)
        break;
        // If we would run over VirtualEnd, we need to go to the next EfiConventionalMemory range
      }
      else if(Piece->VirtualStart > OldAddress) // Turns out the nearest compatible VirtualStart is > OldAddress. Use that, then.
      {
        DiscoveredAddress = Piece->VirtualStart;
        break;
      }
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free virtual addresses by 32 bytes...\r\n");
#endif
    return -1;
  }

  return DiscoveredAddress;
}

//----------------------------------------------------------------------------------------------------------------------------------
//  VAllocateFreeAddressBy64Bytes: Allocate A Free Virtual Memory Address, Bottom-Up, 64-Byte Aligned
//----------------------------------------------------------------------------------------------------------------------------------
//
// Returns the next 64-byte aligned address marked as available (EfiConventionalMemory) that is > the supplied OldAddress.
//
// numbytes: total number of bytes needed
// OldAddress: An baseline address to search bottom-up from
//

EFI_VIRTUAL_ADDRESS VAllocateFreeAddressBy64Bytes(size_t numbytes, EFI_VIRTUAL_ADDRESS OldAddress)
{
//  EFI_STATUS memmap_status;
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_VIRTUAL_ADDRESS VirtualEnd;
  EFI_VIRTUAL_ADDRESS DiscoveredAddress;
  size_t num_64_bytes = numbytes >> 6;
  if(numbytes & 0x3F)
  { // This might "waste" up to 63 bytes, but alignment makes it worthwhile.
    num_64_bytes++;
  }

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the size of each piece... which should just give the start of the next section when added to VirtualStart.
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    // Check for EfiConventionalMemory in the map, and if it's big enough
    if((Piece->Type == EfiConventionalMemory) && ((Piece->NumberOfPages << EFI_PAGE_SHIFT) >= num_64_bytes))
    { // Found a range big enough.
      // Get the end of this range for bounds checking
      VirtualEnd = Piece->VirtualStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT) - 1;
      // num_x_bytes * x gives total size to allocate for array
      if((OldAddress >= Piece->VirtualStart) && ((OldAddress + (num_64_bytes << 6)) < VirtualEnd)) // Bounds check on OldAddress
      {
        // OldAddress + offset is [still] in-bounds, so return the next available x-byte aligned address in the range.
        DiscoveredAddress = OldAddress + num_64_bytes; // Left shift num_x_bytes by 1 or 2 to check every 0x10 or 0x100 sets of bytes (must also modify the above VirtualEnd bound check)
        break;
        // If we would run over VirtualEnd, we need to go to the next EfiConventionalMemory range
      }
      else if(Piece->VirtualStart > OldAddress) // Turns out the nearest compatible VirtualStart is > OldAddress. Use that, then.
      {
        DiscoveredAddress = Piece->VirtualStart;
        break;
      }
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
#ifdef MEMORY_CHECK_INFO
    printf("No more free virtual addresses by 64 bytes...\r\n");
#endif
    return -1;
  }

  return DiscoveredAddress;
}
/*
//----------------------------------------------------------------------------------------------------------------------------------
//  AllocateMemoryPages: Allocate Pages at a Physical Address
//----------------------------------------------------------------------------------------------------------------------------------
//
// This function modifies the memory map to indicate these pages are now used.
//

void AllocateMemoryPages(EFI_PHYSICAL_ADDRESS address, size_t pages)
{

}

//----------------------------------------------------------------------------------------------------------------------------------
//  AllocateMemory: Allocate Bytes at a Physical Address
//----------------------------------------------------------------------------------------------------------------------------------
//
// This function modifies the memory map to indicate these bytes are now used.
//

void AllocateMemory(EFI_PHYSICAL_ADDRESS address, size_t numbytes)
{

}

//----------------------------------------------------------------------------------------------------------------------------------
//  VAllocateMemoryPages: Allocate Pages at a Virtual Address
//----------------------------------------------------------------------------------------------------------------------------------
//
// This function modifies the memory map to indicate these pages are now used.
//

void VAllocateMemoryPages(EFI_VIRTUAL_ADDRESS address, size_t pages)
{

}

//----------------------------------------------------------------------------------------------------------------------------------
//  VAllocateMemory: Allocate Bytes at a Virtual Address
//----------------------------------------------------------------------------------------------------------------------------------
//
// This function modifies the memory map to indicate these bytes are now used.
//

void VAllocateMemory(EFI_VIRTUAL_ADDRESS address, size_t numbytes)
{

}
*/

/*
// Old void Setup_MemMap(void)
{
  // Make a new memory map with the location of the map itself, which is needed to use malloc().
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_PHYSICAL_ADDRESS PhysicalEnd;
  size_t numpages = Global_Memory_Info.MemMapSize >> EFI_PAGE_SHIFT;
  if(Global_Memory_Info.MemMapSize & EFI_PAGE_MASK)
  { // Need one more page to contain the whole map
    numpages++;
  }

  // We know the map is at Global_Memory_Info.MemMap.
  // Iterate through the memory map to find what descriptor contains it. It'll be in a section of type EfiLoaderData (or whatever the bootloader allocated for it before ExitBootServices())
  for(Piece = Global_Memory_Info.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize))
  {
    if((Piece->Type == EfiLoaderData) && (Piece->NumberOfPages >= numpages)) // EfiLoaderData here would need to be changed if the bootloader ever allocates something else for it
    {
      PhysicalEnd = Piece->PhysicalStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT); // Get the end of this range for bounds checking (this value might be the start address of the next range)

      if(
          ((uint8_t*)Global_Memory_Info.MemMap >= (uint8_t*)Piece->PhysicalStart)
          &&
          (((uint8_t*)Global_Memory_Info.MemMap + (numpages << EFI_PAGE_SHIFT)) <= (uint8_t*)PhysicalEnd)
        ) // Bounds check
      {
        // Found it
        // If we need to add any entries, the whole map's gotta move unless there's space in the last 4kB page.
        if(Piece->NumberOfPages == numpages) // Check trivial case (this lone section is actually the whole map)
        {
          Piece->Type = EfiMaxMemoryType + 3; // Special memmap type
          // Done here.
        }
        else if((uint8_t*)Global_Memory_Info.MemMap == (uint8_t*)Piece->PhysicalStart) // Memmap is on the start of the descriptor, but it's not the whole descriptor, so the map needs at least one more descriptor
        { // Modify 1 entry and add 1 entry
          if( ((numpages << EFI_PAGE_SHIFT) - Global_Memory_Info.MemMapSize) >= Global_Memory_Info.MemMapDescriptorSize)
          { // Alright, there's enough space in this page for another descriptor

            // Make a temporary descriptor to hold current piece's values, but modified for memmap
            EFI_MEMORY_DESCRIPTOR new_descriptor_temp;
            new_descriptor_temp.Type = EfiMaxMemoryType + 3; // Special memmap type
            new_descriptor_temp.Pad = Piece->Pad;
            new_descriptor_temp.PhysicalStart = Piece->PhysicalStart;
            new_descriptor_temp.VirtualStart = Piece->VirtualStart;
            new_descriptor_temp.NumberOfPages = numpages;
            new_descriptor_temp.Attribute = Piece->Attribute;

            // Modify this descriptor (shrink it) to reflect its new values
            Piece->PhysicalStart += (numpages << EFI_PAGE_SHIFT);
            Piece->VirtualStart += (numpages << EFI_PAGE_SHIFT);
            Piece->NumberOfPages -= numpages;

            // Move (copy) the whole memmap that's above this piece (including this freshly modified piece) from this piece to one MemMapDescriptorSize over
            AVX_memmove((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize, Piece, ((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize) - (uint8_t*)Piece); // Pointer math to get size

            // Insert the new piece (by overwriting the now-duplicated entry with new values)
            // I.e. turn this piece into what was stored in the temporary descriptor above
            Piece->Type = new_descriptor_temp.Type;
            Piece->Pad = new_descriptor_temp.Pad;
            Piece->PhysicalStart = new_descriptor_temp.PhysicalStart;
            Piece->VirtualStart = new_descriptor_temp.VirtualStart;
            Piece->NumberOfPages = new_descriptor_temp.NumberOfPages;
            Piece->Attribute = new_descriptor_temp.Attribute;

            // Update Global_Memory_Info
            Global_Memory_Info.MemMapSize += Global_Memory_Info.MemMapDescriptorSize; // Only need to update total size here

            // All done.
          }
          else
          { // Need to move the map somewhere with more pages available. Don't want to play any fragmentation games with it.
            // This means up to 2 new descriptors may be needed.

            // First, find a suitable location for the new, bigger map
            size_t additional_pages = 0;
            if((Global_Memory_Info.MemMapDescriptorSize << 1) <= EFI_PAGE_SIZE) // We need enough space to add 2 descriptors (new memmap and clear the old one)
            {
              additional_pages++; // Add 1
            }
            else // This may never be necessary, but if it does become needed one day (huge descriptors or something), support is already here.
            {
              additional_pages = (Global_Memory_Info.MemMapDescriptorSize << 1) >> EFI_PAGE_SHIFT;
              if((Global_Memory_Info.MemMapDescriptorSize << 1) & EFI_PAGE_MASK)
              {
                additional_pages++;
              }
            }
            size_t new_numpages = numpages + additional_pages;

            // Map's gettin' evicted, gotta relocate.
            EFI_PHYSICAL_ADDRESS new_MemMap_base_address = ActuallyFreeAddress(new_numpages, 0); // This will only give addresses at the base of a chunk of EfiConventionalMemory
            if(new_MemMap_base_address == -1)
            {
              printf("Can't move memmap for enlargement: malloc not usable.\r\n");
              break;
            }

            // Is this method (move whole memmap then modify it) better than moving it in pieces, inserting new descriptors between each piece?

            EFI_MEMORY_DESCRIPTOR * new_MemMap = (EFI_MEMORY_DESCRIPTOR*)new_MemMap_base_address;
            EFI_MEMORY_DESCRIPTOR * old_Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)new_MemMap + ((uint8_t*)Piece - (uint8_t*)Global_Memory_Info.MemMap)); // This is the descriptor where MemMap used to be; old_Piece contains it for use with the new map
            EFI_MEMORY_DESCRIPTOR * new_Piece; // We could re-use the Piece pointer now, but I think this is clearer.

            // Zero out the new memmap destination
            AVX_memset(new_MemMap, 0, new_numpages << EFI_PAGE_SHIFT);

            // Move (copy) the map from MemMap to new_MemMap
            AVX_memmove(new_MemMap, Global_Memory_Info.MemMap, Global_Memory_Info.MemMapSize);
            // Zero out the old one
            AVX_memset(Global_Memory_Info.MemMap, 0, Global_Memory_Info.MemMapSize); // We know MemMap should never overlap anytihing else, especially since we're moving (copying) it from EfiLoaderData to EfiConventionalMemory

            // Get a new piece pointer for the new location of the map (scan the map to find it)
            for(new_Piece = new_MemMap; new_Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)new_MemMap + Global_Memory_Info.MemMapSize); new_Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)new_Piece + Global_Memory_Info.MemMapDescriptorSize))
            {
              if((new_Piece->Type == EfiConventionalMemory) && (new_Piece->NumberOfPages >= new_numpages)) // The new map's in EfiConventionalMemory
              {
                PhysicalEnd = new_Piece->PhysicalStart + (new_Piece->NumberOfPages << EFI_PAGE_SHIFT); // Get the end of this range for bounds checking (this value might be the start address of the next range)

                if(
                    ((uint8_t*)new_MemMap >= (uint8_t*)new_Piece->PhysicalStart)
                    &&
                    (((uint8_t*)new_MemMap + (new_numpages << EFI_PAGE_SHIFT)) <= (uint8_t*)PhysicalEnd)
                  ) // Bounds check
                {
                  break; // Found it, new_Piece holds the spot now. Also, we know it's at the start of new_Piece->PhysicalStart because we put it there with ActuallyFreeAddress.
                }
              }
            }

            // Mark the new area as memmap similarly to the above process, but using new_Piece instead of Piece, and new_MemMap instead of Global_Memory_Info.MemMap
            if(new_Piece->NumberOfPages == new_numpages) // Trivial case: The new space descriptor is just the right size and needs no splitting; saves a memory descriptor
            {
              new_Piece->Type = EfiMaxMemoryType + 3; // Special memmap type
            }
            else // Need to insert a memmap descriptor. Thanks to the way ActuallyFreeAddress works we know the map's new area is at the base of the EfiConventionalMemory descriptor
            {
              // Make a temporary descriptor to hold current piece's values, but modified for memmap
              EFI_MEMORY_DESCRIPTOR new_descriptor_temp;
              new_descriptor_temp.Type = EfiMaxMemoryType + 3; // Special memmap type
              new_descriptor_temp.Pad = new_Piece->Pad;
              new_descriptor_temp.PhysicalStart = new_Piece->PhysicalStart;
              new_descriptor_temp.VirtualStart = new_Piece->VirtualStart;
              new_descriptor_temp.NumberOfPages = new_numpages;
              new_descriptor_temp.Attribute = new_Piece->Attribute;

              // Modify this descriptor (shrink it) to reflect its new values
              new_Piece->PhysicalStart += (new_numpages << EFI_PAGE_SHIFT);
              new_Piece->VirtualStart += (new_numpages << EFI_PAGE_SHIFT);
              new_Piece->NumberOfPages -= new_numpages;

              // Move (copy) the whole memmap that's above this piece (including this freshly modified piece) from this piece to one MemMapDescriptorSize over
              AVX_memmove((uint8_t*)new_Piece + Global_Memory_Info.MemMapDescriptorSize, new_Piece, ((uint8_t*)new_MemMap + Global_Memory_Info.MemMapSize) - (uint8_t*)new_Piece); // Pointer math to get size

              // Insert the new piece (by overwriting the now-duplicated entry with new values)
              // I.e. turn this piece into what was stored in the temporary descriptor above
              new_Piece->Type = new_descriptor_temp.Type;
              new_Piece->Pad = new_descriptor_temp.Pad;
              new_Piece->PhysicalStart = new_descriptor_temp.PhysicalStart;
              new_Piece->VirtualStart = new_descriptor_temp.VirtualStart;
              new_Piece->NumberOfPages = new_descriptor_temp.NumberOfPages;
              new_Piece->Attribute = new_descriptor_temp.Attribute;
            }
            // Done modifying new map.
            // Clean up the old memmap area (mark it as EfiConventionalMemory) using old_Piece

            // Since old memmap is known not to be the entirety of the EfiLoaderData descriptor (though it is at the base address), we need to add 1 new EfiConventionalMemory entry
            // Make a temporary descriptor to hold current piece's values, but modified for EfiConventionalMemory
            EFI_MEMORY_DESCRIPTOR new_descriptor_temp;
            new_descriptor_temp.Type = EfiConventionalMemory;
            new_descriptor_temp.Pad = old_Piece->Pad;
            new_descriptor_temp.PhysicalStart = old_Piece->PhysicalStart;
            new_descriptor_temp.VirtualStart = old_Piece->VirtualStart;
            new_descriptor_temp.NumberOfPages = numpages;
            new_descriptor_temp.Attribute = old_Piece->Attribute;

            // Modify this descriptor (shrink it) to reflect its new values
            old_Piece->PhysicalStart += (numpages << EFI_PAGE_SHIFT);
            old_Piece->VirtualStart += (numpages << EFI_PAGE_SHIFT);
            old_Piece->NumberOfPages -= numpages;

            // Move (copy) the whole memmap that's above this piece (including this freshly modified piece) from this piece to one MemMapDescriptorSize over
            AVX_memmove((uint8_t*)old_Piece + Global_Memory_Info.MemMapDescriptorSize, old_Piece, ((uint8_t*)new_MemMap + Global_Memory_Info.MemMapSize) - (uint8_t*)old_Piece); // Pointer math to get size

            // Insert the new piece (by overwriting the now-duplicated entry with new values)
            // I.e. turn this piece into what was stored in the temporary descriptor above
            old_Piece->Type = new_descriptor_temp.Type;
            old_Piece->Pad = new_descriptor_temp.Pad;
            old_Piece->PhysicalStart = new_descriptor_temp.PhysicalStart;
            old_Piece->VirtualStart = new_descriptor_temp.VirtualStart;
            old_Piece->NumberOfPages = new_descriptor_temp.NumberOfPages;
            old_Piece->Attribute = new_descriptor_temp.Attribute;

            // Update Global_Memory_Info
            Global_Memory_Info.MemMapSize += Global_Memory_Info.MemMapDescriptorSize;
            Global_Memory_Info.MemMap = new_MemMap;

            // Done. Wow.
          }
          // Done with this case (memmap is on the start of the EfiLoaderData section)
        }
        else if(((uint8_t*)Global_Memory_Info.MemMap + (numpages << EFI_PAGE_SHIFT)) == (uint8_t*)PhysicalEnd) // Memmap is at the end of the piece
        { // Modify 1 entry and add 1 entry
          if( ((numpages << EFI_PAGE_SHIFT) - Global_Memory_Info.MemMapSize) >= Global_Memory_Info.MemMapDescriptorSize)
          { // Alright, there's enough space in this page for another descriptor

            // Make a temporary descriptor to hold current piece's values, but size shrunken by size of memmap
            EFI_MEMORY_DESCRIPTOR new_descriptor_temp;
            new_descriptor_temp.Type = Piece->Type;
            new_descriptor_temp.Pad = Piece->Pad;
            new_descriptor_temp.PhysicalStart = Piece->PhysicalStart;
            new_descriptor_temp.VirtualStart = Piece->VirtualStart;
            new_descriptor_temp.NumberOfPages = Piece->NumberOfPages - numpages;
            new_descriptor_temp.Attribute = Piece->Attribute;

            // Modify this descriptor to reflect its new values (become the new memmap entry)
            Piece->Type = EfiMaxMemoryType + 3; // Special memmap type
            // Nothing for pad
            Piece->PhysicalStart += (numpages << EFI_PAGE_SHIFT);
            Piece->VirtualStart += (numpages << EFI_PAGE_SHIFT);
            Piece->NumberOfPages = numpages;
            // Nothing for attribute

            // Move (copy) the whole memmap that's above this piece (including this freshly modified piece) from this piece to one MemMapDescriptorSize over
            AVX_memmove((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize, Piece, ((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize) - (uint8_t*)Piece); // Pointer math to get size

            // Insert the "new" piece (by overwriting the now-duplicated memmap entry with old values)
            // I.e. turn this piece into what was stored in the temporary descriptor above
            Piece->Type = new_descriptor_temp.Type;
            Piece->Pad = new_descriptor_temp.Pad;
            Piece->PhysicalStart = new_descriptor_temp.PhysicalStart;
            Piece->VirtualStart = new_descriptor_temp.VirtualStart;
            Piece->NumberOfPages = new_descriptor_temp.NumberOfPages;
            Piece->Attribute = new_descriptor_temp.Attribute;

            // Update Global_Memory_Info
            Global_Memory_Info.MemMapSize += Global_Memory_Info.MemMapDescriptorSize; // Only need to update total size here

            // All done.
          }
          else // Not enough space left in the page
          { // Need to move the map somewhere with more pages available. Don't want to play any fragmentation games with it.

            // Move the whole map like "base of EfiLoaderData" case
            // This is the exact same process...

            size_t additional_pages = 0;
            if((Global_Memory_Info.MemMapDescriptorSize << 1) <= EFI_PAGE_SIZE) // We need enough space to add 2 descriptors (new memmap and clear the old one)
            {
              additional_pages++; // Add 1
            }
            else // This may never be necessary, but if it does become needed one day (huge descriptors or something), support is already here.
            {
              additional_pages = (Global_Memory_Info.MemMapDescriptorSize << 1) >> EFI_PAGE_SHIFT;
              if((Global_Memory_Info.MemMapDescriptorSize << 1) & EFI_PAGE_MASK)
              {
                additional_pages++;
              }
            }
            size_t new_numpages = numpages + additional_pages;

            EFI_PHYSICAL_ADDRESS new_MemMap_base_address = ActuallyFreeAddress(new_numpages, 0); // This will only give addresses at the base of a chunk of EfiConventionalMemory
            if(new_MemMap_base_address == -1)
            {
              printf("Can't move memmap for enlargement: malloc not usable.\r\n");
              break;
            }

            // Is this method (move whole memmap then modify it) better than moving it in pieces, inserting new descriptors between each piece?

            EFI_MEMORY_DESCRIPTOR * new_MemMap = (EFI_MEMORY_DESCRIPTOR*)new_MemMap_base_address;
            EFI_MEMORY_DESCRIPTOR * old_Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)new_MemMap + ((uint8_t*)Piece - (uint8_t*)Global_Memory_Info.MemMap)); // This is the descriptor where MemMap used to be; old_Piece contains it for use with the new map
            EFI_MEMORY_DESCRIPTOR * new_Piece; // We could re-use the Piece pointer now, but I think this is clearer.

            // Zero out the new memmap destination
            AVX_memset(new_MemMap, 0, new_numpages << EFI_PAGE_SHIFT);

            // Move (copy) the map from MemMap to new_MemMap
            AVX_memmove(new_MemMap, Global_Memory_Info.MemMap, Global_Memory_Info.MemMapSize);
            // Zero out the old one
            AVX_memset(Global_Memory_Info.MemMap, 0, Global_Memory_Info.MemMapSize); // We know MemMap should never overlap anytihing else, especially since we're moving (copying) it from EfiLoaderData to EfiConventionalMemory

            // Get a new piece pointer for the new location of the map (scan the map to find it)
            for(new_Piece = new_MemMap; new_Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)new_MemMap + Global_Memory_Info.MemMapSize); new_Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)new_Piece + Global_Memory_Info.MemMapDescriptorSize))
            {
              if((new_Piece->Type == EfiConventionalMemory) && (new_Piece->NumberOfPages >= new_numpages)) // The new map's in EfiConventionalMemory
              {
                PhysicalEnd = new_Piece->PhysicalStart + (new_Piece->NumberOfPages << EFI_PAGE_SHIFT); // Get the end of this range for bounds checking (this value might be the start address of the next range)

                if(
                    ((uint8_t*)new_MemMap >= (uint8_t*)new_Piece->PhysicalStart)
                    &&
                    (((uint8_t*)new_MemMap + (new_numpages << EFI_PAGE_SHIFT)) <= (uint8_t*)PhysicalEnd)
                  ) // Bounds check
                {
                  break; // Found it, new_Piece holds the spot now. Also, we know it's at the start of new_Piece->PhysicalStart because we put it there with ActuallyFreeAddress.
                }
              }
            }

            // Mark the new area as memmap similarly to the above process, but using new_Piece instead of Piece, and new_MemMap instead of Global_Memory_Info.MemMap
            if(new_Piece->NumberOfPages == new_numpages) // Trivial case: The new space descriptor is just the right size and needs no splitting; saves a memory descriptor
            {
              new_Piece->Type = EfiMaxMemoryType + 3; // Special memmap type
            }
            else // Need to insert a memmap descriptor. Thanks to the way ActuallyFreeAddress works we know the map's new area is at the base of the EfiConventionalMemory descriptor
            {
              // Make a temporary descriptor to hold current piece's values, but modified for memmap
              EFI_MEMORY_DESCRIPTOR new_descriptor_temp;
              new_descriptor_temp.Type = EfiMaxMemoryType + 3; // Special memmap type
              new_descriptor_temp.Pad = new_Piece->Pad;
              new_descriptor_temp.PhysicalStart = new_Piece->PhysicalStart;
              new_descriptor_temp.VirtualStart = new_Piece->VirtualStart;
              new_descriptor_temp.NumberOfPages = new_numpages;
              new_descriptor_temp.Attribute = new_Piece->Attribute;

              // Modify this descriptor (shrink it) to reflect its new values
              new_Piece->PhysicalStart += (new_numpages << EFI_PAGE_SHIFT);
              new_Piece->VirtualStart += (new_numpages << EFI_PAGE_SHIFT);
              new_Piece->NumberOfPages -= new_numpages;

              // Move (copy) the whole memmap that's above this piece (including this freshly modified piece) from this piece to one MemMapDescriptorSize over
              AVX_memmove((uint8_t*)new_Piece + Global_Memory_Info.MemMapDescriptorSize, new_Piece, ((uint8_t*)new_MemMap + Global_Memory_Info.MemMapSize) - (uint8_t*)new_Piece); // Pointer math to get size

              // Insert the new piece (by overwriting the now-duplicated entry with new values)
              // I.e. turn this piece into what was stored in the temporary descriptor above
              new_Piece->Type = new_descriptor_temp.Type;
              new_Piece->Pad = new_descriptor_temp.Pad;
              new_Piece->PhysicalStart = new_descriptor_temp.PhysicalStart;
              new_Piece->VirtualStart = new_descriptor_temp.VirtualStart;
              new_Piece->NumberOfPages = new_descriptor_temp.NumberOfPages;
              new_Piece->Attribute = new_descriptor_temp.Attribute;
            }
            // Done modifying new map.
            // Clean up the old memmap area (mark it as EfiConventionalMemory) using old_Piece

            // Since old memmap is known not to be the entirety of the EfiLoaderData descriptor (though it is at the end of it), we need to add 1 new EfiConventionalMemory entry
            // Make a temporary descriptor to hold current piece's values
            EFI_MEMORY_DESCRIPTOR new_descriptor_temp;
            new_descriptor_temp.Type = old_Piece->Type;
            new_descriptor_temp.Pad = old_Piece->Pad;
            new_descriptor_temp.PhysicalStart = old_Piece->PhysicalStart;
            new_descriptor_temp.VirtualStart = old_Piece->VirtualStart;
            new_descriptor_temp.NumberOfPages = old_Piece->NumberOfPages - numpages;
            new_descriptor_temp.Attribute = old_Piece->Attribute;

            // Modify this descriptor (shrink it) to reflect its new values
            old_Piece->Type = EfiConventionalMemory;
            // Pad can stay the same
            old_Piece->PhysicalStart += (numpages << EFI_PAGE_SHIFT);
            old_Piece->VirtualStart += (numpages << EFI_PAGE_SHIFT);
            old_Piece->NumberOfPages = numpages;
            // Attribute can stay the same

            // Move (copy) the whole memmap that's above this piece (including this freshly modified piece) from this piece to one MemMapDescriptorSize over
            AVX_memmove((uint8_t*)old_Piece + Global_Memory_Info.MemMapDescriptorSize, old_Piece, ((uint8_t*)new_MemMap + Global_Memory_Info.MemMapSize) - (uint8_t*)old_Piece); // Pointer math to get size

            // Insert the new piece (by overwriting the now-duplicated entry with new values)
            // I.e. turn this piece into what was stored in the temporary descriptor above
            old_Piece->Type = new_descriptor_temp.Type;
            old_Piece->Pad = new_descriptor_temp.Pad;
            old_Piece->PhysicalStart = new_descriptor_temp.PhysicalStart;
            old_Piece->VirtualStart = new_descriptor_temp.VirtualStart;
            old_Piece->NumberOfPages = new_descriptor_temp.NumberOfPages;
            old_Piece->Attribute = new_descriptor_temp.Attribute;

            // Update Global_Memory_Info
            Global_Memory_Info.MemMapSize += Global_Memory_Info.MemMapDescriptorSize;
            Global_Memory_Info.MemMap = new_MemMap;

            // Done. Yay.
          }
          // Done with this case (memmap is on the end of the EfiLoaderData section)
        }
        else // Memmap is somewhere in the middle of the entry
        { // Modify 1 entry and add 2 entries
          if( ((numpages << EFI_PAGE_SHIFT) - Global_Memory_Info.MemMapSize) >= 2*Global_Memory_Info.MemMapDescriptorSize)
          { // There's enough space in this page for another 2 descriptors

            // How many pages do the memmap's surroundings take?
            size_t below_memmap_pages = ((uint8_t*)Global_Memory_Info.MemMap - (uint8_t*)Piece->PhysicalStart) >> EFI_PAGE_SHIFT; // This should never lose information because UEFI's memory quanta is 4kB pages.
            size_t above_memmap_pages = Piece->NumberOfPages - numpages - below_memmap_pages;

            // Make a temporary descriptor to hold "below memmap" segment
            EFI_MEMORY_DESCRIPTOR new_descriptor_temp_below;
            new_descriptor_temp_below.Type = Piece->Type;
            new_descriptor_temp_below.Pad = Piece->Pad;
            new_descriptor_temp_below.PhysicalStart = Piece->PhysicalStart;
            new_descriptor_temp_below.VirtualStart = Piece->VirtualStart;
            new_descriptor_temp_below.NumberOfPages = below_memmap_pages;
            new_descriptor_temp_below.Attribute = Piece->Attribute;

            // Make a temporary descriptor to hold "memmap" segment
            EFI_MEMORY_DESCRIPTOR new_descriptor_temp;
            new_descriptor_temp.Type = EfiMaxMemoryType + 3; // Special memmap type
            new_descriptor_temp.Pad = Piece->Pad;
            new_descriptor_temp.PhysicalStart = Piece->PhysicalStart + (below_memmap_pages << EFI_PAGE_SHIFT);
            new_descriptor_temp.VirtualStart = Piece->VirtualStart + (below_memmap_pages << EFI_PAGE_SHIFT);
            new_descriptor_temp.NumberOfPages = numpages;
            new_descriptor_temp.Attribute = Piece->Attribute;

            // Modify this descriptor's to become the "above memmap" part (higher memory address part)
            // Type stays the same
            // Nothing for pad
            Piece->PhysicalStart += (below_memmap_pages << EFI_PAGE_SHIFT) + (numpages << EFI_PAGE_SHIFT);
            Piece->VirtualStart += (below_memmap_pages << EFI_PAGE_SHIFT) + (numpages << EFI_PAGE_SHIFT);
            Piece->NumberOfPages = above_memmap_pages;
            // Nothing for attribute

            // Move (copy) the whole memmap that's above this piece (including this freshly modified piece) from this piece to one MemMapDescriptorSize over
            AVX_memmove((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize, Piece, ((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize) - (uint8_t*)Piece); // Pointer math to get size

            // Insert the "memmap" descriptor into this now-duplicate piece
            // I.e. turn this now-duplicate piece into what was stored in the temporary descriptor above
            Piece->Type = new_descriptor_temp.Type;
            Piece->Pad = new_descriptor_temp.Pad;
            Piece->PhysicalStart = new_descriptor_temp.PhysicalStart;
            Piece->VirtualStart = new_descriptor_temp.VirtualStart;
            Piece->NumberOfPages = new_descriptor_temp.NumberOfPages;
            Piece->Attribute = new_descriptor_temp.Attribute;

            // Update Global_Memory_Info
            Global_Memory_Info.MemMapSize += Global_Memory_Info.MemMapDescriptorSize;

            // Now do it again to insert the "below memmap" piece

            // Move (copy) the whole memmap that's above this piece (including this freshly modified piece) from this piece to one MemMapDescriptorSize over
            AVX_memmove((uint8_t*)Piece + Global_Memory_Info.MemMapDescriptorSize, Piece, ((uint8_t*)Global_Memory_Info.MemMap + Global_Memory_Info.MemMapSize) - (uint8_t*)Piece); // Pointer math to get size

            // Insert the "below memmap" piece
            // I.e. turn this now-duplicate piece into what was stored in the "below memmap" temporary descriptor above
            Piece->Type = new_descriptor_temp_below.Type;
            Piece->Pad = new_descriptor_temp_below.Pad;
            Piece->PhysicalStart = new_descriptor_temp_below.PhysicalStart;
            Piece->VirtualStart = new_descriptor_temp_below.VirtualStart;
            Piece->NumberOfPages = new_descriptor_temp_below.NumberOfPages;
            Piece->Attribute = new_descriptor_temp_below.Attribute;

            // Update Global_Memory_Info
            Global_Memory_Info.MemMapSize += Global_Memory_Info.MemMapDescriptorSize;

            // Done.
          }
          else
          { // Need to move the map somewhere with more pages available. Don't want to play any fragmentation games with it.

            // Move the whole map like "base of EfiLoaderData" case
            // This is the exact same process...

            size_t additional_pages = 0;
            if((Global_Memory_Info.MemMapDescriptorSize * 3) <= EFI_PAGE_SIZE) // We need enough space to add 3 descriptors (new memmap and clear the old one, clearing will make 2 new descriptors)
            {
              additional_pages++; // Add 1
            }
            else // This may never be necessary, but if it does become needed one day (huge descriptors or something), support is already here.
            {
              additional_pages = (Global_Memory_Info.MemMapDescriptorSize * 3) >> EFI_PAGE_SHIFT;
              if((Global_Memory_Info.MemMapDescriptorSize * 3) & EFI_PAGE_MASK)
              {
                additional_pages++;
              }
            }
            size_t new_numpages = numpages + additional_pages;

            EFI_PHYSICAL_ADDRESS new_MemMap_base_address = ActuallyFreeAddress(new_numpages, 0); // This will only give addresses at the base of a chunk of EfiConventionalMemory
            if(new_MemMap_base_address == -1)
            {
              printf("Can't move memmap for enlargement: malloc not usable.\r\n");
              break;
            }

            // Is this method (move whole memmap then modify it) better than moving it in pieces, inserting new descriptors between each piece?

            EFI_MEMORY_DESCRIPTOR * new_MemMap = (EFI_MEMORY_DESCRIPTOR*)new_MemMap_base_address;
            EFI_MEMORY_DESCRIPTOR * old_Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)new_MemMap + ((uint8_t*)Piece - (uint8_t*)Global_Memory_Info.MemMap)); // This is the descriptor where MemMap used to be; old_Piece contains it for use with the new map
            EFI_MEMORY_DESCRIPTOR * new_Piece; // We could re-use the Piece pointer now, but I think this is clearer.

            // Zero out the new memmap destination
            AVX_memset(new_MemMap, 0, new_numpages << EFI_PAGE_SHIFT);

            // Move (copy) the map from MemMap to new_MemMap
            AVX_memmove(new_MemMap, Global_Memory_Info.MemMap, Global_Memory_Info.MemMapSize);
            // Zero out the old one
            AVX_memset(Global_Memory_Info.MemMap, 0, Global_Memory_Info.MemMapSize); // We know MemMap should never overlap anytihing else, especially since we're moving (copying) it from EfiLoaderData to EfiConventionalMemory

            // Get a new piece pointer for the new location of the map (scan the map to find it)
            for(new_Piece = new_MemMap; new_Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)new_MemMap + Global_Memory_Info.MemMapSize); new_Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)new_Piece + Global_Memory_Info.MemMapDescriptorSize))
            {
              if((new_Piece->Type == EfiConventionalMemory) && (new_Piece->NumberOfPages >= new_numpages)) // The new map's in EfiConventionalMemory
              {
                PhysicalEnd = new_Piece->PhysicalStart + (new_Piece->NumberOfPages << EFI_PAGE_SHIFT); // Get the end of this range for bounds checking (this value might be the start address of the next range)

                if(
                    ((uint8_t*)new_MemMap >= (uint8_t*)new_Piece->PhysicalStart)
                    &&
                    (((uint8_t*)new_MemMap + (new_numpages << EFI_PAGE_SHIFT)) <= (uint8_t*)PhysicalEnd)
                  ) // Bounds check
                {
                  break; // Found it, new_Piece holds the spot now. Also, we know it's at the start of new_Piece->PhysicalStart because we put it there with ActuallyFreeAddress.
                }
              }
            }

            // Mark the new area as memmap similarly to the above process, but using new_Piece instead of Piece, and new_MemMap instead of Global_Memory_Info.MemMap
            if(new_Piece->NumberOfPages == new_numpages) // Trivial case: The new space descriptor is just the right size and needs no splitting; saves a memory descriptor
            {
              new_Piece->Type = EfiMaxMemoryType + 3; // Special memmap type
            }
            else // Need to insert a memmap descriptor. Thanks to the way ActuallyFreeAddress works we know the map's new area is at the base of the EfiConventionalMemory descriptor
            {
              // Make a temporary descriptor to hold current piece's values, but modified for memmap
              EFI_MEMORY_DESCRIPTOR new_descriptor_temp;
              new_descriptor_temp.Type = EfiMaxMemoryType + 3; // Special memmap type
              new_descriptor_temp.Pad = new_Piece->Pad;
              new_descriptor_temp.PhysicalStart = new_Piece->PhysicalStart;
              new_descriptor_temp.VirtualStart = new_Piece->VirtualStart;
              new_descriptor_temp.NumberOfPages = new_numpages;
              new_descriptor_temp.Attribute = new_Piece->Attribute;

              // Modify this descriptor (shrink it) to reflect its new values
              new_Piece->PhysicalStart += (new_numpages << EFI_PAGE_SHIFT);
              new_Piece->VirtualStart += (new_numpages << EFI_PAGE_SHIFT);
              new_Piece->NumberOfPages -= new_numpages;

              // Move (copy) the whole memmap that's above this piece (including this freshly modified piece) from this piece to one MemMapDescriptorSize over
              AVX_memmove((uint8_t*)new_Piece + Global_Memory_Info.MemMapDescriptorSize, new_Piece, ((uint8_t*)new_MemMap + Global_Memory_Info.MemMapSize) - (uint8_t*)new_Piece); // Pointer math to get size

              // Insert the new piece (by overwriting the now-duplicated entry with new values)
              // I.e. turn this piece into what was stored in the temporary descriptor above
              new_Piece->Type = new_descriptor_temp.Type;
              new_Piece->Pad = new_descriptor_temp.Pad;
              new_Piece->PhysicalStart = new_descriptor_temp.PhysicalStart;
              new_Piece->VirtualStart = new_descriptor_temp.VirtualStart;
              new_Piece->NumberOfPages = new_descriptor_temp.NumberOfPages;
              new_Piece->Attribute = new_descriptor_temp.Attribute;
            }
            // Done modifying new map.
            // Clean up the old memmap area (mark it as EfiConventionalMemory) using old_Piece

            // Since old memmap is known not to be the entirety of the EfiLoaderData descriptor (it's in the middle of it), we need to add 1 new EfiConventionalMemory entry and 1 new EfiLoaderData entry
            // How many pages do the old memmap's surroundings take?
            size_t below_memmap_pages = ((uint8_t*)Global_Memory_Info.MemMap - (uint8_t*)old_Piece->PhysicalStart) >> EFI_PAGE_SHIFT; // This should never lose information because UEFI's memory quanta is 4kB pages.
            size_t above_memmap_pages = old_Piece->NumberOfPages - numpages - below_memmap_pages;

            // Make a temporary descriptor to hold "below memmap" segment
            EFI_MEMORY_DESCRIPTOR new_descriptor_temp_below;
            new_descriptor_temp_below.Type = old_Piece->Type;
            new_descriptor_temp_below.Pad = old_Piece->Pad;
            new_descriptor_temp_below.PhysicalStart = old_Piece->PhysicalStart;
            new_descriptor_temp_below.VirtualStart = old_Piece->VirtualStart;
            new_descriptor_temp_below.NumberOfPages = below_memmap_pages;
            new_descriptor_temp_below.Attribute = old_Piece->Attribute;

            // Make a temporary descriptor to hold "memmap" segment
            EFI_MEMORY_DESCRIPTOR new_descriptor_temp;
            new_descriptor_temp.Type = EfiConventionalMemory;
            new_descriptor_temp.Pad = old_Piece->Pad;
            new_descriptor_temp.PhysicalStart = old_Piece->PhysicalStart + (below_memmap_pages << EFI_PAGE_SHIFT);
            new_descriptor_temp.VirtualStart = old_Piece->VirtualStart + (below_memmap_pages << EFI_PAGE_SHIFT);
            new_descriptor_temp.NumberOfPages = numpages;
            new_descriptor_temp.Attribute = old_Piece->Attribute;

            // Modify this descriptor's to become the "above memmap" part (higher memory address part)
            // Type stays the same
            // Nothing for pad
            old_Piece->PhysicalStart += (below_memmap_pages << EFI_PAGE_SHIFT) + (numpages << EFI_PAGE_SHIFT);
            old_Piece->VirtualStart += (below_memmap_pages << EFI_PAGE_SHIFT) + (numpages << EFI_PAGE_SHIFT);
            old_Piece->NumberOfPages = above_memmap_pages;
            // Nothing for attribute

            // Move (copy) the whole memmap that's above this piece (including this freshly modified piece) from this piece to one MemMapDescriptorSize over
            AVX_memmove((uint8_t*)old_Piece + Global_Memory_Info.MemMapDescriptorSize, old_Piece, ((uint8_t*)new_MemMap + Global_Memory_Info.MemMapSize) - (uint8_t*)old_Piece); // Pointer math to get size

            // Insert the "memmap" descriptor into this now-duplicate piece
            // I.e. turn this now-duplicate piece into what was stored in the temporary descriptor above
            old_Piece->Type = new_descriptor_temp.Type;
            old_Piece->Pad = new_descriptor_temp.Pad;
            old_Piece->PhysicalStart = new_descriptor_temp.PhysicalStart;
            old_Piece->VirtualStart = new_descriptor_temp.VirtualStart;
            old_Piece->NumberOfPages = new_descriptor_temp.NumberOfPages;
            old_Piece->Attribute = new_descriptor_temp.Attribute;

            // Update Global_Memory_Info
            Global_Memory_Info.MemMapSize += Global_Memory_Info.MemMapDescriptorSize;

            // Now do it again to insert the "below memmap" piece

            // Move (copy) the whole memmap that's above this piece (including this freshly modified piece) from this piece to one MemMapDescriptorSize over
            AVX_memmove((uint8_t*)old_Piece + Global_Memory_Info.MemMapDescriptorSize, old_Piece, ((uint8_t*)new_MemMap + Global_Memory_Info.MemMapSize) - (uint8_t*)old_Piece); // Pointer math to get size

            // Insert the "below memmap" piece
            // I.e. turn this now-duplicate piece into what was stored in the "below memmap" temporary descriptor above
            old_Piece->Type = new_descriptor_temp_below.Type;
            old_Piece->Pad = new_descriptor_temp_below.Pad;
            old_Piece->PhysicalStart = new_descriptor_temp_below.PhysicalStart;
            old_Piece->VirtualStart = new_descriptor_temp_below.VirtualStart;
            old_Piece->NumberOfPages = new_descriptor_temp_below.NumberOfPages;
            old_Piece->Attribute = new_descriptor_temp_below.Attribute;

            // Update Global_Memory_Info
            Global_Memory_Info.MemMapSize += Global_Memory_Info.MemMapDescriptorSize;
            Global_Memory_Info.MemMap = new_MemMap;

            // Done. Woot.
          }
          // Done with this case (memmap is in the middle somewhere of the EfiLoaderData section)
        }
        // All done.
        break;
      } // end of "Found it"
    } // end of checking this EfiLoaderData descriptor
  } // end for

  // TODO: Call mergecontiguousconventionalmemory here or something
  // Each allocate function should call an update memmap with address, size, memtype
  // Every free call should contain a call to mergecontiguousconventionalmemory
}
*/
