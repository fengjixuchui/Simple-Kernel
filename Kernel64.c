
#include "Kernel64.h"

#define fontarray font8x8_basic // Must be set up in UTF-8

// The only data we can work with is what is passed into this function, plus the few std includes in the .h file.
// To print text requires a bitmap font.
//void kernel_main(EFI_MEMORY_DESCRIPTOR * Memory_Map, EFI_RUNTIME_SERVICES * RTServices, EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE * GPU_Mode, EFI_FILE_INFO * FileMeta, void * RSDP)
void kernel_main(LOADER_PARAMS * LP) // Loader Parameters
{
  /*
    typedef struct PARAMETER_BLOCK {
      EFI_MEMORY_DESCRIPTOR               *Memory_Map;
      EFI_RUNTIME_SERVICES                *RTServices;
      EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE   *GPU_Mode;
      EFI_FILE_INFO                       *FileMeta;
      void                                *RSDP;
    } LOADER_PARAMS;
  */
//  int wait = 1;
  uint32_t iterator = 1;
//  drawTriangle( LP->GPU_Mode->FrameBufferBase, LP->GPU_Mode->Info->HorizontalResolution / 2, LP->GPU_Mode->Info->VerticalResolution / 2 - 25, 100, 0x00ff99ff, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution );
  single_char_anywhere_scaled(LP->GPU_Mode->FrameBufferBase, fontarray['@'], 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x000000FF, 0xFF000000, (LP->GPU_Mode->Info->HorizontalResolution - 10*8)/2, (LP->GPU_Mode->Info->VerticalResolution - 10*8)/2, 10);

  while(iterator) // loop ends on overflow
  {
//    asm volatile ("pause");
    asm volatile ("nop");
    iterator++; // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
  }
  iterator = 1;

  Colorscreen(LP->GPU_Mode->FrameBufferBase, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x000000FF); // Blue in BGRX (X = reserved, technically an "empty alpha channel" for 32-bit memory alignment)
  single_char(LP->GPU_Mode->FrameBufferBase, fontarray['?'], 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF, 0x00000000);
  single_char_anywhere(LP->GPU_Mode->FrameBufferBase, fontarray['!'], 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF, 0xFF000000, LP->GPU_Mode->Info->HorizontalResolution/4, LP->GPU_Mode->Info->VerticalResolution/3);
  single_char_anywhere_scaled(LP->GPU_Mode->FrameBufferBase, fontarray['H'], 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF, 0xFF000000, 10, 10, 5);
  string_anywhere_scaled(LP->GPU_Mode->FrameBufferBase, "Is it soup?", 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF, 0x00000000, 10, 10, 1);
  while(iterator) // loop ends on overflow
  {
//    asm volatile ("pause");
    asm volatile ("nop");
    iterator++; // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
  }
  iterator = 1;

  Colorscreen(LP->GPU_Mode->FrameBufferBase, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x0000FF00); // Green in BGRX (X = reserved, technically an "empty alpha channel" for 32-bit memory alignment)
  single_char(LP->GPU_Mode->FrameBufferBase, fontarray['A'], 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF, 0x00000000);
  single_char_anywhere(LP->GPU_Mode->FrameBufferBase, fontarray['!'], 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF, 0xFF000000, LP->GPU_Mode->Info->HorizontalResolution/4, LP->GPU_Mode->Info->VerticalResolution/3);
  string_anywhere_scaled(LP->GPU_Mode->FrameBufferBase, "Is it really soup?", 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF, 0x00000000, 50, 50, 3);
  while(iterator) // loop ends on overflow
  {
//    asm volatile ("pause");
    asm volatile ("nop");
    iterator++; // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
  }
  iterator = 1;

  Colorscreen(LP->GPU_Mode->FrameBufferBase, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FF0000); // Red in BGRX (X = reserved, technically an "empty alpha channel" for 32-bit memory alignment)
  single_char(LP->GPU_Mode->FrameBufferBase, fontarray['2'], 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF, 0xFF000000);
  while(iterator) // loop ends on overflow
  {
//    asm volatile ("pause");
    asm volatile ("nop");
    iterator++; // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
  }
  iterator = 1;

  Blackscreen(LP->GPU_Mode->FrameBufferBase, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution); // X in BGRX (X = reserved, technically an "empty alpha channel" for 32-bit memory alignment)
  single_pixel(LP->GPU_Mode->FrameBufferBase, LP->GPU_Mode->Info->HorizontalResolution / 2, LP->GPU_Mode->Info->VerticalResolution / 2, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF);
  single_char(LP->GPU_Mode->FrameBufferBase, fontarray['@'], 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF, 0x00000000);
  single_char_anywhere(LP->GPU_Mode->FrameBufferBase, fontarray['!'], 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF, 0xFF000000, 512, 512);
  single_char_anywhere_scaled(LP->GPU_Mode->FrameBufferBase, fontarray['I'], 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF, 0xFF000000, 10, 10, 2);
  string_anywhere_scaled(LP->GPU_Mode->FrameBufferBase, "OMG it's actually soup! I don't believe it!!", 8, 8, LP->GPU_Mode->Info->HorizontalResolution, LP->GPU_Mode->Info->VerticalResolution, 0x00FFFFFF, 0x00000000, 0,  LP->GPU_Mode->Info->VerticalResolution/2, 2);
  while(iterator) // loop ends on overflow
  {
//    asm volatile ("pause");
    asm volatile ("nop");
    iterator++; // Count to 2^32, which is about 1 second at 4 GHz. 2^64 is... still insanely long.
  }

  LP->RTServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL); // Shutdown the system
}

// A big ol' in-your-face triangle.
void drawTriangle( EFI_PHYSICAL_ADDRESS lfb_base_addr, UINT32 center_x, UINT32 center_y, UINTN width, UINT32 color, UINT32 DESIRED_HREZ, UINT32 DESIRED_VREZ)
{
    UINT32* at = (UINT32*)lfb_base_addr; // The thing at lfb_base_addr is an address pointing to UINT32s. lfb_base_addr itself is a 64-bit number.
    UINTN row, col;

    at += (DESIRED_HREZ * (center_y - width / 2) + center_x - width / 2);

    for (row = 0; row < width / 2; row++)
    {
        for (col = 0; col < width - row * 2; col++)
            *at++ = color;
        at += (DESIRED_HREZ - col);
        for (col = 0; col < width - row * 2; col++)
            *at++ = color;
        at += (DESIRED_HREZ - col + 1);
      }
}

void Blackscreen(EFI_PHYSICAL_ADDRESS lfb_base_addr, UINT32 HREZ, UINT32 VREZ)
{
  UINT32 row, col;
  for (row = 0; row < VREZ; row++)
  {
    for (col = 0; col < HREZ; col++)
    {
      *(UINT32*)lfb_base_addr = 0x00000000;
      lfb_base_addr+=4; // 4 Bpp == 32 bpp
    }
  }
}

void Colorscreen(EFI_PHYSICAL_ADDRESS lfb_base_addr, UINT32 HREZ, UINT32 VREZ, UINT32 color)
{
  UINT32 row, col;
  for (row = 0; row < VREZ; row++)
  {
    for (col = 0; col < HREZ; col++)
    {
      *(UINT32*)lfb_base_addr = color;
      lfb_base_addr+=4;
    }
  }
}


void single_pixel(EFI_PHYSICAL_ADDRESS lfb_base_addr, UINT32 x, UINT32 y, UINT32 HREZ, UINT32 VREZ, UINT32 color)
{
  UINT32 row, col;
  col = x * 4;
  row = y * 4 * HREZ;

  if(y > VREZ || x > HREZ) // Need some kind of error indicator (makes screen red)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x00FF0000);
  }

  *(UINT32*)(lfb_base_addr + row + col) = color;
}

void single_char(EFI_PHYSICAL_ADDRESS lfb_base_addr, char * character, UINT32 height, UINT32 width, UINT32 HREZ, UINT32 VREZ, UINT32 font_color, UINT32 highlight_color)
{
  // Height in number of bytes and width in number of bits
  // Assuming "character" is an array of bytes, e.g. { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},   // U+0040 (@), which is an 8x8 '@' sign
  if(height > VREZ || width > HREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x00FF0000); // Need some kind of error indicator (makes screen red)
  } // Could use an instruction like ARM's USAT to truncate values

  //mapping: x*4 + y*4*(HREZ), x is column number, y is row number; every 4*HREZ bytes is a new on-screen row
  // y max is VREZ, x max is HREZ, 'x4' is the LFB expansion, since 1 bit input maps to 4 bytes output
  for(int row = 0; row < height; row++) // for byte in row
  {
    for (int bit = 0; bit < width; bit++) // for bit in column
    {
      if((character[row] >> bit) & 0x1) // if a 1, output
      {
        if(*(UINT32*)(lfb_base_addr + row*4*HREZ + bit*4) != font_color) // No need to write if it's already there
        {
          *(UINT32*)(lfb_base_addr + row*4*HREZ + bit*4) = font_color;
        }
      }
      else // if a 0, background or skip (set highlight_color to 0xFF000000 for transparency)
      {
        if((highlight_color != 0xFF000000) && (*(UINT32*)(lfb_base_addr + row*4*HREZ + bit*4) != highlight_color))
        {
          *(UINT32*)(lfb_base_addr + row*4*HREZ + bit*4) = highlight_color;
        }
        // do nothing if color's already there for speed
      }
    }
  }
}

void single_char_anywhere(EFI_PHYSICAL_ADDRESS lfb_base_addr, char * character, UINT32 height, UINT32 width, UINT32 HREZ, UINT32 VREZ, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y)
{
  // x and y are top leftmost coordinates
  // Height in number of bytes and width in number of bits
  // Assuming "character" is an array of bytes, e.g. { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},   // U+0040 (@), which is an 8x8 '@' sign
  if(height > VREZ || width > HREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x00FF0000); // Need some kind of error indicator (makes screen red)
  } // Could use an instruction like ARM's USAT to truncate values
  else if(x > HREZ || y > VREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x0000FF00); // Makes screen green
  }
  else if ((y + height) > VREZ || (x + width) > HREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x000000FF); // Makes screen blue
  }

  //mapping: x*4 + y*4*(HREZ), x is column number, y is row number; every 4*HREZ bytes is a new on-screen row
  // y max is VREZ, x max is HREZ, 'x4' is the LFB expansion, since 1 bit input maps to 4 bytes output
  for(int row = 0; row < height; row++) // for byte in row
  {
    for (int bit = 0; bit < width; bit++) // for bit in column
    {
      if((character[row] >> bit) & 0x1) // if a 1, output
      {
        if(*(UINT32*)(lfb_base_addr + (y + row)*4*HREZ + (x + bit)*4) != font_color) // No need to write if it's already there
        {
          *(UINT32*)(lfb_base_addr + (y + row)*4*HREZ + (x + bit)*4) = font_color;
        }
      }
      else // if a 0, background or skip (set highlight_color to 0xFF000000 for transparency)
      {
        if((highlight_color != 0xFF000000) && (*(UINT32*)(lfb_base_addr + (y + row)*4*HREZ + (x + bit)*4) != highlight_color))
        {
          *(UINT32*)(lfb_base_addr + (y + row)*4*HREZ + (x + bit)*4) = highlight_color;
        }
        // do nothing if color's already there for speed
      }
    }
  }
}
// You know, this could probably do images. just pass in an appropriately-formatted array of bytes as the 'character' pointer. Arbitrarily-sized fonts should work, too.
// This is probably the slowest possible way of doing what this function does, though, since it's all based on absolute addressing.
void single_char_anywhere_scaled(EFI_PHYSICAL_ADDRESS lfb_base_addr, char * character, UINT32 height, UINT32 width, UINT32 HREZ, UINT32 VREZ, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale)
{
  // scale is an integer scale factor, e.g. 2 for 2x, 3 for 3x, etc.
  // x and y are top leftmost coordinates
  // Height in number of bytes and width in number of bits
  // Assuming "character" is an array of bytes, e.g. { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},   // U+0040 (@), which is an 8x8 '@' sign
  if(height > VREZ || width > HREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x00FF0000); // Need some kind of error indicator (makes screen red)
  } // Could use an instruction like ARM's USAT to truncate values
  else if(x > HREZ || y > VREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x0000FF00); // Makes screen green
  }
  else if ((y + scale*height) > VREZ || (x + scale*width) > HREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x000000FF); // Makes screen blue
  }

  //mapping: x*4 + y*4*(HREZ), x is column number, y is row number; every 4*HREZ bytes is a new on-screen row
  // y max is VREZ, x max is HREZ, 'x4' is the LFB expansion, since 1 bit input maps to 4 bytes output

  // A 2x scale should turn 1 pixel into a square of 2x2 pixels
  for(int row = 0; row < height; row++) // for byte in row
  {
    for (int bit = 0; bit < width; bit++) // for bit in column
    {
      if((character[row] >> bit) & 0x1) // if a 1, output
      {
// start scale here
        for(int b = 0; b < scale; b++)
        {
          for(int a = 0; a < scale; a++)
          {
            if(*(UINT32*)(lfb_base_addr + ((y*HREZ + x) + scale*(row*HREZ + bit) + (b*HREZ + a))*4) != font_color) // No need to write if it's already there
            {
              *(UINT32*)(lfb_base_addr + ((y*HREZ + x) + scale*(row*HREZ + bit) + (b*HREZ + a))*4) = font_color;
            }
          }
        }
//end scale here
      }
      else // if a 0, background or skip (set highlight_color to 0xFF000000 for transparency)
      {
// start scale here
        for(int b = 0; b < scale; b++)
        {
          for(int a = 0; a < scale; a++)
          {
            if((highlight_color != 0xFF000000) && (*(UINT32*)(lfb_base_addr + ((y*HREZ + x) + scale*(row*HREZ + bit) + (b*HREZ + a))*4) != highlight_color))
            {
              *(UINT32*)(lfb_base_addr + ((y*HREZ + x) + scale*(row*HREZ + bit) + (b*HREZ + a))*4) = highlight_color;
            }
            // do nothing if color's already there for speed
          }
        }
//end scale here
      }
    }
  }
}

// literal strings in C are automatically null-terminated. i.e. "hi" is actually 3 characters long.
void string_anywhere_scaled(EFI_PHYSICAL_ADDRESS lfb_base_addr, char * string, UINT32 height, UINT32 width, UINT32 HREZ, UINT32 VREZ, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale)
{
  // scale is an integer scale factor, e.g. 2 for 2x, 3 for 3x, etc.
  // x and y are top leftmost coordinates
  // Height in number of bytes and width in number of bits, per character
  // Assuming "character" is an array of bytes, e.g. { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},   // U+0040 (@), which is an 8x8 '@' sign
  if(height > VREZ || width > HREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x00FF0000); // Need some kind of error indicator (makes screen red)
  } // Could use an instruction like ARM's USAT to truncate values
  else if(x > HREZ || y > VREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x0000FF00); // Makes screen green
  }
  else if ((y + scale*height) > VREZ || (x + scale*width) > HREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x000000FF); // Makes screen blue
  }

  //mapping: x*4 + y*4*(HREZ), x is column number, y is row number; every 4*HREZ bytes is a new on-screen row
  // y max is VREZ, x max is HREZ, 'x4' is the LFB expansion, since 1 bit input maps to 4 bytes output

  // A 2x scale should turn 1 pixel into a square of 2x2 pixels
// iterate through characters in string
// start while
  uint32_t item = 0;
  char * character;
  while(string[item] != '\0') // for char in string until \0
  {
    character = fontarray[(int)string[item]]; // match the character to the font, using UTF-8.
    // This would expect a font array to include everything in the UTF-8 character set... Or just use the most common ones.
    for(int row = 0; row < height; row++) // for number of rows
    {
      for (int bit = 0; bit < width; bit++) // for bit in column
      {
        if((character[row] >> bit) & 0x1) // if a 1, output
        {
          // start scale here
          for(int b = 0; b < scale; b++)
          {
            for(int a = 0; a < scale; a++)
            {
              if(*(UINT32*)(lfb_base_addr + ((y*HREZ + x) + scale*(row*HREZ + bit) + (b*HREZ + a) + scale * item * width)*4) != font_color) // No need to write if it's already there
              {
                *(UINT32*)(lfb_base_addr + ((y*HREZ + x) + scale*(row*HREZ + bit) + (b*HREZ + a) + scale * item * width)*4) = font_color;
              }
            }
          } //end scale here
        } //end if
        else // if a 0, background or skip (set highlight_color to 0xFF000000 for transparency)
        {
          // start scale here
          for(int b = 0; b < scale; b++)
          {
            for(int a = 0; a < scale; a++)
            {
              if((highlight_color != 0xFF000000) && (*(UINT32*)(lfb_base_addr + ((y*HREZ + x) + scale*(row*HREZ + bit) + (b*HREZ + a) + scale * item * width)*4) != highlight_color))
              {
                *(UINT32*)(lfb_base_addr + ((y*HREZ + x) + scale*(row*HREZ + bit) + (b*HREZ + a) + scale * item * width)*4) = highlight_color;
              }
              // do nothing if color's already there for speed
            }
          } //end scale here
        } // end else
      } // end bit in column
    } // end byte in row
    item++;
  } // end while
} // end function

void formatted_string_anywhere_scaled(EFI_PHYSICAL_ADDRESS lfb_base_addr, char * string, UINT32 height, UINT32 width, UINT32 HREZ, UINT32 VREZ, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale)
{
  // scale is an integer scale factor, e.g. 2 for 2x, 3 for 3x, etc.
  // x and y are top leftmost coordinates
  // Height in number of bytes and width in number of bits, per character
  // Assuming "character" is an array of bytes, e.g. { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},   // U+0040 (@), which is an 8x8 '@' sign
  if(height > VREZ || width > HREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x00FF0000); // Need some kind of error indicator (makes screen red)
  } // Could use an instruction like ARM's USAT to truncate values
  else if(x > HREZ || y > VREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x0000FF00); // Makes screen green
  }
  else if ((y + scale*height) > VREZ || (x + scale*width) > HREZ)
  {
    Colorscreen(lfb_base_addr, HREZ, VREZ, 0x000000FF); // Makes screen blue
  }

  //mapping: x*4 + y*4*(HREZ), x is column number, y is row number; every 4*HREZ bytes is a new on-screen row
  // y max is VREZ, x max is HREZ, 'x4' is the LFB expansion, since 1 bit input maps to 4 bytes output

  // A 2x scale should turn 1 pixel into a square of 2x2 pixels
// iterate through characters in string
// start while
  uint32_t item = 0;
  uint32_t formatting_subtractor = 0; // need to keep track of formatting characters so the printed output doesn't get messed up
  uint32_t formatting_adder = 0; // need to keep track of formatting characters so the printed output doesn't get messed up
  int format_end;
  int padding = 0;
  char * character;
  char * stringbuffer[4096]; // Max size a string can be

  while(string[item] != '\0') // for char in string until \0
  {
    character = fontarray[(int)string[item]]; // match the character to the font, using UTF-8.
    if(character == '%')
    {
      //formatted string, check what comes next and iterate "item" until out of formatting code
      formatting_subtractor++; // To account for the first "%"
      format_end = 0;
      while(!format_end) //TODO: What printf and co. do is build a string first and then output it. that's probably what should happen here, too: build a string and then send it to string_anywhere_scaled. No need for Output_render then!
      {
        character = fontarray[(int)string[++item]];
        switch(character) // We DO have stdarg, so we can use va_arg!
        {
          case '%': //done
              //
              // %% -> %
              //

              // Keep on going and print the % like normal, with the output offset by 1 for the first %
              format_end = 1;
              break; // This breaks the switch statement, but a continue will go back to a while loop.

          case '0': //done
              // need to know how many zeroes there are
              padding = 1;
              formatting_subtractor++;
              break; // Go back to start of while loop to check the next value

          case '-':
              Item.PadBefore = FALSE;
              formatting_subtractor++;
              break;

          case '.':
              Item.WidthParse = &Item.FieldWidth;
              formatting_subtractor++;
              break;

          case '*':
              *Item.WidthParse = va_arg(ps->args, UINTN);
              formatting_subtractor++;
              break;

          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
              formatting_subtractor++;
              if(padding == 1) // TODO: padding_size and formatting_size are supposed to be minimum widths for the whole modifier--right now this gives fixed padding
              {
                character = '0'; // this won't work. need fontarray
                padding = 0;
              }
              else
              {
                character = ' ';
              }

              int padding_size = *(int)character - '0'; // *(int)character is the same as (int)character[0]
              item++;
              while(string[item] <= '9' && string[item] >= '0') // check the next item
              {
                padding_size = padding_size * 10 + ((int)string[item] - '0');
                item++;
                formatting_subtractor++;
              }

              //padding_size -= [length of next thing] // next thing will either be 8, 16, 32, or 64-bit. either hhu, hu, u, or lu (llu == lu)
              for(int i = 0; i < padding_size; i++) //pad with zeros
              {
                Output_render(lfb_base_addr, character, height, width, HREZ, VREZ, font_color, highlight_color, x, y, scale, --item, formatting_subtractor, formatting_adder); // de-increment item so that the next character is correct and to not screw up the padding
              }
              formatting_adder += padding_size;
//              formatting_adder += (padding_size + [length of next thing]); // Don't need this if "next thing" truly gets evaluated next and not in this case statement
              continue; // need to check for x or d or something next

          case 'a':
              Item.Item.pc = va_arg(ps->args, CHAR8 *);
              Item.Item.Ascii = TRUE;
              if (!Item.Item.pc) {
                  Item.Item.pc = (CHAR8 *)"(null)";
              }
              formatting_subtractor++;
              break;

          case 's':
              Item.Item.pw = va_arg(ps->args, CHAR16 *);
              if (!Item.Item.pw) {
                  Item.Item.pw = L"(null)";
              }
              formatting_subtractor++;
              break;

          case 'c':
              Item.Scratch[0] = (CHAR16) va_arg(ps->args, UINTN);
              Item.Scratch[1] = 0;
              Item.Item.pw = Item.Scratch;
              formatting_subtractor++;
              break;

          case 'l':
              Item.Long = TRUE;
              formatting_subtractor++;
              break;

          case 'X': // should be uppercase hex
              Item.Width = Item.Long ? 16 : 8;
              Item.Pad = '0';
              formatting_subtractor++;
              break;
          case 'x':
              ValueToHex (
                  Item.Scratch,
                  Item.Long ? va_arg(ps->args, UINT64) : va_arg(ps->args, UINT32)
                  );
              Item.Item.pw = Item.Scratch;
              formatting_subtractor++;
              break;

          case 'G':
              GuidToString (Item.Scratch, va_arg(ps->args, EFI_GUID *));
              Item.Item.pw = Item.Scratch;
              formatting_subtractor++;
              break;

          case 'g':
              GuidToString (Item.Scratch, va_arg(ps->args, EFI_GUID *));
              Item.Item.pw = Item.Scratch;
              formatting_subtractor++;
              break;

          case 'u':
              ValueToString (
                  Item.Scratch,
                  Item.Comma,
                  Item.Long ? va_arg(ps->args, UINT64) : va_arg(ps->args, UINT32)
                  );
              Item.Item.pw = Item.Scratch;
              formatting_subtractor++;
              break;

          case 'd':
              ValueToString (
                  Item.Scratch,
                  Item.Comma,
                  Item.Long ? va_arg(ps->args, INT64) : va_arg(ps->args, INT32)
                  );
              Item.Item.pw = Item.Scratch;
              formatting_subtractor++;
              break;

          case 'f':
              FloatToString (
                  Item.Scratch,
                  Item.Comma,
                  va_arg(ps->args, double)
                  );
              Item.Item.pw = Item.Scratch;
              formatting_subtractor++;
              break;

          case 'n':
              PSETATTR(ps, ps->AttrNorm);
              formatting_subtractor++;
              break;

          case 'h':
              PSETATTR(ps, ps->AttrHighlight);
              formatting_subtractor++;
              break;

          case 'e':
              PSETATTR(ps, ps->AttrError);
              formatting_subtractor++;
              break;

          case 'N':
              Attr = ps->AttrNorm;
              break;

          case 'i':
              Attr = ps->AttrHighlight;
              formatting_subtractor++;
              break;

          case 'E':
              Attr = ps->AttrError;
              formatting_subtractor++;
              break;

          default:
              character = fontarray['?'];
              format_end = 1;
              break;
        } //end switch
      } //end while !format_end
    }// end %

    Output_render(lfb_base_addr, character, height, width, HREZ, VREZ, font_color, highlight_color, x, y, scale, item, formatting_subtractor, formatting_adder);
    item++;
  } // end while
} // end function

void Output_render(EFI_PHYSICAL_ADDRESS lfb_base_addr, char * character, UINT32 height, UINT32 width, UINT32 HREZ, UINT32 VREZ, UINT32 font_color, UINT32 highlight_color, UINT32 x, UINT32 y, UINT32 scale, UINT32 item, UINT32 formatting_subtractor, UINT32 formatting_adder)
{
  for(int row = 0; row < height; row++) // for number of rows
  {
    for (int bit = 0; bit < width; bit++) // for bit in column
    {
      if((character[row] >> bit) & 0x1) // if a 1, output NOTE: There's gotta be a better way to do this.
      {
        // start scale here
        for(int b = 0; b < scale; b++)
        {
          for(int a = 0; a < scale; a++)
          {
            if(*(UINT32*)(lfb_base_addr + ((y*HREZ + x) + scale*(row*HREZ + bit) + (b*HREZ + a) + scale * (item - formatting_subtractor + formatting_adder) * width)*4) != font_color) // No need to write if it's already there
            {
              *(UINT32*)(lfb_base_addr + ((y*HREZ + x) + scale*(row*HREZ + bit) + (b*HREZ + a) + scale * (item - formatting_subtractor + formatting_adder) * width)*4) = font_color;
            }
          }
        } //end scale here
      } //end if
      else // if a 0, background or skip (set highlight_color to 0xFF000000 for transparency)
      {
        // start scale here
        for(int b = 0; b < scale; b++)
        {
          for(int a = 0; a < scale; a++)
          {
            if((highlight_color != 0xFF000000) && (*(UINT32*)(lfb_base_addr + ((y*HREZ + x) + scale*(row*HREZ + bit) + (b*HREZ + a) + scale * (item - formatting_subtractor + formatting_adder) * width)*4) != highlight_color))
            {
              *(UINT32*)(lfb_base_addr + ((y*HREZ + x) + scale*(row*HREZ + bit) + (b*HREZ + a) + scale * (item - formatting_subtractor + formatting_adder) * width)*4) = highlight_color;
            }
            // do nothing if color's already there for speed
          }
        } //end scale here
      } // end else
    } // end bit in column
  } // end byte in row
}

////////////////////////////////////////////////////////

  //TODO: perhaps save the cursor position when done in a global struct
  // TODO: wraparound when reach the bottom / right edge

////////////////////////////////////////////////////

//IDEA: Once get video output for printing, check if we're in long mode
// Would be great if std libs could be used, too. Wouldn't need much more than Printf and getchar() the way we're doing things, though.
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
RT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL); // Hard reboot the system
RT->ResetSystem (EfiResetWarm, EFI_SUCCESS, 0, NULL); // Soft reboot the system

There is also EfiResetPlatformSpecific, but that's not really important. (Takes a standard 128-bit EFI_GUID as ResetData for a custom reset type)
*/


/* GRAPHICS REFERENCE
typedef int64_t    INTN;
typedef uint64_t   UINTN;

typedef uint64_t   UINT64;
typedef int64_t    INT64;

typedef uint32_t   UINT32;
typedef int32_t    INT32;

typedef uint16_t   UINT16;
typedef int16_t    INT16;
typedef uint8_t    UINT8;
typedef int8_t     INT8;

typedef UINT64          EFI_PHYSICAL_ADDRESS;

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

*/

/* RUNTIME SERVICE REFERENCE
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
*/
