//==================================================================================================================================
//  Simple Kernel: Main Header
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
// This file provides inclusions, #define switches, structure definitions, and function prototypes for a bare-metal x86-64 program
// (also known as a 64-bit kernel). See Kernel64.c for further details about this program.
//

#ifndef _Kernel64_H
#define _Kernel64_H

#define MAJOR_VER 0
#define MINOR_VER 9

// In freestanding mode, the only available standard header files are: <float.h>,
// <iso646.h>, <limits.h>, <stdarg.h>, <stdbool.h>, <stddef.h>, and <stdint.h>
// (C99 standard 4.6).

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <float.h>
#include <stdarg.h>

//#include <cpuid.h> // ...We also have this. Don't need it, though.

//----------------------------------------------------------------------------------------------------------------------------------
//  UEFI and Bootloader Functions, Definitions, and Declarations
//----------------------------------------------------------------------------------------------------------------------------------
//
// Functions, definitions, and declarations necessary for using UEFI functions and services passed in by the bootloader
//

#include "EfiBind.h"
#include "EfiTypes.h"
#include "EfiError.h"

#include "avxmem.h"
#include "ISR.h"

//#include <acpi.h>

// GRAPHICS
typedef struct {
  UINT32            RedMask;
  UINT32            GreenMask;
  UINT32            BlueMask;
  UINT32            ReservedMask;
} EFI_PIXEL_BITMASK;

typedef enum {
  PixelRedGreenBlueReserved8BitPerColor, // 0
  PixelBlueGreenRedReserved8BitPerColor, // 1
  PixelBitMask, // 2
  PixelBltOnly, // 3
  PixelFormatMax // 4
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
  UINT32                     Version;
  UINT32                     HorizontalResolution;
  UINT32                     VerticalResolution;
  EFI_GRAPHICS_PIXEL_FORMAT  PixelFormat; // 0 - 4
  EFI_PIXEL_BITMASK          PixelInformation;
  UINT32                     PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
  UINT32                                 MaxMode;
  UINT32                                 Mode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION   *Info;
  UINTN                                  SizeOfInfo;
  EFI_PHYSICAL_ADDRESS                   FrameBufferBase;
  UINTN                                  FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

//
// Standard EFI table header
//

// Defined in EfiTypes.h
/*
typedef struct _EFI_TABLE_HEADER {
    UINT64                      Signature;
    UINT32                      Revision;
    UINT32                      HeaderSize;
    UINT32                      CRC32;
    UINT32                      Reserved;
} EFI_TABLE_HEADER;
*/

//
// EFI Time
//

typedef struct {
        UINT32                      Resolution;     // 1e-6 parts per million
        UINT32                      Accuracy;       // hertz
        BOOLEAN                     SetsToZero;     // Set clears sub-second time
} EFI_TIME_CAPABILITIES;


typedef
EFI_STATUS
(EFIAPI *EFI_GET_TIME) (
    OUT EFI_TIME                    *Time,
    OUT EFI_TIME_CAPABILITIES       *Capabilities OPTIONAL
    );

typedef
EFI_STATUS
(EFIAPI *EFI_SET_TIME) (
    IN EFI_TIME                     *Time
    );

typedef
EFI_STATUS
(EFIAPI *EFI_GET_WAKEUP_TIME) (
    OUT BOOLEAN                     *Enabled,
    OUT BOOLEAN                     *Pending,
    OUT EFI_TIME                    *Time
    );

typedef
EFI_STATUS
(EFIAPI *EFI_SET_WAKEUP_TIME) (
    IN BOOLEAN                      Enable,
    IN EFI_TIME                     *Time OPTIONAL
    );

//
// EFI platform varibles
//

#define EFI_GLOBAL_VARIABLE     \
    { 0x8BE4DF61, 0x93CA, 0x11d2, {0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C} }

// Variable attributes
#define EFI_VARIABLE_NON_VOLATILE                          0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS                    0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS                        0x00000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD                 0x00000008
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS            0x00000010
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS 0x00000020
#define EFI_VARIABLE_APPEND_WRITE                          0x00000040

// Variable size limitation
#define EFI_MAXIMUM_VARIABLE_SIZE           1024

typedef
EFI_STATUS
(EFIAPI *EFI_GET_VARIABLE) (
    IN CHAR16                       *VariableName,
    IN EFI_GUID                     *VendorGuid,
    OUT UINT32                      *Attributes OPTIONAL,
    IN OUT UINTN                    *DataSize,
    OUT VOID                        *Data
    );

typedef
EFI_STATUS
(EFIAPI *EFI_GET_NEXT_VARIABLE_NAME) (
    IN OUT UINTN                    *VariableNameSize,
    IN OUT CHAR16                   *VariableName,
    IN OUT EFI_GUID                 *VendorGuid
    );


typedef
EFI_STATUS
(EFIAPI *EFI_SET_VARIABLE) (
    IN CHAR16                       *VariableName,
    IN EFI_GUID                     *VendorGuid,
    IN UINT32                       Attributes,
    IN UINTN                        DataSize,
    IN VOID                         *Data
    );

//
// EFI Memory
//

// Looking for something that's not here? Try EfiTypes.h.

typedef
EFI_STATUS
(EFIAPI *EFI_SET_VIRTUAL_ADDRESS_MAP) (                // For identity mapping, pass these:
    IN UINTN                        MemoryMapSize,     // LP->Memory_Map_Size
    IN UINTN                        DescriptorSize,    // LP->Memory_Map_Descriptor_Size
    IN UINT32                       DescriptorVersion, // LP->Memory_Map_Descriptor_Version
    IN EFI_MEMORY_DESCRIPTOR        *VirtualMap        // LP->Memory_Map
    );


#define EFI_OPTIONAL_PTR            0x00000001
#define EFI_INTERNAL_FNC            0x00000002      // Pointer to internal runtime fnc
#define EFI_INTERNAL_PTR            0x00000004      // Pointer to internal runtime data


typedef
EFI_STATUS
(EFIAPI *EFI_CONVERT_POINTER) (
    IN UINTN                        DebugDisposition,
    IN OUT VOID                     **Address
    );

//
// EFI Reset Functions
//

typedef enum {
    EfiResetCold,
    EfiResetWarm,
    EfiResetShutdown
} EFI_RESET_TYPE;

typedef
EFI_STATUS
(EFIAPI *EFI_RESET_SYSTEM) (
    IN EFI_RESET_TYPE           ResetType,
    IN EFI_STATUS               ResetStatus,
    IN UINTN                    DataSize,
    IN CHAR16                   *ResetData OPTIONAL
    );

//
// Misc
//

typedef
EFI_STATUS
(EFIAPI *EFI_GET_NEXT_HIGH_MONO_COUNT) (
    OUT UINT32                  *HighCount
    );

typedef struct {
    EFI_GUID                    CapsuleGuid;
    UINT32                      HeaderSize;
    UINT32                      Flags;
    UINT32                      CapsuleImageSize;
} EFI_CAPSULE_HEADER;

#define CAPSULE_FLAGS_PERSIST_ACROSS_RESET    0x00010000
#define CAPSULE_FLAGS_POPULATE_SYSTEM_TABLE   0x00020000
#define CAPSULE_FLAGS_INITIATE_RESET          0x00040000

typedef
EFI_STATUS
(EFIAPI *EFI_UPDATE_CAPSULE) (
    IN EFI_CAPSULE_HEADER       **CapsuleHeaderArray,
    IN UINTN                    CapsuleCount,
    IN EFI_PHYSICAL_ADDRESS     ScatterGatherList OPTIONAL
    );

typedef
EFI_STATUS
(EFIAPI *EFI_QUERY_CAPSULE_CAPABILITIES) (
    IN  EFI_CAPSULE_HEADER       **CapsuleHeaderArray,
    IN  UINTN                    CapsuleCount,
    OUT UINT64                   *MaximumCapsuleSize,
    OUT EFI_RESET_TYPE           *ResetType
    );

typedef
EFI_STATUS
(EFIAPI *EFI_QUERY_VARIABLE_INFO) (
    IN  UINT32                  Attributes,
    OUT UINT64                  *MaximumVariableStorageSize,
    OUT UINT64                  *RemainingVariableStorageSize,
    OUT UINT64                  *MaximumVariableSize
    );

//
// EFI Runtime Services
//

typedef struct  {
    EFI_TABLE_HEADER                Hdr;

    //
    // Time services
    //

    EFI_GET_TIME                    GetTime;
    EFI_SET_TIME                    SetTime;
    EFI_GET_WAKEUP_TIME             GetWakeupTime;
    EFI_SET_WAKEUP_TIME             SetWakeupTime;

    //
    // Virtual memory services
    //

    EFI_SET_VIRTUAL_ADDRESS_MAP     SetVirtualAddressMap;
    EFI_CONVERT_POINTER             ConvertPointer;

    //
    // Variable serviers
    //

    EFI_GET_VARIABLE                GetVariable;
    EFI_GET_NEXT_VARIABLE_NAME      GetNextVariableName;
    EFI_SET_VARIABLE                SetVariable;

    //
    // Misc
    //

    EFI_GET_NEXT_HIGH_MONO_COUNT    GetNextHighMonotonicCount;
    EFI_RESET_SYSTEM                ResetSystem;

    EFI_UPDATE_CAPSULE              UpdateCapsule;
    EFI_QUERY_CAPSULE_CAPABILITIES  QueryCapsuleCapabilities;
    EFI_QUERY_VARIABLE_INFO         QueryVariableInfo;
} EFI_RUNTIME_SERVICES;

//
// EFI File Metadata
//

typedef struct {
    UINT64                  Size;
    UINT64                  FileSize;
    UINT64                  PhysicalSize;
    EFI_TIME                CreateTime;
    EFI_TIME                LastAccessTime;
    EFI_TIME                ModificationTime;
    UINT64                  Attribute;
    CHAR16                  FileName[1];
} EFI_FILE_INFO;

//
// System Configuration Table Definitions
//

#define MPS_TABLE_GUID    \
    { 0xeb9d2d2f, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d} }

#define ACPI_10_TABLE_GUID    \
    { 0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d} }

#define ACPI_20_TABLE_GUID  \
    { 0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81} }

#define SMBIOS_TABLE_GUID    \
    { 0xeb9d2d31, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d} }

#define SMBIOS3_TABLE_GUID    \
    { 0xf2fd1544, 0x9794, 0x4a2c, {0x99, 0x2e, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94} }

#define SAL_SYSTEM_TABLE_GUID    \
    { 0xeb9d2d32, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d} }

static const EFI_GUID MpsTableGuid = MPS_TABLE_GUID;
static const EFI_GUID Acpi10TableGuid = ACPI_10_TABLE_GUID;
static const EFI_GUID Acpi20TableGuid = ACPI_20_TABLE_GUID;
static const EFI_GUID SmbiosTableGuid = SMBIOS_TABLE_GUID;
static const EFI_GUID Smbios3TableGuid = SMBIOS3_TABLE_GUID;
static const EFI_GUID SalSystemTableGuid = SAL_SYSTEM_TABLE_GUID;

typedef struct _EFI_CONFIGURATION_TABLE {
    EFI_GUID                VendorGuid;
    VOID                    *VendorTable;
} EFI_CONFIGURATION_TABLE;

//
// Bootloader Structures
//

typedef struct {
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  *GPUArray;             // This array contains the EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE structures for each available framebuffer
  UINT64                              NumberOfFrameBuffers; // The number of pointers in the array (== the number of available framebuffers)
} GPU_CONFIG;

// LP->GPU_Configs->GPU_MODE.FrameBufferBase
// LP->GPU_Configs->GPU_MODE.Info->HorizontalResolution
// GPU_MODE == GPUArray[0,1,2...NumberOfFrameBuffers]

// #define GTX1080 LP->GPU_Configs->GPUArray[0]
// GTX1080.FrameBufferBase
// GTX1080.Info->HorizontalResolution

typedef struct {
  UINT32                    UEFI_Version;                   // The system UEFI version
  UINT32                    Bootloader_MajorVersion;        // The major version of the bootloader
  UINT32                    Bootloader_MinorVersion;        // The minor version of the bootloader

  UINT32                    Memory_Map_Descriptor_Version;  // The memory descriptor version
  UINTN                     Memory_Map_Descriptor_Size;     // The size of an individual memory descriptor
  EFI_MEMORY_DESCRIPTOR    *Memory_Map;                     // The system memory map as an array of EFI_MEMORY_DESCRIPTOR structs
  UINTN                     Memory_Map_Size;                // The total size of the system memory map

  EFI_PHYSICAL_ADDRESS      Kernel_BaseAddress;             // The base memory address of the loaded kernel file
  UINTN                     Kernel_Pages;                   // The number of pages (1 page == 4096 bytes) allocated for the kernel file

  CHAR16                   *ESP_Root_Device_Path;           // A UTF-16 string containing the drive root of the EFI System Partition as converted from UEFI device path format
  UINT64                    ESP_Root_Size;                  // The size (in bytes) of the above ESP root string
  CHAR16                   *Kernel_Path;                    // A UTF-16 string containing the kernel's file path relative to the EFI System Partition root (it's the first line of Kernel64.txt)
  UINT64                    Kernel_Path_Size;               // The size (in bytes) of the above kernel file path
  CHAR16                   *Kernel_Options;                 // A UTF-16 string containing various load options (it's the second line of Kernel64.txt)
  UINT64                    Kernel_Options_Size;            // The size (in bytes) of the above load options string

  EFI_RUNTIME_SERVICES     *RTServices;                     // UEFI Runtime Services
  GPU_CONFIG               *GPU_Configs;                    // Information about available graphics output devices; see below GPU_CONFIG struct for details
  EFI_FILE_INFO            *FileMeta;                       // Kernel file metadata
  EFI_CONFIGURATION_TABLE  *ConfigTables;                   // UEFI-installed system configuration tables (ACPI, SMBIOS, etc.)
  UINTN                     Number_of_ConfigTables;         // The number of system configuration tables
} LOADER_PARAMS;

// END UEFI and Bootloader functions, definitions, and declarations

//==================================================================================================================================
// Anything below this comment (except the #endif at the very bottom) is safe to  remove without breaking compatibility with the
// Simple UEFI Bootloader. Useful if you wanted to make your own kernel from total scratch.
//==================================================================================================================================

//----------------------------------------------------------------------------------------------------------------------------------
//  Function Support Definitions
//----------------------------------------------------------------------------------------------------------------------------------
//
// Support structure definitions and declarations
//

// For memory functions, like malloc and friends in Memory.c
typedef struct {
  UINTN                   MemMapSize;              // Size of the memory map (LP->Memory_Map_Size)
  UINTN                   MemMapDescriptorSize;    // Size of memory map descriptors (LP->Memory_Map_Descriptor_Size)
  EFI_MEMORY_DESCRIPTOR  *MemMap;                  // Pointer to memory map (LP->Memory_Map)
  UINT32                  MemMapDescriptorVersion; // Memory map descriptor version
  UINT32                  Pad;                     // Pad to multiple of 64 bits
} GLOBAL_MEMORY_INFO_STRUCT;

// For printf
typedef struct {
	EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  defaultGPU;       // Default EFI GOP output device from GPUArray (should be GPUArray[0] if there's only 1)
	UINT32                             height;           // Character font height
	UINT32                             width;            // Character font width (in bits)
	UINT32                             font_color;       // Default font color
	UINT32                             highlight_color;  // Default highlight color
  UINT32                             background_color; // Default background color
	UINT32                             x;                // Leftmost x-coord that's in-bounds (NOTE: per UEFI Spec 2.7 Errata A, (0,0) is always the top left in-bounds pixel.)
	UINT32                             y;                // Topmost y-coord
	UINT32                             scale;            // Output scale for systemfont used by printf
  UINT32                             index;            // Global string index for printf, etc. to keep track of cursor's postion in the framebuffer
  UINT32                             textscrollmode;   // What to do when a newline goes off the bottom of the screen: 0 = scroll entire screen, 1 = wrap around to the top
} GLOBAL_PRINT_INFO_STRUCT;

// Intel Architecture Manual Vol. 3A, Fig. 3-11 (Pseudo-Descriptor Formats)
typedef struct __attribute__ ((packed)) {
  UINT16 Limit; // Limit + 1 = size, since limit + base = the last valid address
  UINT64 BaseAddress;
} DT_STRUCT; // GDTR and IDTR use this format

// Intel Architecture Manual Vol. 3A, Fig. 3-8 (Segment Descriptor)
typedef struct __attribute__ ((packed)) {
  UINT16 SegmentLimit1; // Low bits, SegmentLimit2andMisc2 has MSBs (it's a 20-bit value)
  UINT16 BaseAddress1; // Low bits (15:0)
  UINT8  BaseAddress2; // Next bits (23:16)
  UINT8  Misc1; // Bits 0-3: segment/gate Type, 4: S, 5-6: DPL, 7: P
  UINT8  SegmentLimit2andMisc2; // Bits 0-3: seglimit2, 4: Available, 5: L, 6: D/B, 7: G
  UINT8  BaseAddress3; // Most significant bits (31:24)
} GDT_ENTRY_STRUCT; // This whole struct can fit in a 64-bit int. Printf %lx could give the whole thing.

// Intel Architecture Manual Vol. 3A, Fig. 7-4 (Format of TSS and LDT Descriptors in 64-bit Mode)
typedef struct __attribute__ ((packed)) {
  UINT16 SegmentLimit1; // Low bits, SegmentLimit2andMisc2 has MSBs (it's a 20-bit value)
  UINT16 BaseAddress1; // Low bits (15:0)
  UINT8  BaseAddress2; // Next bits (23:16)
  UINT8  Misc1; // Bits 0-3: segment/gate Type, 4: S, 5-6: DPL, 7: P
  UINT8  SegmentLimit2andMisc2; // Bits 0-3: seglimit2, 4: Available, 5: L, 6: D/B, 7: G
  UINT8  BaseAddress3; // More significant bits (31:24)
  UINT32 BaseAddress4; // Most significant bits (63:32)
  UINT8  Reserved;
  UINT8  Misc3andReserved2; // Low 4 bits are 0, upper 4 bits are reserved
  UINT16 Reserved3;
} TSS_LDT_ENTRY_STRUCT; // TSS and LDT use this

// Intel Architecture Manual Vol. 3A, Fig. 5-9 (Call-Gate Descriptor in IA-32e mode)
typedef struct __attribute__ ((packed)) {
  UINT16 SegmentOffset1; // Low bits (15:0)
  UINT16 SegmentSelector;
  UINT8  Zero; // Should be set to all 0
  UINT8  Misc1; // Bits 0-3: segment/gate Type (1100), 4: S (set to 0), 5-6: DPL, 7: P
  UINT16 SegmentOffset2; // Middle bits (31:16)
  UINT32 SegmentOffset3; // Most significant bits (63:32)
  UINT8  Reserved;
  UINT8  Misc2andReserved2; // Low 5 bits should be 0, upper 3 bits are reserved
  UINT16 Reserved3;
} CALL_GATE_ENTRY_STRUCT;
// Call gates aren't needed if privilege level doesn't change and if not switching out of 64-bit mode
// This software stays in privilege level 0 (ring 0) and 64-bit mode, so call gates aren't used

// Intel Architecture Manual Vol. 3A, Fig. 7-11 (64-Bit TSS Format)
typedef struct __attribute__ ((packed)) {
  UINT32 Reserved_0;
  // RSP values for privilege levels 0-2
  UINT32 RSP_0_low;
  UINT32 RSP_0_high;
  UINT32 RSP_1_low;
  UINT32 RSP_1_high;
  UINT32 RSP_2_low;
  UINT32 RSP_2_high;

  UINT32 Reserved_1;
  UINT32 Reserved_2;
  // Interrupt Stack Table pointers
  UINT32 IST_1_low;
  UINT32 IST_1_high;
  UINT32 IST_2_low;
  UINT32 IST_2_high;
  UINT32 IST_3_low;
  UINT32 IST_3_high;
  UINT32 IST_4_low;
  UINT32 IST_4_high;
  UINT32 IST_5_low;
  UINT32 IST_5_high;
  UINT32 IST_6_low;
  UINT32 IST_6_high;
  UINT32 IST_7_low;
  UINT32 IST_7_high;

  UINT32 Reserved_3;
  UINT32 Reserved_4;

  UINT16 Reserved_5;
  UINT16 IO_Map_Base; // 16-bit offset to I/O permission bit map, relative to 64-bit TSS base (i.e. base of this struct)
} TSS64_STRUCT;

// Intel Architecture Manual Vol. 3A, Fig. 6-7 (64-Bit IDT Gate Descriptors)
typedef struct __attribute__ ((packed)) {
 UINT16 Offset1; // Low offset bits (15:0)
 UINT16 SegmentSelector;
 UINT8  ISTandZero; // Low bits (2:0) are IST, (7:3) should be set to 0
 UINT8  Misc; // Bits 0-3: segment/gate Type, 4: S (set to 0), 5-6: DPL, 7: P
 UINT16 Offset2; // Middle offset bits (31:16)
 UINT32 Offset3; // Upper offset bits (63:32)
 UINT32 Reserved;
} IDT_GATE_STRUCT; // Interrupt and trap gates use this format

// See ISR.h for the specific interrupt structures

// ACPI Structures
// ACPI Specification 6.2A, section 5.2.5 (Root System Description Pointer (RSDP))
typedef struct __attribute__((packed)) {
  char      Signature[8]; // "RSD PTR " with trailing space
  uint8_t   Checksum;
  char      OEMID[6];
  uint8_t   Revision;
  uint32_t  RSDTAddress; // 32-bit RSDT
} RSDP_10_STRUCT;

typedef struct __attribute__((packed)) {
  RSDP_10_STRUCT  RSDP_10_Section;
  uint32_t        Length;
  uint64_t        XSDTAddress; // 64-bit RSDT is XSDT
  uint8_t         ExtendedChecksum;
  uint8_t         Reserved[3];
} RSDP_20_STRUCT;

typedef struct __attribute__((packed)) {
  char      Signature[4];
  uint32_t  Length;
  uint8_t   Revision;
  uint8_t   Checksum;
  char      OEMID[6];
  char      OEMTableID[8];
  uint32_t  OEMRevision;
  uint32_t  CreatorID;
  uint32_t  CreatorRevision;
} SDT_HEADER_STRUCT;

typedef struct __attribute__((packed)) {
  SDT_HEADER_STRUCT SDTHeader;
  uint64_t          Entry[1]; // Size of XSDT is determined by "Length," and each entry is 8 bytes
} XSDT_STRUCT; // Signature is "XSDT"

//----------------------------------------------------------------------------------------------------------------------------------
// Global Variables
//----------------------------------------------------------------------------------------------------------------------------------
/*
// Configuration Table GUIDs
extern EFI_GUID MpsTableGuid;
extern EFI_GUID Acpi10TableGuid;
extern EFI_GUID Acpi20TableGuid;
extern EFI_GUID SmbiosTableGuid;
extern EFI_GUID Smbios3TableGuid;
extern EFI_GUID SalSystemTableGuid;
*/
extern GLOBAL_MEMORY_INFO_STRUCT Global_Memory_Info;
extern GLOBAL_PRINT_INFO_STRUCT Global_Print_Info;

// Because kernel_main() is a naked function and can't have local variables that would require stack space...
extern unsigned char swapped_image[];
extern char brandstring[];
extern char Manufacturer_ID[];

//----------------------------------------------------------------------------------------------------------------------------------
//  Function Prototypes
//----------------------------------------------------------------------------------------------------------------------------------
//
// All functions defined by files in the "src" folder
//

// Initialization-related functions (System.c)
void System_Init(LOADER_PARAMS * LP);

uint64_t get_tick(void);
void HaCF(void); // Note: this is at the very bottom of System.c
void Enable_AVX(void);
void Enable_Maskable_Interrupts(void); // Exceptions and Non-Maskable Interrupts are always enabled.
void Enable_HWP(void);
uint8_t Hypervisor_check(void);
uint8_t read_perfs_initial(uint64_t * perfs);
uint64_t get_CPU_freq(uint64_t * perfs, uint8_t avg_or_measure);

uint32_t portio_rw(uint16_t port_address, uint32_t data, int size, int rw);
uint64_t msr_rw(uint64_t msr, uint64_t data, int rw);
uint32_t vmxcsr_rw(uint32_t data, int rw);
uint32_t mxcsr_rw(uint32_t data, int rw);
uint64_t control_register_rw(int crX, uint64_t in_out, int rw);
uint64_t xcr_rw(uint64_t xcrX, uint64_t data, int rw);
uint64_t read_cs(void);

DT_STRUCT get_gdtr(void);
void set_gdtr(DT_STRUCT gdtr_data);
DT_STRUCT get_idtr(void);
void set_idtr(DT_STRUCT idtr_data);
uint16_t get_ldtr(void);
void set_ldtr(uint16_t ldtr_data);
uint16_t get_tsr(void);
void set_tsr(uint16_t tsr_data);

void Setup_MinimalGDT(void);
void Setup_IDT(void);
void Setup_Paging(void);

char * Get_Brandstring(uint32_t * brandstring); // "brandstring" must be a 48-byte array
char * Get_Manufacturer_ID(char * Manufacturer_ID); // "Manufacturer_ID" must be a 13-byte array
void cpu_features(uint64_t rax_value, uint64_t rcx_value);

 // For interrupt handling
void User_ISR_handler(INTERRUPT_FRAME * i_frame);
void CPU_ISR_handler(INTERRUPT_FRAME * i_frame);
void CPU_EXC_handler(EXCEPTION_FRAME * e_frame);

  // Special CPU handlers
void DE_ISR_handler(INTERRUPT_FRAME * i_frame); // Fault #DE: Divide Error (divide by 0 or not enough bits in destination)
void DB_ISR_handler(INTERRUPT_FRAME * i_frame); // Fault/Trap #DB: Debug Exception
void NMI_ISR_handler(INTERRUPT_FRAME * i_frame); // NMI (Nonmaskable External Interrupt)
void BP_ISR_handler(INTERRUPT_FRAME * i_frame); // Trap #BP: Breakpoint (INT3 instruction)
void OF_ISR_handler(INTERRUPT_FRAME * i_frame); // Trap #OF: Overflow (INTO instruction)
void BR_ISR_handler(INTERRUPT_FRAME * i_frame); // Fault #BR: BOUND Range Exceeded (BOUND instruction)
void UD_ISR_handler(INTERRUPT_FRAME * i_frame); // Fault #UD: Invalid or Undefined Opcode
void NM_ISR_handler(INTERRUPT_FRAME * i_frame); // Fault #NM: Device Not Available Exception

void DF_EXC_handler(EXCEPTION_FRAME * e_frame); // Abort #DF: Double Fault (error code is always 0)

void CSO_ISR_handler(INTERRUPT_FRAME * i_frame); // Fault (i386): Coprocessor Segment Overrun (long since obsolete, included for completeness)

void TS_EXC_handler(EXCEPTION_FRAME * e_frame); // Fault #TS: Invalid TSS
void NP_EXC_handler(EXCEPTION_FRAME * e_frame); // Fault #NP: Segment Not Present
void SS_EXC_handler(EXCEPTION_FRAME * e_frame); // Fault #SS: Stack Segment Fault
void GP_EXC_handler(EXCEPTION_FRAME * e_frame); // Fault #GP: General Protection
void PF_EXC_handler(EXCEPTION_FRAME * e_frame); // Fault #PF: Page Fault

void MF_ISR_handler(INTERRUPT_FRAME * i_frame); // Fault #MF: Math Error (x87 FPU Floating-Point Math Error)

void AC_EXC_handler(EXCEPTION_FRAME * e_frame); // Fault #AC: Alignment Check (error code is always 0)

void MC_ISR_handler(INTERRUPT_FRAME * i_frame); // Abort #MC: Machine Check
void XM_ISR_handler(INTERRUPT_FRAME * i_frame); // Fault #XM: SIMD Floating-Point Exception (SSE instructions)
void VE_ISR_handler(INTERRUPT_FRAME * i_frame); // Fault #VE: Virtualization Exception

void SX_EXC_handler(EXCEPTION_FRAME * e_frame); // Fault #SX: Security Exception

  // Special user-defined handlers
// (none yet!)

 // Interrupt support functions
void ISR_regdump(INTERRUPT_FRAME * i_frame);
void EXC_regdump(EXCEPTION_FRAME * e_frame);
void AVX_regdump(XSAVE_AREA_LAYOUT * layout_area);

// NOTE: Not in System.c, these functions are in Kernel64.c.
void Print_All_CRs_and_Some_Major_CPU_Features(void);
void Print_Loader_Params(LOADER_PARAMS * LP);
void Print_Segment_Registers(void);

// Memory-related functions (Memory.c)
uint8_t VerifyZeroMem(size_t NumBytes, uint64_t BaseAddr); // BaseAddr is a 64-bit unsigned int whose value is the memory address to verify
uint64_t GetMaxMappedPhysicalAddress(void);
uint64_t GetVisibleSystemRam(void);
uint64_t GetFreeSystemRam(void);
uint64_t GetFreePersistentRam(void);
uint64_t GuessInstalledSystemRam(void);
void print_system_memmap(void);
EFI_MEMORY_DESCRIPTOR * Set_Identity_VMAP(EFI_RUNTIME_SERVICES * RTServices);
void Setup_MemMap(void);
void ReclaimEfiBootServicesMemory(void);
void ReclaimEfiLoaderCodeMemory(void);
void MergeContiguousConventionalMemory(void);
EFI_PHYSICAL_ADDRESS pagetable_alloc(uint64_t pagetables_size);

  // For physical addresses
__attribute__((malloc)) void * malloc(size_t numbytes);

__attribute__((malloc)) void * malloc16(size_t numbytes);
__attribute__((malloc)) void * malloc32(size_t numbytes);
__attribute__((malloc)) void * malloc64(size_t numbytes);
__attribute__((malloc)) void * malloc4k(size_t pages);

EFI_PHYSICAL_ADDRESS ActuallyFreeAddress(size_t pages, EFI_PHYSICAL_ADDRESS OldAddress);
EFI_PHYSICAL_ADDRESS ActuallyFreeAddressByPage(size_t pages, EFI_PHYSICAL_ADDRESS OldAddress);
EFI_PHYSICAL_ADDRESS AllocateFreeAddressByPage(size_t pages, EFI_PHYSICAL_ADDRESS OldAddress);
EFI_PHYSICAL_ADDRESS AllocateFreeAddressBy16Bytes(size_t numbytes, EFI_PHYSICAL_ADDRESS OldAddress);
EFI_PHYSICAL_ADDRESS AllocateFreeAddressBy32Bytes(size_t numbytes, EFI_PHYSICAL_ADDRESS OldAddress);
EFI_PHYSICAL_ADDRESS AllocateFreeAddressBy64Bytes(size_t numbytes, EFI_PHYSICAL_ADDRESS OldAddress);

  // For virtual addresses
__attribute__((malloc)) void * Vmalloc(size_t numbytes);

__attribute__((malloc)) void * Vmalloc16(size_t numbytes);
__attribute__((malloc)) void * Vmalloc32(size_t numbytes);
__attribute__((malloc)) void * Vmalloc64(size_t numbytes);
__attribute__((malloc)) void * Vmalloc4k(size_t pages);

EFI_VIRTUAL_ADDRESS VActuallyFreeAddress(size_t pages, EFI_VIRTUAL_ADDRESS OldAddress);
EFI_VIRTUAL_ADDRESS VActuallyFreeAddressByPage(size_t pages, EFI_VIRTUAL_ADDRESS OldAddress);
EFI_VIRTUAL_ADDRESS VAllocateFreeAddressByPage(size_t pages, EFI_VIRTUAL_ADDRESS OldAddress);
EFI_VIRTUAL_ADDRESS VAllocateFreeAddressBy16Bytes(size_t numbytes, EFI_VIRTUAL_ADDRESS OldAddress);
EFI_VIRTUAL_ADDRESS VAllocateFreeAddressBy32Bytes(size_t numbytes, EFI_VIRTUAL_ADDRESS OldAddress);
EFI_VIRTUAL_ADDRESS VAllocateFreeAddressBy64Bytes(size_t numbytes, EFI_VIRTUAL_ADDRESS OldAddress);

// Drawing-related functions (Display.c)
void Blackscreen(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU);
void Colorscreen(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 color);

void Resetdefaultcolorscreen(void);
void Resetdefaultscreen(void);

void single_pixel(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 x, UINT32 y, UINT32 color);

void bitmap_anywhere_scaled(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, const unsigned char * bitmap, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale);
void Output_render_bitmap(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, const unsigned char * bitmap, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale, UINT32 index);

void bitmap_bitswap(const unsigned char * bitmap, UINT32 height, UINT32 width, unsigned char * output);
void bitmap_bitreverse(const unsigned char * bitmap, UINT32 height, UINT32 width, unsigned char * output);
void bitmap_bytemirror(const unsigned char * bitmap, UINT32 height, UINT32 width, unsigned char * output);

// Text-related functions (Display.c)
void Initialize_Global_Printf_Defaults(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU);

void single_char(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, int character, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color);
void single_char_anywhere(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, int character, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y);
void single_char_anywhere_scaled(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, int character, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale);

void string_anywhere_scaled(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, const char * string, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale);
void formatted_string_anywhere_scaled(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale, const char * string, ...);
void Output_render_text(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, int character, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale, UINT32 index);

// Printf-related functions (Print.c)
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int vsnrprintf(char *str, size_t size, int radix, const char *format, va_list ap);
int sprintf(char *buf, const char *cfmt, ...);
int vsprintf(char *buf, const char *cfmt, va_list ap);

int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int kvprintf(char const *fmt, void (*func)(int, void*), void *arg, int radix, va_list ap);

void print_utf16_as_utf8(CHAR16 * strung, UINT64 size);
char * UCS2_to_UTF8(CHAR16 * strang, UINT64 size);

// Don't remove this #endif
#endif /* _Kernel64_H */
