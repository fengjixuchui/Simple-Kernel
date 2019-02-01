## Simple-Kernel
A minimal, cross-platform development environment for building bare-metal x86-64 programs. It is primarily designed to make programs for use with https://github.com/KNNSpeed/Simple-UEFI-Bootloader.

**Version 0.y**

This build system compiles native executables for the builder's platform (Windows, Mac, or Linux) that can be loaded by the bootloader. A sample kernel containing a software renderer, text output, and multi-GPU graphical support is also included in this repository.

See "Issues" for my to-do list for the included sample kernel, and see the "Releases" tab of this project for executable binary forms of it.  
  
*Apologies to AMD Ryzen users: I can't test Ryzen binaries as I don't have any Ryzen machines. The build scripts are provided with the caveat that they ought to produce usable binaries (I don't see why they wouldn't).*

**Building a Program**

See the below "How to Build from Source" section for complete compilation instructions for each platform, and then all you need to do is put your code in "src" and "inc" in place of mine (leave the "startup" folder as-is). Once compiled, your program can be run in the same way as described in the "Releases" section of https://github.com/KNNSpeed/Simple-UEFI-Bootloader using a UEFI-supporting VM like Hyper-V or on actual hardware.

***Important Points to Consider:***

The entry point function (i.e. the "main" function) of your program should look like this, otherwise the kernel will fail to run:  

```
void kernel_main(LOADER_PARAMS * LP) // Loader Parameters  
{  

}
```  

The LOADER_PARAMS data type is defined as the following structure:
```
typedef struct {
  EFI_MEMORY_DESCRIPTOR  *Memory_Map;
  EFI_RUNTIME_SERVICES   *RTServices;
  GPU_CONFIG             *GPU_Configs;
  EFI_FILE_INFO          *FileMeta;
  void                   *RSDP;
} LOADER_PARAMS;
```

Of those pointers, the only data type not defined by UEFI spec is `GPU_CONFIG`, which looks like this:

```
typedef struct {
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  *GPUArray; // This array contains the EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE structures for each available framebuffer
  UINT64                              NumberOfFrameBuffers; // The number of pointers in the array (== the number of available framebuffers)
} GPU_CONFIG;
```

You will find some relevant structures defined in "Kernel64.h" of the sample kernel, with the rest defined in the "EfiBind.h" and "EfiTypes.h" files in the "startup" directory.

You will also need to `#include` the "Efi" files from "startup" in your code: refer to the "Kernel64.h" file in the "inc" directory for an example. You may find it easiest to just ```#include "Kernel64.h"``` in your code after removing any unnecessary function prototypes from the file, as it already has all the requisite inclusions and EFI structures for LOADER_PARAMS defined within it.

**Target System Requirements**  

*These are the same as the bootloader's requirements. If your target system can run the bootloader, you're all set.*  

- x86-64 architecture  
- Secure Boot must be disabled  
- More than 4GB RAM (though it seems to work OK with less, e.g. Hyper-V with only 1GB)  
- A graphics card (Intel, AMD, NVidia, etc.) **with UEFI GOP support**  
- A keyboard  

The earliest GPUs with UEFI GOP support were released around the Radeon HD 7xxx series (~2011). Anything that age or newer should have UEFI GOP support, though older models, like early 7970s, required owners to contact GPU vendors to get UEFI-compatible firmware. On Windows, you can check if your graphics card(s) have UEFI GOP support by downloading TechPowerUp's GPU-Z utility and seeing whether or not the UEFI checkbox is checked. If it is, you're all set!  

*NOTE: You need to check each graphics card if there is a mix, as you will only be able to use the ones with UEFI GOP support. Per the system requirements above, you need at least one compliant device.*  

**License and Crediting**  

***TL;DR:***  

Effectively PD (Public Domain) for all code in this repository not already covered by a license (i.e. my original source code), **as long as you give proper credit to this project.** See below for an example of what that might look like--more examples are included in the LICENSE file. If you don't give credit to this project, per the license you aren't allowed to use it. That's pretty much it (and why it's "effectively" or "almost" PD, or "PD with Credit" if I have to give it a nickname).  

Please see the LICENSE file for further information on all licenses covering code created for and used in this project.  

*Example Citation:*  

From KNNSpeed's "Simple Kernel":  
https://github.com/KNNSpeed/Simple-Kernel  
V0.y, [Date you got it]  

***Slightly More Detailed License Summary:***

If you want to use, copy, modify, and/or distribute this project's original source code, in other words the code in this repository not already covered under any license, simply copy & paste the below 3 lines somewhere reasonable like in an acknowledgements or references section, as a comment in the code, at the bottom of a README or in a LICENSE file, etc. Then, change "[Date you got it]" to the date you acquired the code, and don't sue me if something goes wrong - especially since there's no warranty (and sometimes firmware vendors just don't follow the UEFI spec in unforeseen ways, but it would be great if you posted an issue so I could fix it!). Thanks!

From KNNSpeed's "Simple Kernel":  
https://github.com/KNNSpeed/Simple-Kernel  
V0.y, [Date you got it]  

(As mentioned in the TL;DR, please see the LICENSE file for further information on all licenses covering code created for and used in this project.)  

**How to Build from Source**  

Windows: Requires MinGW-w64 based on GCC 8.1.0 or later  
Mac: Requires Mac OS Sierra or later with the latest XCode Command Line Tools for the OS  
Linux: Requires GCC 8.0.0 or later and Binutils 2.29.1 or later  

I cannot make any guarantees whatsoever for earlier versions, especially with the number of compilation and linking flags used.  

***Windows:***  
1. Download and extract or clone this repository into a dedicated folder, preferably somewhere easy like C:\BareMetalx64

2. Download MinGW-w64 "x86_64-posix-seh" from https://sourceforge.net/projects/mingw-w64/ (click "Files" and scroll down - pay attention to the version numbers!).

3. Extract the archive into the "Backend" folder.

4. Open Windows PowerShell or the Command Prompt in the "Simple-Kernel" folder and type ".\Compile.bat"

    *That's it! It should compile and a binary called "Kernel64.exe" will be output into the "Backend" folder.*

***Mac:***  
1. Download and extract or clone this repository into a dedicated folder, preferably somewhere easy like ~/BareMetalx64

2. Open Terminal in the "Simple-Kernel" folder and run "./Compile-Mac.sh"

    *That's it! It should compile and a binary called "Kernel64.mach64" will be output into the "Backend" folder.*

***Linux:***  

1. Download and extract or clone this repository into a dedicated folder, preferably somewhere easy like ~/BareMetalx64

2. If, in the terminal, "gcc --version" reports GCC 8.0.0 or later and "ld --version" reports 2.29.1 or later, do steps 2a, 2b, and 2c. Otherwise go to step 3.

    2a. Type "which gcc" in the terminal, and make a note of what it says (something like /usr/bin/gcc or /usr/local/bin/gcc)

    2b. Open Compile.sh in an editor of your choice (nano, gedit, vim, etc.) and set the GCC_FOLDER_NAME variable at the top to be the part before "bin" (e.g. /usr or /usr/local, without the last slash). Do the same thing for BINUTILS_FOLDER_NAME, except use the output of "which ld" to get the directory path preceding "bin" instead.

    2c. Now set the terminal to the Simple-Kernel folder and run "./Compile.sh", which should work and output Kernel64.elf in the Backend folder. *That's it!*

3. Looks like we need to build GCC & Binutils. Navigate to the "Backend" folder in terminal and do "git clone git://gcc.gnu.org/git/gcc.git" there. This will download a copy of GCC 8.0.0, which is necessary for "static-pie" support (when combined with Binutils 2.29.1 or later, it allows  statically-linked, position-independent executables to be created; earlier versions do not). If that git link ever changes, you'll need to find wherever the official GCC git repository ran off to.

4. Once GCC has been cloned, in the cloned folder do "contrib/download_prerequisites" and then "./configure -v --build=x86_64-linux-gnu --host=x86_64-linux-gnu --target=x86_64-linux-gnu --prefix=$PWD/../gcc-8 --enable-checking=release --enable-languages=c --disable-multilib"

    NOTE: If you want, you can enable other languages like c++, fortran, objective-c (objc), go, etc. with enable-languages. You can also change the name of the folder it will built into by changing --prefix=[desired folder]. The above command line will configure GCC to be made in a folder called gcc-8 inside the "Backend" folder. Be aware that --prefix requires an absolute path.

5. After configuration completes, do "make -j [twice the number of cores of your CPU]" and go eat lunch. Unfortunately, sometimes building the latest GCC produces build-time errors; I ran into an "aclocal-1.15" issue when building via Linux on Windows (fixed by installing the latest version of Ubuntu on Windows and using the latest autoconf).

6. Now just do "make install" and GCC will be put into the gcc-8 folder from step 4.

7. Next, grab binutils 2.29.1 or later from https://ftp.gnu.org/gnu/binutils/ and extract the archive to Backend.

8. In the extracted Binutils folder, do "mkdir build" and "cd build" before configuring with "../configure --prefix=$PWD/../binutils-binaries --enable-gold --enable-ld=default --enable-plugins --enable-shared --disable-werror"

    NOTE: The "prefix" flag means the same thing as GCC's.

9. Once configuration is finished, do "make -j [twice the number of CPU cores]" and go have dinner.

10. Once make is done making, do "make -k check" and do a crossword or something. There should be a very small number of errors, if any.

11. Finally, do "make install" to install the package into binutils-binaries. Congratulations, you've just built some of the biggest Linux sources ever!

12. Open Compile.sh in an editor of your choice (nano, gedit, vim, etc.) and set the GCC_FOLDER_NAME variable at the top (e.g. gcc-8 without any slashes). Do the same thing for the BINUTILS_FOLDER_NAME, except use the binutils-binaries folder.

13. At long last, you should be able to run "./Compile.sh" from within the "Simple-Kernel" folder.

    *That's it! It should compile and a binary called "Kernel64.elf" will be output into the "Backend" folder.*

    For more information about building GCC and Binutils, see these: http://www.linuxfromscratch.org/blfs/view/cvs/general/gcc.html & http://www.linuxfromscratch.org/lfs/view/development/chapter06/binutils.html  

**Change Log**  

V0.y (2/1/2019) - Major code cleanup, added printf() and a whole host of text-displaying functions, resolved issues #5 and #6. No new binaries will be made for this version.

V0.x (2/2/2018) - Initial upload of environment and compilable sample. Not yet given a version number.  

**Acknowledgements**  

- [Marcel Sondaar](https://mysticos.combuster.nl/) for the original public domain 8x8 font
- [Daniel Hepper](https://github.com/dhepper/) for converting the 8x8 font into [public domain C headers](https://github.com/dhepper/font8x8)
- [Intel Corporation](https://www.intel.com/content/www/us/en/homepage.html) for EfiTypes.h, the x86-64 EfiBind.h, and EfiError.h (the ones used in this project are derived from [TianoCore EDK II](https://github.com/tianocore/edk2/))
- [UEFI Forum](http://www.uefi.org/) for the [UEFI Specification Version 2.7 (Errata A)](http://www.uefi.org/sites/default/files/resources/UEFI%20Spec%202_7_A%20Sept%206.pdf), as well as for [previous UEFI 2.x specifications](http://www.uefi.org/specifications)
- [PhoenixWiki](http://wiki.phoenix.com/wiki/index.php/Category:UEFI) for very handy documentation on UEFI functions
- [The GNU project](https://www.gnu.org/home.en.html) for [GCC](https://gcc.gnu.org/), a fantastic and versatile compiler, and [Binutils](https://www.gnu.org/software/binutils/), equally fantastic binary utilities
- [MinGW-w64](https://mingw-w64.org/doku.php) for porting GCC to Windows
- [LLVM](https://llvm.org/) for providing a feature-rich and efficient native compiler suite for Apple platforms
