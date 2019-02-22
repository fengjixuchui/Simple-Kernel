//==================================================================================================================================
//  Simple Kernel: Text and Graphics Display Output Functions
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
// This file provides various functions for text and graphics output.
//

#include "Kernel64.h"
#include "font8x8.h" // This can only be included by one file since it's h-files containing initialized variables (in this case arrays)

// Set the default font with this
#define SYSTEMFONT font8x8_basic // Must be set up in UTF-8

//----------------------------------------------------------------------------------------------------------------------------------
// Initialize_Global_Printf_Defaults: Set Up Printf
//----------------------------------------------------------------------------------------------------------------------------------
//
// Initialize printf and bind it to a specific GPU framebuffer.
//

void Initialize_Global_Printf_Defaults(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU)
{
  // Set global default print information--needed for printf
  Global_Print_Info.defaultGPU = GPU;
  Global_Print_Info.height = 8; // Character font height (height*scale should not exceed VerticalResolution--it should still work, but it might be really messy and bizarrely cut off)
  Global_Print_Info.width = 8; // Character font width (in bits) (width*scale should not exceed HorizontalResolution, same reason as above)
  Global_Print_Info.font_color = 0x00FFFFFF; // Default font color -- TODO: Use EFI Pixel Info
  Global_Print_Info.highlight_color = 0x00000000; // Default highlight color
  Global_Print_Info.background_color = 0x00000000; // Default background color
  Global_Print_Info.x = 0; // Leftmost x-coord that's in-bounds (NOTE: per UEFI Spec 2.7 Errata A, (0,0) is always the top left in-bounds pixel.)
  Global_Print_Info.y = 0; // Topmost y-coord
  Global_Print_Info.scale = 1; // Output scale for systemfont used by printf (positive integer scaling only, default 1 = no scaling)
  Global_Print_Info.index = 0; // Global string index for printf, etc. to keep track of cursor's postion in the framebuffer
  Global_Print_Info.textscrollmode = 0; // What to do when a newline goes off the bottom of the screen. See next comment for values.
  //
  // textscrollmode:
  //  0 = wrap around to the top (default)
  //  1 up to VerticalResolution - 1 = Scroll this many vertical lines at a time
  //                                   (NOTE: Gaps between text lines will occur
  //                                   if this is not an integer factor of the
  //                                   space below the lowest character, except
  //                                   for special cases below)
  //  VerticalResolution = Maximum supported value, will simply wipe the screen.
  //
  //  Special cases:
  //    - Using height*scale gives a "quick scroll" for text, as it scrolls up an
  //      entire character at once (recommended mode for scrolling). This special
  //      case does not leave gaps between text lines when using arbitrary font
  //      sizes/scales.
  //    - Using VerticalResolution will just quickly wipe the screen and print
  //      the next character on top where it would have gone next anyways.
  //
  // SMOOTH TEXT SCROLLING WARNING:
  // The higher the screen resolution and the larger the font size + scaling, the
  // slower low values are. Using 1 on a 4K screen takes almost exactly 30
  // seconds to scroll up a 120-height character on an i7-7700HQ, but man it's
  // smooooooth. Using 2 takes half the time, etc.
  //
}

//----------------------------------------------------------------------------------------------------------------------------------
// formatted_string_anywhere_scaled: A More Flexible Printf
//----------------------------------------------------------------------------------------------------------------------------------
//
// A massively customizable printf-like function. Supports everything printf supports. Not bound to any particular GPU.
//
// height and width: height (bytes) and width (bits) of the string's font characters; there is no automatic way of getting this information for weird font sizes (e.g. 17 bits wide), sorry.
// font_color: font color
// highlight_color: highlight/background color for the string's characters (it's called highlight color in word processors)
// x and y: coordinate positions of the top leftmost pixel of the string
// scale: integer font scaling factor >= 1
// string: printf-style string
//

void formatted_string_anywhere_scaled(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale, const char * string, ...)
{
  // Height in number of bytes and width in number of bits, per character where "character" is an array of bytes, e.g. { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, which is U+0040 (@). This is an 8x8 '@' sign.
  if(height > GPU.Info->VerticalResolution || width > GPU.Info->HorizontalResolution)
  {
    Colorscreen(GPU, 0x00FF0000); // Need some kind of error indicator (makes screen red)
  } // Could use an instruction like ARM's USAT to truncate values
  else if(x > GPU.Info->HorizontalResolution || y > GPU.Info->VerticalResolution)
  {
    Colorscreen(GPU, 0x0000FF00); // Makes screen green
  }
  else if ((y + scale*height) > GPU.Info->VerticalResolution || (x + scale*width) > GPU.Info->HorizontalResolution)
  {
    Colorscreen(GPU, 0x000000FF); // Makes screen blue
  }
  va_list arguments;

  va_start(arguments, string);
  ssize_t buffersize = vsnprintf(NULL, 0, string, arguments); // Get size of needed buffer
  char output_string[buffersize + 1]; // (v)snprintf does not account for \0
  vsprintf(output_string, string, arguments); // Write string to buffer
  va_end(arguments);

  string_anywhere_scaled(GPU, output_string, height, width, font_color, highlight_color, x, y, scale);
}

//----------------------------------------------------------------------------------------------------------------------------------
// Resetdefaultscreen: Reset Printf Cursor and Black Screen
//----------------------------------------------------------------------------------------------------------------------------------
//
// Reset Printf cursor to (0,0) and wipe the visible portion of the screen buffer to black
//

void Resetdefaultscreen(void)
{
  Global_Print_Info.x = 0;
  Global_Print_Info.y = 0;
  Global_Print_Info.index = 0;
  Blackscreen(Global_Print_Info.defaultGPU);
}

//----------------------------------------------------------------------------------------------------------------------------------
// Resetdefaultcolorscreen: Reset Printf Cursor and Color Screen
//----------------------------------------------------------------------------------------------------------------------------------
//
// Reset Printf cursor to (0,0) and wipe the visible portiion of the screen buffer area to the default background color (whatever is
// currently set as Global_Print_Info.background_color)
//

void Resetdefaultcolorscreen(void)
{
  Global_Print_Info.x = 0;
  Global_Print_Info.y = 0;
  Global_Print_Info.index = 0;
  Colorscreen(Global_Print_Info.defaultGPU, Global_Print_Info.background_color);
}

//----------------------------------------------------------------------------------------------------------------------------------
// Blackscreen: Make the Screen Black
//----------------------------------------------------------------------------------------------------------------------------------
//
// Wipe the visible portion of the screen buffer to black
//

void Blackscreen(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU)
{
  Colorscreen(GPU, 0x00000000);
}

//----------------------------------------------------------------------------------------------------------------------------------
// Colorscreen: Make the Screen a Color
//----------------------------------------------------------------------------------------------------------------------------------
//
// Wipe the visible portion of the screen buffer to a specified color
//

void Colorscreen(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 color)
{
  Global_Print_Info.background_color = color;

  AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)GPU.FrameBufferBase, color, GPU.Info->VerticalResolution * GPU.Info->PixelsPerScanLine);
/*  // This could work, too, if writing to the offscreen area is undesired. It'll probably be a little slower than a contiguous AVX_memset_4B, however.
  for (row = 0; row < GPU.Info->VerticalResolution; row++)
  {
    // Per UEFI Spec 2.7 Errata A, framebuffer address 0 coincides with the top leftmost pixel. i.e. screen padding is only HorizontalResolution + porch.
    AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(GPU.FrameBufferBase + 4 * GPU.Info->PixelsPerScanLine * row), color, GPU.Info->HorizontalResolution); // The thing at FrameBufferBase is an address pointing to UINT32s. FrameBufferBase itself is a 64-bit number.
  }
*/

/* // Old version (non-AVX)
  UINT32 row, col;
  UINT32 backporch = GPU.Info->PixelsPerScanLine - GPU.Info->HorizontalResolution; // The area offscreen is the back porch. Sometimes it's 0.

  for (row = 0; row < GPU.Info->VerticalResolution; row++)
  {
    for (col = 0; col < (GPU.Info->PixelsPerScanLine - backporch); col++) // Per UEFI Spec 2.7 Errata A, framebuffer address 0 coincides with the top leftmost pixel. i.e. screen padding is only HorizontalResolution + porch.
    {
      *(UINT32*)(GPU.FrameBufferBase + 4 * (GPU.Info->PixelsPerScanLine * row + col)) = color; // The thing at FrameBufferBase is an address pointing to UINT32s. FrameBufferBase itself is a 64-bit number.
    }
  }
*/

/* // Leaving this here for posterity. The framebuffer size might be a fair bit larger than the visible area (possibly for scrolling support? Regardless, some are just the size of the native resolution).
  for(UINTN i = 0; i < GPU.FrameBufferSize; i+=4) //32 bpp == 4 Bpp
  {
    *(UINT32*)(GPU.FrameBufferBase + i) = color; // FrameBufferBase is a 64-bit address that points to UINT32s.
  }
*/
}

//----------------------------------------------------------------------------------------------------------------------------------
// single_pixel: Color a Single Pixel
//----------------------------------------------------------------------------------------------------------------------------------
//
// Set a specified pixel, in (x,y) coordinates from the top left of the screen (0,0), to a specified color
//

// Screen turns red if a pixel is put outside the visible area.
void single_pixel(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 x, UINT32 y, UINT32 color)
{
  if(y > GPU.Info->VerticalResolution || x > GPU.Info->HorizontalResolution) // Need some kind of error indicator (makes screen red)
  {
    Colorscreen(GPU, 0x00FF0000);
  }

  *(UINT32*)(GPU.FrameBufferBase + (y * GPU.Info->PixelsPerScanLine + x) * 4) = color;
//  Output_render(GPU, 0x01, 1, 1, color, 0xFF000000, x, y, 1, 0); // Make highlight transparent to skip that part of output render (transparent = no highlight)
}

//----------------------------------------------------------------------------------------------------------------------------------
// single_char: Color a Single Character
//----------------------------------------------------------------------------------------------------------------------------------
//
// Print a single character of the default font (set at the top of this file) at the top left of the screen (0,0) and at a specified
// font color and highlight color
//
// character: 'a' or 'b' (with single quotes), for example
// height and width: height (bytes) and width (bits) of the font character; there is no automatic way of getting this information for weird font sizes (e.g. 17 bits wide), sorry.
// font_color: font color
// highlight_color: highlight/background color for the string's characters (it's called highlight color in word processors)
//

void single_char(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, int character, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color)
{
  // Assuming "character" is an array of bytes, e.g. { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, which would be U+0040 (@) -- an 8x8 '@' sign.
  if(height > GPU.Info->VerticalResolution || width > GPU.Info->HorizontalResolution)
  {
    Colorscreen(GPU, 0x00FF0000); // Need some kind of error indicator (makes screen red)
  } // Could use an instruction like ARM's USAT to truncate values

  Output_render_text(GPU, character, height, width, font_color, highlight_color, 0, 0, 1, 0);
}

//----------------------------------------------------------------------------------------------------------------------------------
// single_char_anywhere: Color a Single Character Anywhere
//----------------------------------------------------------------------------------------------------------------------------------
//
// Print a single character of the default font (set at the top of this file), at (x,y) coordinates from the top left of the screen
// (0,0), using a specified font color and highlight color
//
// character: 'a' or 'b' (with single quotes), for example
// height and width: height (bytes) and width (bits) of the font character; there is no automatic way of getting this information for weird font sizes (e.g. 17 bits wide), sorry.
// font_color: font color
// highlight_color: highlight/background color for the string's characters (it's called highlight color in word processors)
// x and y: coordinate positions of the top leftmost pixel of the string
//

void single_char_anywhere(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, int character, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y)
{
  // Assuming "character" is an array of bytes, e.g. { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, which would be U+0040 (@) -- an 8x8 '@' sign.
  if(height > GPU.Info->VerticalResolution || width > GPU.Info->HorizontalResolution)
  {
    Colorscreen(GPU, 0x00FF0000); // Need some kind of error indicator (makes screen red)
  } // Could use an instruction like ARM's USAT to truncate values
  else if(x > GPU.Info->HorizontalResolution || y > GPU.Info->VerticalResolution)
  {
    Colorscreen(GPU, 0x0000FF00); // Makes screen green
  }
  else if ((y + height) > GPU.Info->VerticalResolution || (x + width) > GPU.Info->HorizontalResolution)
  {
    Colorscreen(GPU, 0x000000FF); // Makes screen blue
  }

  Output_render_text(GPU, character, height, width, font_color, highlight_color, x, y, 1, 0);
}

//----------------------------------------------------------------------------------------------------------------------------------
// single_char_anywhere_scaled: Color a Single Character Anywhere with Scaling
//----------------------------------------------------------------------------------------------------------------------------------
//
// Print a single character of the default font (set at the top of this file), at (x,y) coordinates from the top left of the screen
// (0,0), using a specified font color, highlight color, and scale factor
//
// character: 'a' or 'b' (with single quotes), for example
// height and width: height (bytes) and width (bits) of the font character; there is no automatic way of getting this information for weird font sizes (e.g. 17 bits wide), sorry.
// font_color: font color
// highlight_color: highlight/background color for the string's characters (it's called highlight color in word processors)
// x and y: coordinate positions of the top leftmost pixel of the string
// scale: integer font scaling factor >= 1
//

void single_char_anywhere_scaled(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, int character, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale)
{
  // Assuming "character" is an array of bytes, e.g. { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, which would be U+0040 (@) -- an 8x8 '@' sign.
  if(height > GPU.Info->VerticalResolution || width > GPU.Info->HorizontalResolution)
  {
    Colorscreen(GPU, 0x00FF0000); // Need some kind of error indicator (makes screen red)
  } // Could use an instruction like ARM's USAT to truncate values
  else if(x > GPU.Info->HorizontalResolution || y > GPU.Info->VerticalResolution)
  {
    Colorscreen(GPU, 0x0000FF00); // Makes screen green
  }
  else if ((y + scale*height) > GPU.Info->VerticalResolution || (x + scale*width) > GPU.Info->HorizontalResolution)
  {
    Colorscreen(GPU, 0x000000FF); // Makes screen blue
  }

  Output_render_text(GPU, character, height, width, font_color, highlight_color, x, y, scale, 0);
}

//----------------------------------------------------------------------------------------------------------------------------------
// string_anywhere_scaled: Color a String Anywhere with Scaling
//----------------------------------------------------------------------------------------------------------------------------------
//
// Print a string of the default font (set at the top of this file), at (x,y) coordinates from the top left of the screen
// (0,0), using a specified font color, highlight color, and scale factor
//
// string: "a string like this" (no formatting specifiers -- use formatted_string_anywhere_scaled() for that instead)
// height and width: height (bytes) and width (bits) of the string's font characters; there is no automatic way of getting this information for weird font sizes (e.g. 17 bits wide), sorry.
// font_color: font color
// highlight_color: highlight/background color for the string's characters (it's called highlight color in word processors)
// x and y: coordinate positions of the top leftmost pixel of the string
// scale: integer font scaling factor >= 1
//
// NOTE: literal strings in C are automatically null-terminated. i.e. "hi" is actually 3 characters long.
// This function allows direct output of a pre-made string, either a hardcoded one or one made via sprintf.
//

void string_anywhere_scaled(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, const char * string, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale)
{
  if(height > GPU.Info->VerticalResolution || width > GPU.Info->HorizontalResolution)
  {
    Colorscreen(GPU, 0x00FF0000); // Need some kind of error indicator (makes screen red)
  } // Could use an instruction like ARM's USAT to truncate values
  else if(x > GPU.Info->HorizontalResolution || y > GPU.Info->VerticalResolution)
  {
    Colorscreen(GPU, 0x0000FF00); // Makes screen green
  }
  else if ((y + scale*height) > GPU.Info->VerticalResolution || (x + scale*width) > GPU.Info->HorizontalResolution)
  {
    Colorscreen(GPU, 0x000000FF); // Makes screen blue
  }

  //mapping: x*4 + y*4*(PixelsPerScanLine), x is column number, y is row number; every 4*PixelsPerScanLine bytes is a new row
  // y max is VerticalResolution, x max is HorizontalResolution, 'x4' is the LFB expansion, since 1 bit input maps to 4 bytes output

  // A 2x scale should turn 1 pixel into a square of 2x2 pixels
// iterate through characters in string
// start while
  uint32_t index = 0;
  while(string[index] != '\0') // for char in string until \0
  {
    // match the character to the font, using UTF-8.
    // This would expect a font array to include everything in the UTF-8 character set... Or just use the most common ones.
    Output_render_text(GPU, string[index], height, width, font_color, highlight_color, x, y, scale, index);
    index++;
  } // end while
} // end function

//----------------------------------------------------------------------------------------------------------------------------------
// Output_render_text: Render a Character to the Screen
//----------------------------------------------------------------------------------------------------------------------------------
//
// This function draws a character of the default font on the screen, given the following parameters:
//
// character: 'a' or 'b' (with single quotes), for example
// height and width: height (bytes) and width (bits) of the font character; there is no automatic way of getting this information for weird font sizes (e.g. 17 bits wide), sorry.
// font_color: font color
// highlight_color: highlight/background color for the string's characters (it's called highlight color in word processors)
// x and y: coordinate positions of the top leftmost pixel of the string
// scale: integer font scaling factor >= 1
// index: mainly for strings, it's for keeping track of which character in the string is being output
//

void Output_render_text(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, int character, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale, UINT32 index)
{
  // Compact ceiling function, so that size doesn't need to be passed in
  // This should be faster than a divide followed by a mod
  uint32_t row_iterator = width >> 3;
  if((width & 0x7) != 0)
  {
    row_iterator++;
  } // Width should never be zero, so the iterator will always be at least 1
  uint32_t i;

  for(uint32_t row = 0; row < height; row++) // for number of rows in the character of the fontarray
  {
    i = 0;
    for (uint32_t bit = 0; bit < width; bit++) // for bit in column
    {
      if(((bit & 0x7) == 0 ) && (bit > 0))
      {
        i++;
      }
      // if a 1, output NOTE: There's gotta be a better way to do this.
//      if( character[row*row_iterator + i] & (1 << (7 - (bit & 0x7))) )
      if((SYSTEMFONT[character][row*row_iterator + i] >> (bit & 0x7)) & 0x1)
      {
        // start scale here
        for(uint32_t b = 0; b < scale; b++)
        {
          for(uint32_t a = 0; a < scale; a++)
          {
            *(UINT32*)(GPU.FrameBufferBase + ((y*GPU.Info->PixelsPerScanLine + x) + scale*(row*GPU.Info->PixelsPerScanLine + bit) + (b*GPU.Info->PixelsPerScanLine + a) + scale * index * width)*4) = font_color;
          }
        } //end scale here
      } //end if
      else // if a 0, background or skip (set highlight_color to 0xFF000000 for transparency)
      {
        // start scale here
        for(uint32_t b = 0; b < scale; b++)
        {
          for(uint32_t a = 0; a < scale; a++)
          {
            if(highlight_color != 0xFF000000) // Transparency "color"
            {
              *(UINT32*)(GPU.FrameBufferBase + ((y*GPU.Info->PixelsPerScanLine + x) + scale*(row*GPU.Info->PixelsPerScanLine + bit) + (b*GPU.Info->PixelsPerScanLine + a) + scale * index * width)*4) = highlight_color;
            }
          }
        } //end scale here
      } // end else
    } // end bit in column
  } // end byte in row
}

//----------------------------------------------------------------------------------------------------------------------------------
// bitmap_anywhere_scaled: Color a Single Bitmap Anywhere with Scaling
//----------------------------------------------------------------------------------------------------------------------------------
//
// Print a single, single-color bitmapped character at (x,y) coordinates from the top left of the screen (0,0) using a specified font
// color, highlight color, and scale factor
//
// bitmap: a bitmapped image formatted like a font character
// height and width: height (bytes) and width (bits) of the bitmap; there is no automatic way of getting this information for weird font sizes (e.g. 17 bits wide), sorry.
// font_color: font color
// highlight_color: highlight/background color for the string's characters (it's called highlight color in word processors)
// x and y: coordinate positions of the top leftmost pixel of the string
// scale: font scale factor
//
// This function allows for printing of individual characters not in the default font, making it like single_char_anywhere, but for
// non-font characters and similarly-formatted images. Just pass in an appropriately-formatted array of bytes as the "bitmap" pointer.
//
// Note that single_char_anywhere_scaled() takes 'a' or 'b', this would take something like character_array['a'] instead.
//

void bitmap_anywhere_scaled(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, const unsigned char * bitmap, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale)
{
  // Assuming "bitmap" is an array of bytes, e.g. { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, which would be U+0040 (@) -- an 8x8 '@' sign.
  if(height > GPU.Info->VerticalResolution || width > GPU.Info->HorizontalResolution)
  {
    Colorscreen(GPU, 0x00FF0000); // Need some kind of error indicator (makes screen red)
  } // Could use an instruction like ARM's USAT to truncate values
  else if(x > GPU.Info->HorizontalResolution || y > GPU.Info->VerticalResolution)
  {
    Colorscreen(GPU, 0x0000FF00); // Makes screen green
  }
  else if ((y + scale*height) > GPU.Info->VerticalResolution || (x + scale*width) > GPU.Info->HorizontalResolution)
  {
    Colorscreen(GPU, 0x000000FF); // Makes screen blue
  }

  Output_render_bitmap(GPU, bitmap, height, width, font_color, highlight_color, x, y, scale, 0);
}


//----------------------------------------------------------------------------------------------------------------------------------
// Output_render_bitmap: Render a Single-Color Bitmap to the Screen
//----------------------------------------------------------------------------------------------------------------------------------
//
// This function draws a bitmapped character, given the following parameters:
//
// bitmap: a bitmapped image formatted like a font character
// height and width: height (bytes) and width (bits) of the bitmap; there is no automatic way of getting this information for weird font sizes (e.g. 17 bits wide), sorry.
// font_color: font color
// highlight_color: highlight/background color for the string's characters (it's called highlight color in word processors)
// x and y: coordinate positions of the top leftmost pixel of the string
// scale: integer font scaling factor >= 1
// index: mainly for strings, it's for keeping track of which character in the string is being output
//
// This is essentially the same thing as Output_render_text(), but for bitmaps that are not part of the default font.
//
// Note that single_char_anywhere_scaled() takes 'a' or 'b', this would take something like character_array['a'] instead.
//

void Output_render_bitmap(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, const unsigned char * bitmap, UINT32 height, UINT32 width, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale, UINT32 index)
{
  // Compact ceiling function, so that size doesn't need to be passed in
  // This should be faster than a divide followed by a mod
  uint32_t row_iterator = width >> 3;
  if((width & 0x7) != 0)
  {
    row_iterator++;
  } // Width should never be zero, so the iterator will always be at least 1
  uint32_t i;

  for(uint32_t row = 0; row < height; row++) // for number of rows in the character of the fontarray
  {
    i = 0;
    for (uint32_t bit = 0; bit < width; bit++) // for bit in column
    {
      if(((bit & 0x7) == 0 ) && (bit > 0))
      {
        i++;
      }
      // if a 1, output NOTE: There's gotta be a better way to do this.
//      if( character[row*row_iterator + i] & (1 << (7 - (bit & 0x7))) )
      if((bitmap[row*row_iterator + i] >> (bit & 0x7)) & 0x1) // This is pretty much the only difference from Output_render_text
      {
        // start scale here
        for(uint32_t b = 0; b < scale; b++)
        {
          for(uint32_t a = 0; a < scale; a++)
          {
            *(UINT32*)(GPU.FrameBufferBase + ((y*GPU.Info->PixelsPerScanLine + x) + scale*(row*GPU.Info->PixelsPerScanLine + bit) + (b*GPU.Info->PixelsPerScanLine + a) + scale * index * width)*4) = font_color;
          }
        } //end scale here
      } //end if
      else // if a 0, background or skip (set highlight_color to 0xFF000000 for transparency)
      {
        // start scale here
        for(uint32_t b = 0; b < scale; b++)
        {
          for(uint32_t a = 0; a < scale; a++)
          {
            if(highlight_color != 0xFF000000) // Transparency "color"
            {
              *(UINT32*)(GPU.FrameBufferBase + ((y*GPU.Info->PixelsPerScanLine + x) + scale*(row*GPU.Info->PixelsPerScanLine + bit) + (b*GPU.Info->PixelsPerScanLine + a) + scale * index * width)*4) = highlight_color;
            }
          }
        } //end scale here
      } // end else
    } // end bit in column
  } // end byte in row
}

// TODO: Finish/optimize bitmap renderers first.
// Maybe combine output renders into one with a 't' 'b' or 'v' mode.
// This could use AVX
/*
void Output_render_vector(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 x_init, UINT32 y_init, UINT32 x_final, UINT32 y_final, UINT32 color, UINT32 scale)
{

}
*/

//----------------------------------------------------------------------------------------------------------------------------------
// bitmap_bitswap: Swap Bitmap Bits
//----------------------------------------------------------------------------------------------------------------------------------
//
// Swaps the high 4 bits with the low 4 bits in each byte of an array
//
// bitmap: an array of bytes
// height and width: height (bytes) and width (bits) of the bitmap; there is no automatic way of getting this information for weird font sizes (e.g. 17 bits wide), sorry.
//

void bitmap_bitswap(const unsigned char * bitmap, UINT32 height, UINT32 width, unsigned char * output)
{
  uint32_t row_iterator = width >> 3;
  if((width & 0x7) != 0)
  {
    row_iterator++;
  } // Width should never be zero, so the iterator will always be at least 1

  for(uint32_t iter = 0; iter < height*row_iterator; iter++) // Flip one byte at a time
  {
    output[iter] = (((bitmap[iter] >> 4) & 0xF) | ((bitmap[iter] & 0xF) << 4));
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
// bitmap_bitreverse: Reverse Bitmap Bits
//----------------------------------------------------------------------------------------------------------------------------------
//
// Inverts each individual byte in an array: 01234567 --> 76543210
// It reverses the order of bits in each byte of an array, but it does not reorder any bytes. This does not change endianness, as
// changing endianness would be reversing the order of bytes in a given data type like uint64_t.
//
// bitmap: an array of bytes
// height and width: height (bytes) and width (bits) of the bitmap; there is no automatic way of getting this information for weird font sizes (e.g. 17 bits wide), sorry.
//

void bitmap_bitreverse(const unsigned char * bitmap, UINT32 height, UINT32 width, unsigned char * output)
{
  uint32_t row_iterator = width >> 3;
  if((width & 0x7) != 0)
  {
    row_iterator++;
  } // Width should never be zero, so the iterator will always be at least 1

  for(uint32_t iter = 0; iter < height*row_iterator; iter++) // Invert one byte at a time
  {
    output[iter] = 0;
    for(uint32_t bit = 0; bit < 8; bit++)
    {
      if( bitmap[iter] & (1 << (7 - bit)) )
      {
        output[iter] += (1 << bit);
      }
    }
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
// bitmap_bytemirror: Mirror a Rectangular Array of Bytes
//----------------------------------------------------------------------------------------------------------------------------------
//
// Requires rectangular arrays, and it creates a horizontal reflection of the entire array (like looking in a mirror)
//
// bitmap: a rectangular array of bytes
// height and width: height (bytes) and width (bits) of the bitmap; there is no automatic way of getting this information for weird font sizes (e.g. 17 bits wide), sorry.
//

void bitmap_bytemirror(const unsigned char * bitmap, UINT32 height, UINT32 width, unsigned char * output) // Width in bits, height in bytes
{
  uint32_t row_iterator = width >> 3;
  if((width & 0x7) != 0)
  {
    row_iterator++;
  } // Width should never be zero, so the iterator will always be at least 1

  uint32_t iter, parallel_iter;
  if(row_iterator & 0x01)
  {// Odd number of bytes per row
    for(iter = 0, parallel_iter = row_iterator - 1; (iter + (row_iterator >> 1) + 1) < height*row_iterator; iter++, parallel_iter--) // Mirror one byte at a time
    {
      if(iter == parallel_iter) // Integer divide, (iter%row_iterator == row_iterator/2 - 1)
      {
        iter += (row_iterator >> 1) + 1; // Hop the middle byte
        parallel_iter = iter + row_iterator - 1;
      }

      output[iter] = bitmap[parallel_iter]; // parallel_iter must mirror iter
      output[parallel_iter] = bitmap[iter];
    }
  }
  else
  {// Even number of bytes per row
    for(iter = 0, parallel_iter = row_iterator - 1; (iter + (row_iterator >> 1)) < height*row_iterator; iter++, parallel_iter--) // Mirror one byte at a time
    {
      if(iter - 1 == parallel_iter) // Integer divide, (iter%row_iterator == row_iterator/2 - 1)
      {
        iter += (row_iterator >> 1); // Skip to the next row to swap
        parallel_iter = iter + row_iterator - 1; // Appropriately position parallel_iter based on iter
      }

      output[iter] = bitmap[parallel_iter]; // Parallel_iter must mirror iter
      output[parallel_iter] = bitmap[iter];
    }
  }
}
