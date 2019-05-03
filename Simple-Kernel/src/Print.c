//==================================================================================================================================
//  Simple Kernel: Printf Functions
//==================================================================================================================================
//
// Version 1.0
//
// Modifier:
//  KNNSpeed
//
// Source Code:
//  https://github.com/KNNSpeed/Simple-Kernel
//
// This file provides a printf function for use in a bare-metal/freestanding
// environment. It's licensed under the BSD 3-clause below, though it won't do
// you much good without the supporting functions of the rest of the Simple
// Kernel framework... which is licensed under the even more lax "PD with Credit"
// license at the above link. :)
//
// Derived from FreeBSD's subr_prf, with modifications inspired by Michael Steil's
// "A Standalone printf() for Early Bootup" at https://www.pagetable.com/?p=298
//

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)subr_prf.c	8.3 (Berkeley) 1/21/94
 */

#include "Kernel64.h"

//
// QUICK SCROLL NOTE:
//
// Two ways of quick scroll: "New" method and "Old" method for dealing with text
// sizes not aligned with the screen's vertical resolution.
//
// "New" has a partially-scrolled line up top, but text is aligned at the
// bottom no matter the size. "Old" has no partially-scrolled line of text at
// the top, but it has a gap of the default background color below the lowest
// line of text. Does not matter for text sizes aligned with the screen's
// vertical resolution.
//
// Comment out this definition to use the "old" method.
#define NEW_QUICK_SCROLL

static size_t strlen(const char *s);
static inline int imax(int a, int b);
static void  printf_putchar(int ch, void *arg);
static char *ksprintn(char *nbuf, uintmax_t num, int base, int *len, int upper);
static void  snprintf_func(int ch, void *arg);

#define NBBY    8               /* number of bits in a byte */

char const hex2ascii_data[] = "0123456789abcdefghijklmnopqrstuvwxyz";

#define hex2ascii(hex)  (hex2ascii_data[hex])
#define toupper(c)      ((c) - 0x20 * (((c) >= 'a') && ((c) <= 'z')))

/* Max number conversion buffer length: an unsigned long long int in base 2, plus NUL byte. */
#define MAXNBUF	(sizeof(intmax_t) * NBBY + 1)

struct snprintf_arg {
	char	*str;
	size_t	remain;
};

static size_t strlen(const char *s)
{
	size_t l = 0;
	while (*s++)
		l++;
	return l;
}

static inline int imax(int a, int b)
{
	return (a > b ? a : b);
}

/*
 * Put a NUL-terminated ASCII number (base <= 36) in a buffer in reverse
 * order; return an optional length and a pointer to the last character
 * written in the buffer (i.e., the first character of the string).
 * The buffer pointed to by `nbuf' must have length >= MAXNBUF.
 */
static char * ksprintn(char *nbuf, uintmax_t num, int base, int *lenp, int upper)
{
	char *p, c;

	p = nbuf;
	*p = '\0';
	do {
		c = hex2ascii(num % base);
		*++p = upper ? toupper(c) : c;
	} while (num /= base);
	if (lenp)
		*lenp = p - nbuf;
	return (p);
}

/*
 * Scaled down version of printf(3).
 *
 * Two additional formats:
 *
 * The format %b is supported to decode error registers.
 * Its usage is:
 *
 *	printf("reg=%b\n", regval, "<base><arg>*");
 *
 * where <base> is the output base expressed as a control character, e.g.
 * \10 gives octal; \20 gives hex.  Each arg is a sequence of characters,
 * the first of which gives the bit number to be inspected (origin 1), and
 * the next characters (up to a control character, i.e. a character <= 32),
 * give the name of the register.  Thus:
 *
 *	kvprintf("reg=%b\n", 3, "\10\2BITTWO\1BITONE");
 *
 * would produce output:
 *
 *	reg=3<BITTWO,BITONE>
 *
 * XXX:  %D  -- Hexdump, takes pointer and separator string:
 *		("%6D", ptr, ":")   -> XX:XX:XX:XX:XX:XX
 *		("%*D", len, ptr, " " -> XX XX XX XX ...
 */
int kvprintf(char const *fmt, void (*func)(int, void*), void *arg, int radix, va_list ap)
{
#define PCHAR(c) {int cc=(c); if (func) (*func)(cc,arg); else *d++ = cc; retval++; }
	char nbuf[MAXNBUF];
	char *d;
	const char *p, *percent, *q;
	unsigned char *up;
	int ch, n;
	uintmax_t num;
	int base, lflag, qflag, tmp, width, ladjust, sharpflag, neg, sign, dot;
	int cflag, hflag, jflag, tflag, zflag;
	int bconv, dwidth, upper;
	char padc;
	int stop = 0, retval = 0;

	num = 0;
	q = NULL;
	if (!func)
		d = (char *) arg;
	else
		d = NULL;

	if (fmt == NULL)
		fmt = "(fmt null)\n";

	if (radix < 2 || radix > 36)
		radix = 10;

	for (;;) {
		padc = ' ';
		width = 0;
		while ((ch = (unsigned char)*fmt++) != '%' || stop) {
			if (ch == '\0')
				return (retval);
			PCHAR(ch);
		}
		percent = fmt - 1;
		qflag = 0; lflag = 0; ladjust = 0; sharpflag = 0; neg = 0;
		sign = 0; dot = 0; bconv = 0; dwidth = 0; upper = 0;
		cflag = 0; hflag = 0; jflag = 0; tflag = 0; zflag = 0;
reswitch:	switch (ch = (unsigned char)*fmt++) {
		case '.':
			dot = 1;
			goto reswitch;
		case '#':
			sharpflag = 1;
			goto reswitch;
		case '+':
			sign = 1;
			goto reswitch;
		case '-':
			ladjust = 1;
			goto reswitch;
		case '%':
			PCHAR(ch);
			break;
		case '*':
			if (!dot) {
				width = va_arg(ap, int);
				if (width < 0) {
					ladjust = !ladjust;
					width = -width;
				}
			} else {
				dwidth = va_arg(ap, int);
			}
			goto reswitch;
		case '0':
			if (!dot) {
				padc = '0';
				goto reswitch;
			}
			/* FALLTHROUGH */
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
				for (n = 0;; ++fmt) {
					n = n * 10 + ch - '0';
					ch = *fmt;
					if (ch < '0' || ch > '9')
						break;
				}
			if (dot)
				dwidth = n;
			else
				width = n;
			goto reswitch;
		case 'b':
			ladjust = 1;
			bconv = 1;
			goto handle_nosign;
		case 'c':
			width -= 1;

			if (!ladjust && width > 0)
				while (width--)
					PCHAR(padc);
			PCHAR(va_arg(ap, int));
			if (ladjust && width > 0)
				while (width--)
					PCHAR(padc);
			break;
		case 'D':
			up = va_arg(ap, unsigned char *);
			p = va_arg(ap, char *);
			if (!width)
				width = 16;
			while(width--) {
				PCHAR(hex2ascii(*up >> 4));
				PCHAR(hex2ascii(*up & 0x0f));
				up++;
				if (width)
					for (q=p;*q;q++)
						PCHAR(*q);
			}
			break;
		case 'd':
		case 'i':
			base = 10;
			sign = 1;
			goto handle_sign;
		case 'h':
			if (hflag) {
				hflag = 0;
				cflag = 1;
			} else
				hflag = 1;
			goto reswitch;
		case 'j':
			jflag = 1;
			goto reswitch;
		case 'l':
			if (lflag) {
				lflag = 0;
				qflag = 1;
			} else
				lflag = 1;
			goto reswitch;
		case 'n':
			if (jflag)
				*(va_arg(ap, intmax_t *)) = retval;
			else if (qflag)
				*(va_arg(ap, long long *)) = retval;
			else if (lflag)
				*(va_arg(ap, long *)) = retval;
			else if (zflag)
				*(va_arg(ap, size_t *)) = retval;
			else if (hflag)
				*(va_arg(ap, short *)) = retval;
			else if (cflag)
				*(va_arg(ap, char *)) = retval;
			else
				*(va_arg(ap, int *)) = retval;
			break;
		case 'o':
			base = 8;
			goto handle_nosign;
		case 'p':
			base = 16;
			sharpflag = (width == 0);
			sign = 0;
			num = (uintptr_t)va_arg(ap, void *);
			goto number;
		case 'q':
			qflag = 1;
			goto reswitch;
		case 'r':
			base = radix;
			if (sign)
				goto handle_sign;
			goto handle_nosign;
		case 's':
			p = va_arg(ap, char *);
			if (p == NULL)
				p = "(null)";
			if (!dot)
				n = strlen (p);
			else
				for (n = 0; n < dwidth && p[n]; n++)
					continue;

			width -= n;

			if (!ladjust && width > 0)
				while (width--)
					PCHAR(padc);
			while (n--)
				PCHAR(*p++);
			if (ladjust && width > 0)
				while (width--)
					PCHAR(padc);
			break;
		case 't':
			tflag = 1;
			goto reswitch;
		case 'u':
			base = 10;
			goto handle_nosign;
		case 'X':
			upper = 1;
			__attribute__ ((fallthrough)); // For GCC to stop warning about a fallthrough here
		case 'x':
			base = 16;
			goto handle_nosign;
		case 'y':
			base = 16;
			sign = 1;
			goto handle_sign;
		case 'z':
			zflag = 1;
			goto reswitch;
handle_nosign:
			sign = 0;
			if (jflag)
				num = va_arg(ap, uintmax_t);
			else if (qflag)
				num = va_arg(ap, unsigned long long);
			else if (tflag)
				num = va_arg(ap, ptrdiff_t);
			else if (lflag)
				num = va_arg(ap, unsigned long);
			else if (zflag)
				num = va_arg(ap, size_t);
			else if (hflag)
				num = (unsigned short)va_arg(ap, int);
			else if (cflag)
				num = (unsigned char)va_arg(ap, int);
			else
				num = va_arg(ap, unsigned int);
			if (bconv) {
				q = va_arg(ap, char *);
				base = *q++;
			}
			goto number;
handle_sign:
			if (jflag)
				num = va_arg(ap, intmax_t);
			else if (qflag)
				num = va_arg(ap, long long);
			else if (tflag)
				num = va_arg(ap, ptrdiff_t);
			else if (lflag)
				num = va_arg(ap, long);
			else if (zflag)
				num = va_arg(ap, ssize_t);
			else if (hflag)
				num = (short)va_arg(ap, int);
			else if (cflag)
				num = (char)va_arg(ap, int);
			else
				num = va_arg(ap, int);
number:
			if (sign && (intmax_t)num < 0) {
				neg = 1;
				num = -(intmax_t)num;
			}
			p = ksprintn(nbuf, num, base, &n, upper);
			tmp = 0;

			// There's weird behavior here with #. Don't use # to get 0x with zero-padding
			// (e.g. use 0x%016qx instead, not %#016qx or %#018qx, the latter of which will pad
			// 16 characters for nonzero numbers but zeros will have 18 characters).
			// Same with octal: use a leading zero and don't rely on # if you want zero-padding.
			// # works if you don't need zero padding, though.

			if (sharpflag && num != 0) {
				if (base == 8)
					tmp++;
				else if (base == 16)
					tmp += 2;
			}
			if (neg)
				tmp++;

			if (!ladjust && padc == '0')
				dwidth = width - tmp;
			width -= tmp + imax(dwidth, n);
			dwidth -= n;
			if (!ladjust)
				while (width-- > 0)
					PCHAR(' ');
			if (neg)
				PCHAR('-');
			if (sharpflag && num != 0) {
				if (base == 8) {
					PCHAR('0');
				} else if (base == 16) {
					PCHAR('0');
					PCHAR('x');
				}
			}
			while (dwidth-- > 0)
				PCHAR('0');

			while (*p)
				PCHAR(*p--);

			if (bconv && num != 0) {
				/* %b conversion flag format. */
				tmp = retval;
				while (*q) {
					n = *q++;
					if (num & (1 << (n - 1))) {
						PCHAR(retval != tmp ?
						    ',' : '<');
						for (; (n = *q) > ' '; ++q)
							PCHAR(n);
					} else
						for (; *q > ' '; ++q)
							continue;
				}
				if (retval != tmp) {
					PCHAR('>');
					width -= retval - tmp;
				}
			}

			if (ladjust)
				while (width-- > 0)
					PCHAR(' ');

			break;
		default:
			while (percent < fmt)
				PCHAR(*percent++);
			/*
			 * Since we ignore a formatting argument it is no
			 * longer safe to obey the remaining formatting
			 * arguments as the arguments will no longer match
			 * the format specs.
			 */
			stop = 1;
			break;
		}
	}
#undef PCHAR
}

/*
 * Scaled down version of snprintf(3).
 */
int snprintf(char *str, size_t size, const char *format, ...)
{
	int retval;
	va_list ap;

	va_start(ap, format);
	retval = vsnprintf(str, size, format, ap);
	va_end(ap);
	return(retval);
}

/*
 * Scaled down version of vsnprintf(3).
 */
int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	struct snprintf_arg info;
	int retval;

	info.str = str;
	info.remain = size;
	retval = kvprintf(format, snprintf_func, &info, 10, ap);
	if (info.remain >= 1)
		*info.str++ = '\0';
	return (retval);
}

static void snprintf_func(int ch, void *arg)
{
	struct snprintf_arg *const info = arg;

	if (info->remain >= 2) {
		*info->str++ = ch;
		info->remain--;
	}
}

/*
 * Kernel version which takes radix argument vsnprintf(3).
 */
int vsnrprintf(char *str, size_t size, int radix, const char *format, va_list ap)
{
	struct snprintf_arg info;
	int retval;

	info.str = str;
	info.remain = size;
	retval = kvprintf(format, snprintf_func, &info, radix, ap);
	if (info.remain >= 1)
		*info.str++ = '\0';
	return (retval);
}

// kvprintf requires a putchar function. No console layer here: this putchar draws directly to the screen.
static void printf_putchar(int output_character, void *arglist) // Character is in int form; this putchar will only apply to printf because it modifies the global string index
{
	GLOBAL_PRINT_INFO_STRUCT *arg = (GLOBAL_PRINT_INFO_STRUCT *)arglist; // Need this because arglist has to be a void* due to kvprintf. We need it to be a GLOBAL_PRINT_INFO_STRUCT instead.
	// Escape codes could go here. Full VT-100 control sequence functionality...?
	// Honestly, with the fine-grained control given by Global_Print_Info, do we really need them? (Manipulating x and y, highlight_color, and blackscreen() are plenty for me...)
	/*
	if(output_character == '\033')
	{
		escape_mode = 1;
		...blah blah blah
	}

	else if(escape_mode && output_character == '[')
	{
		control_sequence = 1;
		...blah set x and y and index, whatever
	}
	else if(escape_mode && control_sequence)
	{
		if(output_character == '2')
	}
	else
	{
		escape_mode = 0;
		syntax_is_right = 0;

		...then do the stuff below
	}
	// Nah, this isn't worth it. It's just not necessary.
	*/
	switch(output_character)
	{
		case '\033':
			// Escape doesn't do anything currently.
		case '\x7F':
			// DEL is supposed to get ignored. It doesn't always, but it will here.
			break;
		case '\x85': // NEL, or CR+LF in one character
			arg->index = 0;
			if((arg->y + arg->height * arg->scale) > (arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale)) // Vertical wraparound
			{
				if(!arg->textscrollmode)
				{
					arg->y = 0; // Wrap
				}
				else if(arg->textscrollmode == arg->height*arg->scale) // Quick Scroll
				{

#ifdef NEW_QUICK_SCROLL
					// New way (topmost line will be partially scrolled up offscreen, but no color gap under the bottommost text line; VerticalResolution % (height * scale) == 0 fonts don't have to worry all text on screen is the same size)
					uint64_t min_scroll_size = arg->y + 2*arg->height*arg->scale - arg->defaultGPU.Info->VerticalResolution;
					arg->y = arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale;

					AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * min_scroll_size), (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine*4);
					if(arg->background_color != 0xFF000000)
					{
						AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine);
					}
#else
					// Old way (gap of background color below the bottommost text line, but no partial scroll up top--the topmost line goes away; VerticalResolution % (height * scale) == 0 fonts don't have to worry if all text on screen is the same size)
					// Qualitative test results: This can scroll a 4K screen framebuffer (31MB) extremely quickly :D (Interestingly enough, the standard memmove in memmove.c can also do it pretty quickly since GCC vectorizes it.)
					AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * arg->height * arg->scale), arg->y * arg->defaultGPU.Info->PixelsPerScanLine*4);
					if(arg->background_color != 0xFF000000)
					{
						AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->y * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, (arg->defaultGPU.Info->VerticalResolution - arg->y) * arg->defaultGPU.Info->PixelsPerScanLine);
					}
#endif

				}
				else if(arg->textscrollmode == arg->defaultGPU.Info->VerticalResolution) // Screen wipe
				{
					if(arg->background_color != 0xFF000000)
					{
						AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, arg->background_color, arg->defaultGPU.Info->VerticalResolution * arg->defaultGPU.Info->PixelsPerScanLine);
					}
				}
				else // Smooth scroll
				{
					uint64_t min_scroll_size = arg->y + 2*arg->height*arg->scale - arg->defaultGPU.Info->VerticalResolution;
					arg->y = arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale;
					// This offset correction is needed in case a font size/scale combination is not an integer multiple of the vertical resolution.
					// Even if it is, changing scales or arg->y could cause a variable offset and that needs to be accounted for.

					for(uint64_t smooth = 0; smooth < min_scroll_size; smooth += arg->textscrollmode) // Random: (smooth --> 0) is the same as ((smooth--) > 0); It may not be obvious that they're the same at first glance.
					{
						AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * arg->textscrollmode), (arg->defaultGPU.Info->VerticalResolution - arg->textscrollmode - smooth) * arg->defaultGPU.Info->PixelsPerScanLine * 4);
						if(arg->background_color != 0xFF000000)
						{
							AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + (arg->defaultGPU.Info->VerticalResolution - arg->textscrollmode - smooth) * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, arg->textscrollmode*arg->defaultGPU.Info->PixelsPerScanLine);
						}
					}
				}
			}
			else
			{
				arg->y += arg->height * arg->scale;
			}
			break;
		case '\014': // Form Feed, aka next page
			Resetdefaultcolorscreen();
			break;
		case '\a': // BEL
			// *BEEP* -- there's no output hardware for this yet...
			Colorscreen(arg->defaultGPU, 0x00FFFFFF); // So make the screen white instead.
			break;
		case '\b': // Backspace (non-destructive, though it can be used to overwrite since it just moves the cursor back one)
			if(arg->index != 0)
				arg->index--;
			break;
		case '\r':
			arg->index = 0;
			break;
		case '\v': // Vertical tab
		// Traditionally these were 6 vertical lines down, like how horiztonal tabs are 8 characters across: https://en.wikipedia.org/wiki/Tab_key#Tab_characters
		// NOTE: This implementation does NOT make a vertical line of highlight_color.
			for(int tabspaces = 0; tabspaces < 6; tabspaces++)
			{
				if((arg->y + arg->height * arg->scale) > (arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale)) // Vertical wraparound
				{
					if(!arg->textscrollmode)
					{
						arg->y = 0; // Wrap
					}
					else if(arg->textscrollmode == arg->height*arg->scale) // Quick Scroll
					{

#ifdef NEW_QUICK_SCROLL
						// New way (topmost line will be partially scrolled up offscreen, but no color gap under the bottommost text line; VerticalResolution % (height * scale) == 0 fonts don't have to worry all text on screen is the same size)
						uint64_t min_scroll_size = arg->y + 2*arg->height*arg->scale - arg->defaultGPU.Info->VerticalResolution;
						arg->y = arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale;

						AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * min_scroll_size), (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine*4);
						if(arg->background_color != 0xFF000000)
						{
							AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine);
						}
#else
						// Old way (gap of background color below the bottommost text line, but no partial scroll up top--the topmost line goes away; VerticalResolution % (height * scale) == 0 fonts don't have to worry if all text on screen is the same size)
						// Qualitative test results: This can scroll a 4K screen framebuffer (31MB) extremely quickly :D (Interestingly enough, the standard memmove in memmove.c can also do it pretty quickly since GCC vectorizes it.)
						AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * arg->height * arg->scale), arg->y * arg->defaultGPU.Info->PixelsPerScanLine*4);
						if(arg->background_color != 0xFF000000)
						{
							AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->y * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, (arg->defaultGPU.Info->VerticalResolution - arg->y) * arg->defaultGPU.Info->PixelsPerScanLine);
						}
#endif

					}
					else if(arg->textscrollmode == arg->defaultGPU.Info->VerticalResolution) // Screen wipe
					{
						if(arg->background_color != 0xFF000000)
						{
							AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, arg->background_color, arg->defaultGPU.Info->VerticalResolution * arg->defaultGPU.Info->PixelsPerScanLine);
						}
					}
					else // Smooth scroll
					{
						uint64_t min_scroll_size = arg->y + 2*arg->height*arg->scale - arg->defaultGPU.Info->VerticalResolution;
						arg->y = arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale;
						// This offset correction is needed in case a font size/scale combination is not an integer multiple of the vertical resolution.
						// Even if it is, changing scales or arg->y could cause a variable offset and that needs to be accounted for.

						for(uint64_t smooth = 0; smooth < min_scroll_size; smooth += arg->textscrollmode) // Random: (smooth --> 0) is the same as ((smooth--) > 0); It may not be obvious that they're the same at first glance.
						{
							AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * arg->textscrollmode), (arg->defaultGPU.Info->VerticalResolution - arg->textscrollmode - smooth) * arg->defaultGPU.Info->PixelsPerScanLine * 4);
							if(arg->background_color != 0xFF000000)
							{
								AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + (arg->defaultGPU.Info->VerticalResolution - arg->textscrollmode - smooth) * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, arg->textscrollmode*arg->defaultGPU.Info->PixelsPerScanLine);
							}
						}
					}
				}
				else
				{
					arg->y += arg->height * arg->scale;
				}
			}
			break;
		case '\n':
			if((arg->y + arg->height * arg->scale) > (arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale)) // Vertical wraparound
			{
				if(!arg->textscrollmode)
				{
					arg->y = 0; // Wrap
				}
				else if(arg->textscrollmode == arg->height*arg->scale) // Quick Scroll
				{

#ifdef NEW_QUICK_SCROLL
					// New way (topmost line will be partially scrolled up offscreen, but no color gap under the bottommost text line; VerticalResolution % (height * scale) == 0 fonts don't have to worry all text on screen is the same size)
					uint64_t min_scroll_size = arg->y + 2*arg->height*arg->scale - arg->defaultGPU.Info->VerticalResolution;
					arg->y = arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale;

					AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * min_scroll_size), (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine*4);
					if(arg->background_color != 0xFF000000)
					{
						AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine);
					}
#else
					// Old way (gap of background color below the bottommost text line, but no partial scroll up top--the topmost line goes away; VerticalResolution % (height * scale) == 0 fonts don't have to worry if all text on screen is the same size)
					// Qualitative test results: This can scroll a 4K screen framebuffer (31MB) extremely quickly :D (Interestingly enough, the standard memmove in memmove.c can also do it pretty quickly since GCC vectorizes it.)
					AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * arg->height * arg->scale), arg->y * arg->defaultGPU.Info->PixelsPerScanLine*4);
					if(arg->background_color != 0xFF000000)
					{
						AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->y * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, (arg->defaultGPU.Info->VerticalResolution - arg->y) * arg->defaultGPU.Info->PixelsPerScanLine);
					}
#endif

				}
				else if(arg->textscrollmode == arg->defaultGPU.Info->VerticalResolution) // Screen wipe
				{
					if(arg->background_color != 0xFF000000)
					{
						AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, arg->background_color, arg->defaultGPU.Info->VerticalResolution * arg->defaultGPU.Info->PixelsPerScanLine);
					}
				}
				else // Smooth scroll
				{
					uint64_t min_scroll_size = arg->y + 2*arg->height*arg->scale - arg->defaultGPU.Info->VerticalResolution;
					arg->y = arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale;
					// This offset correction is needed in case a font size/scale combination is not an integer multiple of the vertical resolution.
					// Even if it is, changing scales or arg->y could cause a variable offset and that needs to be accounted for.

					for(uint64_t smooth = 0; smooth < min_scroll_size; smooth += arg->textscrollmode) // Random: (smooth --> 0) is the same as ((smooth--) > 0); It may not be obvious that they're the same at first glance.
					{
						AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * arg->textscrollmode), (arg->defaultGPU.Info->VerticalResolution - arg->textscrollmode - smooth) * arg->defaultGPU.Info->PixelsPerScanLine * 4);
						if(arg->background_color != 0xFF000000)
						{
							AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + (arg->defaultGPU.Info->VerticalResolution - arg->textscrollmode - smooth) * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, arg->textscrollmode*arg->defaultGPU.Info->PixelsPerScanLine);
						}
					}
				}
			}
			else
			{
				arg->y += arg->height * arg->scale;
			}
			break;
		case '\t': // Tab
			for(int tabspaces = 0; tabspaces < 8; tabspaces++) // Just do the default output actions 8 times with a space character. Tab stops are 8 characters across.
			{ // But why not just do arg->index += 8? Because then the highlight won't propagate.
				Output_render_text(arg->defaultGPU, ' ', arg->height, arg->width, arg->font_color, arg->highlight_color, arg->x, arg->y, arg->scale, arg->index);
		//  	Output_render(Global_Print_Info.defaultGPU, output_character, Global_Print_Info.height, Global_Print_Info.width, Global_Print_Info.font_color, Global_Print_Info.highlight_color, Global_Print_Info.x, Global_Print_Info.y, Global_Print_Info.scale, Global_Print_Info.index);

				arg->index++; // Increment global character index
		//  	Global_Print_Info.index++; // This should do the same thing.

				if(arg->index * arg->width * arg->scale > (arg->defaultGPU.Info->HorizontalResolution - arg->width * arg->scale)) // Check if text is running off screen
				{
					arg->index = 0; // Horizontal wraparound
					if((arg->y + arg->height * arg->scale) > (arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale)) // Vertical wraparound if hit the bottom of the screen
					{
						if(!arg->textscrollmode)
						{
							arg->y = 0; // Wrap
						}
						else if(arg->textscrollmode == arg->height*arg->scale) // Quick Scroll
						{

#ifdef NEW_QUICK_SCROLL
							// New way (topmost line will be partially scrolled up offscreen, but no color gap under the bottommost text line; VerticalResolution % (height * scale) == 0 fonts don't have to worry all text on screen is the same size)
							uint64_t min_scroll_size = arg->y + 2*arg->height*arg->scale - arg->defaultGPU.Info->VerticalResolution;
							arg->y = arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale;

							AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * min_scroll_size), (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine*4);
							if(arg->background_color != 0xFF000000)
							{
								AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine);
							}
#else
							// Old way (gap of background color below the bottommost text line, but no partial scroll up top--the topmost line goes away; VerticalResolution % (height * scale) == 0 fonts don't have to worry if all text on screen is the same size)
							// Qualitative test results: This can scroll a 4K screen framebuffer (31MB) extremely quickly :D (Interestingly enough, the standard memmove in memmove.c can also do it pretty quickly since GCC vectorizes it.)
							AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * arg->height * arg->scale), arg->y * arg->defaultGPU.Info->PixelsPerScanLine*4);
							if(arg->background_color != 0xFF000000)
							{
								AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->y * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, (arg->defaultGPU.Info->VerticalResolution - arg->y) * arg->defaultGPU.Info->PixelsPerScanLine);
							}
#endif

						}
						else if(arg->textscrollmode == arg->defaultGPU.Info->VerticalResolution) // Screen wipe
						{
							if(arg->background_color != 0xFF000000)
							{
								AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, arg->background_color, arg->defaultGPU.Info->VerticalResolution * arg->defaultGPU.Info->PixelsPerScanLine);
							}
						}
						else // Smooth scroll
						{
							uint64_t min_scroll_size = arg->y + 2*arg->height*arg->scale - arg->defaultGPU.Info->VerticalResolution;
							arg->y = arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale;
							// This offset correction is needed in case a font size/scale combination is not an integer multiple of the vertical resolution.
							// Even if it is, changing scales or arg->y could cause a variable offset and that needs to be accounted for.

							for(uint64_t smooth = 0; smooth < min_scroll_size; smooth += arg->textscrollmode) // Random: (smooth --> 0) is the same as ((smooth--) > 0); It may not be obvious that they're the same at first glance.
							{
								AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * arg->textscrollmode), (arg->defaultGPU.Info->VerticalResolution - arg->textscrollmode - smooth) * arg->defaultGPU.Info->PixelsPerScanLine * 4);
								if(arg->background_color != 0xFF000000)
								{
									AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + (arg->defaultGPU.Info->VerticalResolution - arg->textscrollmode - smooth) * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, arg->textscrollmode*arg->defaultGPU.Info->PixelsPerScanLine);
								}
							}
						}
					}
					else
					{
						arg->y += arg->height * arg->scale; // Horizontal wrap means vertical goes down one line -- NOTE: The output render already accounts for PixelsPerScanLine offsets.
					}
				}
			}
			break;
		default:
			Output_render_text(arg->defaultGPU, output_character, arg->height, arg->width, arg->font_color, arg->highlight_color, arg->x, arg->y, arg->scale, arg->index);
	//  	Output_render(Global_Print_Info.defaultGPU, output_character, Global_Print_Info.height, Global_Print_Info.width, Global_Print_Info.font_color, Global_Print_Info.highlight_color, Global_Print_Info.x, Global_Print_Info.y, Global_Print_Info.scale, Global_Print_Info.index);

			arg->index++; // Increment global character index
	//  	Global_Print_Info.index++; // This should do the same thing.

			if(arg->index * arg->width * arg->scale > (arg->defaultGPU.Info->HorizontalResolution - arg->width * arg->scale)) // Check if text is running off screen
			{
				arg->index = 0; // Horizontal wraparound
				if((arg->y + arg->height * arg->scale) > (arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale)) // Vertical wraparound if hit the bottom of the screen
				{
					if(!arg->textscrollmode)
					{
						arg->y = 0; // Wrap
					}
					else if(arg->textscrollmode == arg->height*arg->scale) // Quick Scroll
					{

#ifdef NEW_QUICK_SCROLL
						// New way (topmost line will be partially scrolled up offscreen, but no color gap under the bottommost text line; VerticalResolution % (height * scale) == 0 fonts don't have to worry all text on screen is the same size)
						uint64_t min_scroll_size = arg->y + 2*arg->height*arg->scale - arg->defaultGPU.Info->VerticalResolution;
						arg->y = arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale;

						AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * min_scroll_size), (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine*4);
						if(arg->background_color != 0xFF000000)
						{
							AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, (arg->defaultGPU.Info->VerticalResolution - min_scroll_size) * arg->defaultGPU.Info->PixelsPerScanLine);
						}
#else
						// Old way (gap of background color below the bottommost text line, but no partial scroll up top--the topmost line goes away; VerticalResolution % (height * scale) == 0 fonts don't have to worry if all text on screen is the same size)
						// Qualitative test results: This can scroll a 4K screen framebuffer (31MB) extremely quickly :D (Interestingly enough, the standard memmove in memmove.c can also do it pretty quickly since GCC vectorizes it.)
						AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * arg->height * arg->scale), arg->y * arg->defaultGPU.Info->PixelsPerScanLine*4);
						if(arg->background_color != 0xFF000000)
						{
							AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->y * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, (arg->defaultGPU.Info->VerticalResolution - arg->y) * arg->defaultGPU.Info->PixelsPerScanLine);
						}
#endif

					}
					else if(arg->textscrollmode == arg->defaultGPU.Info->VerticalResolution) // Screen wipe
					{
						if(arg->background_color != 0xFF000000)
						{
							AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, arg->background_color, arg->defaultGPU.Info->VerticalResolution * arg->defaultGPU.Info->PixelsPerScanLine);
						}
					}
					else // Smooth scroll
					{
						uint64_t min_scroll_size = arg->y + 2*arg->height*arg->scale - arg->defaultGPU.Info->VerticalResolution;
						arg->y = arg->defaultGPU.Info->VerticalResolution - arg->height * arg->scale;
						// This offset correction is needed in case a font size/scale combination is not an integer multiple of the vertical resolution.
						// Even if it is, changing scales or arg->y could cause a variable offset and that needs to be accounted for.

						for(uint64_t smooth = 0; smooth < min_scroll_size; smooth += arg->textscrollmode) // Random: (smooth --> 0) is the same as ((smooth--) > 0); It may not be obvious that they're the same at first glance.
						{
							AVX_memmove((EFI_PHYSICAL_ADDRESS*)arg->defaultGPU.FrameBufferBase, (EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + arg->defaultGPU.Info->PixelsPerScanLine * 4 * arg->textscrollmode), (arg->defaultGPU.Info->VerticalResolution - arg->textscrollmode - smooth) * arg->defaultGPU.Info->PixelsPerScanLine * 4);
							if(arg->background_color != 0xFF000000)
							{
								AVX_memset_4B((EFI_PHYSICAL_ADDRESS*)(arg->defaultGPU.FrameBufferBase + (arg->defaultGPU.Info->VerticalResolution - arg->textscrollmode - smooth) * arg->defaultGPU.Info->PixelsPerScanLine * 4), arg->background_color, arg->textscrollmode*arg->defaultGPU.Info->PixelsPerScanLine);
							}
						}
					}
				}
				else
				{
					arg->y += arg->height * arg->scale; // Horizontal wrap means vertical goes down one line -- NOTE: The output render already accounts for PixelsPerScanLine offsets.
				}
			}
			break;
	}
}

// Now we can define a real printf()!
int printf(const char *fmt, ...)
{
	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = kvprintf(fmt, printf_putchar, &Global_Print_Info, 10, ap); // The third argument is any arguments to be passed to putchar (e.g. &pca - putchar args)
// retval = kvprintf(fmt, printf_putchar, NULL, 10, ap); // This could work, too (requires using similarly commented code in printf_putchar... Which would be somewhat of a hassle at this point. All those arg->whatever would need to be changed, too!!)
	va_end(ap);

	return (retval);
}

// This also seems useful.
int vprintf(const char *fmt, va_list ap)
{
	int retval;

	retval = kvprintf(fmt, printf_putchar, &Global_Print_Info, 10, ap);

	return (retval);
}

// Likewise a real sprintf()!
/*
 * Scaled down version of sprintf(3).
 */
int sprintf(char *buf, const char *cfmt, ...)
{
	int retval;
	va_list ap;

	va_start(ap, cfmt);
	retval = kvprintf(cfmt, NULL, (void *)buf, 10, ap);
	buf[retval] = '\0';
	va_end(ap);
	return (retval);
}

/*
 * Scaled down version of vsprintf(3).
 */
int vsprintf(char *buf, const char *cfmt, va_list ap)
{
	int retval;

	retval = kvprintf(cfmt, NULL, (void *)buf, 10, ap);
	buf[retval] = '\0';
	return (retval);
}

// A print function that's meant to print simple UCS-2 UEFI strings (2 bytes per character) with a 1-byte per character/UTF8 font.
// Meant to print out CHAR16 strings in the loader params, since printf above doesn't support wide characters.
void print_utf16_as_utf8(CHAR16 * strung, UINT64 size)
{
  for(uint64_t letter = 0; letter < size; letter++)
  {
    if( ((char*)strung)[letter] != 0x00)
    {
      printf("%c", ((char *)strung)[letter]);
    }
  }
}

// Takes simple CHAR16 UEFI strings and returns a CHAR8 string of the same contents.
// Meant to work with CHAR16 strings in the loader params, since printf above doesn't support wide characters.
/*
char * UCS2_to_UTF8(CHAR16 * strang, UINT64 size) // TODO: need malloc
{
  char * new_strang = malloc(size >> 1);
  uint8_t zero_count = 0;
  uint64_t new_letter = 0;

  for(uint64_t letter = 0; letter < size; letter++)
  {
    if( ((char*)strang)[letter] != 0x00)
    {
      new_strang[new_letter] = ((char*)strang)[letter];
      new_letter++;
      zero_count = 0;
    }
    else
    {
      zero_count++;
    }

    if(zero_count == 2) // Double 0 is L'\0'
    {
      new_strang[new_letter] = '\0';
    }
  }

  return new_strang;
}
*/
