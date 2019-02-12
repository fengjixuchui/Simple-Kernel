//==================================================================================================================================
//  Simple Kernel: Main Header
//==================================================================================================================================
//
// Version 0.y
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

/*
In freestanding mode, the only available standard header files are: <float.h>,
<iso646.h>, <limits.h>, <stdarg.h>, <stdbool.h>, <stddef.h>, and <stdint.h>
(C99 standard 4.6).
*/

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <float.h>
#include <stdarg.h>

#include <cpuid.h> // ...But we have this, too.

#include "EfiBind.h"
#include "EfiTypes.h"
#include "EfiError.h"
#include "avxmem.h"

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

#define NextMemoryDescriptor(Ptr,Size)  ((EFI_MEMORY_DESCRIPTOR *) (((UINT8 *) Ptr) + Size))

typedef
EFI_STATUS
(EFIAPI *EFI_SET_VIRTUAL_ADDRESS_MAP) (
    IN UINTN                        MemoryMapSize,
    IN UINTN                        DescriptorSize,
    IN UINT32                       DescriptorVersion,
    IN EFI_MEMORY_DESCRIPTOR        *VirtualMap
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


typedef struct {
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  *GPUArray; // This array contains the EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE structures for each available framebuffer
  UINT64                              NumberOfFrameBuffers; // The number of pointers in the array (== the number of available framebuffers)
} GPU_CONFIG;

// LP->GPU_Configs->GPU_MODE.FrameBufferBase
// LP->GPU_Configs->GPU_MODE.Info->HorizontalResolution
// GPU_MODE == GPUArray[0,1,2...NumberOfFrameBuffers]

// #define GTX1080 LP->GPU_Configs->GPUArray[0]
// GTX1080.FrameBufferBase
// GTX1080.Info->HorizontalResolution

typedef struct {
  EFI_MEMORY_DESCRIPTOR  *Memory_Map;
  EFI_RUNTIME_SERVICES   *RTServices;
  GPU_CONFIG             *GPU_Configs;
  EFI_FILE_INFO          *FileMeta;
  void                   *RSDP;
} LOADER_PARAMS;

//
// Anything below this line (except the #endif at the very bottom) is safe to
// remove without breaking compatibility with the Simple UEFI Bootloader. Useful
// if you wanted to make your own kernel from scratch.
//

#define systemfont font8x8_basic // Must be set up in UTF-8

typedef struct {
	EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  defaultGPU;
	UINT32                             height; // Character font height
	UINT32                             width; // Character font width (in bits)
	UINT32                             font_color; // Default font color
	UINT32                             highlight_color; // Default highlight color
  UINT32                             background_color; // Default background color
	UINT32                             x; // Leftmost x-coord that's in-bounds (NOTE: per UEFI Spec 2.7 Errata A, (0,0) is always the top left in-bounds pixel.)
	UINT32                             y; // Topmost y-coord
	UINT32                             scale; // Output scale for systemfont used by printf
  UINT32                             index; // Global string index for printf, etc. to keep track of cursor's postion in the framebuffer
  UINT32                             textscrollmode; // What to do when a newline goes off the bottom of the screen: 0 = scroll entire screen, 1 = wrap around to the top
} GLOBAL_PRINT_INFO_STRUCT;

GLOBAL_PRINT_INFO_STRUCT Global_Print_Info;

// Intel Architecture Manual Vol. 3A, Fig. 3-11 (Pseudo-Descriptor Formats)
typedef struct __attribute__ ((packed)) {
  UINT16 Limit; // Limit + 1 = size, since limit + base = the last valid address
  UINT64 BaseAddress;
} DT_STRUCT; // GDT, IDT, LDT all use this format

// Intel Architecture Manual Vol. 3A, Fig. 3-8 (Segment Descriptor)
typedef struct __attribute__ ((packed)) {
  UINT16 SegmentLimit1; // Low bits, SegmentLimit2andMisc has MSBs (it's a 20-bit value)
  UINT16 BaseAddress1; // Low bits
  UINT8  BaseAddress2; // Next bits
  UINT8  Misc1; // Bits 0-3: segment/gate Type, 4: S, 5-6: DPL, 7: P
  UINT8  SegmentLimit2andMisc; // Bits 0-3: seglimit2, 4: Available, 5: L, 6: D/B, 7: G
  UINT8  BaseAddress3; // Most significant bits
} GDT_ENTRY_STRUCT; // This whole thing can fit in a 64-bit int. Printf %lx on SegmentLimit1 gives the whole thing.

// Initialization-related functions
void Initialize_Global_Printf_Defaults(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU);
void cpu_features(uint64_t rax_value, uint64_t rcx_value);
uint64_t get_tick(void);
void enable_AVX(void);
void enable_interrupts(void);
uint64_t control_register_rw(int crX, uint64_t in_out, int rw);
uint64_t msr_rw(uint64_t msr, uint64_t data, int rw);
uint64_t xcr_rw(uint64_t xcr, uint64_t data, int rw);
uint64_t read_cs(void);
DT_STRUCT get_gdtr(void);

// Drawing-related functions
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

// Text-related functions
void single_char(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, int character, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color);
void single_char_anywhere(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, int character, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y);
void single_char_anywhere_scaled(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, int character, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale);

void string_anywhere_scaled(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, const char * string, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale);
void formatted_string_anywhere_scaled(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale, const char * string, ...);
void Output_render_text(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, int character, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale, UINT32 index);

// Printf-related functions
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int vsnrprintf(char *str, size_t size, int radix, const char *format, va_list ap);
int sprintf(char *buf, const char *cfmt, ...);
int vsprintf(char *buf, const char *cfmt, va_list ap);

int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int kvprintf(char const *fmt, void (*func)(int, void*), void *arg, int radix, va_list ap);

// Don't remove this #endif
#endif /* _Kernel64_H */
