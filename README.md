# Simple-Kernel
A bare-metal x86-64 kernel for use with https://github.com/KNNSpeed/Simple-UEFI-Bootloader

**Coming Soon** -- See the "Releases" tab for some samples that can be used with the above bootloader.

Current Status: Got font rendering to work and string output. Need to implement formatted text output (i.e. printf's '%' modifier), and clean up the code.

The currently uploaded code will not compile as-is. Kernel64.c is just here to show that this part of the project does also exist.   
  
A full description of how to actually compile it, etc. will be added once I finish my to-do list and clean up the mess of code. See the "Issues" section of this repo for some things on my to-do list.

Note: As far as licensing is concerned, this will be licensed like https://github.com/KNNSpeed/Simple-UEFI-Bootloader, so here's a copy-pasteable citation:

From KNNSpeed's "Simple Kernel":  
https://github.com/KNNSpeed/Simple-Kernel  
V0.x, [Date you got it]  

**How to Build from Source:**  
  
Windows: Requires MinGW-w64 based on GCC 7.1.0 or later  
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

    *That's it! It should compile and a binary called "Kernel64.Mach64" will be output into the "Backend" folder.*
  
***Linux:***  

1. Download and extract or clone this repository into a dedicated folder, preferably somewhere easy like ~/BareMetalx64

2. If, in the terminal, "gcc --version" reports GCC 8.0.0 or later and "ld --version" reports 2.29.1 or later, do steps 2a, 2b, and 2c. Otherwise go to step 3.
    
    2a. Type "which gcc" in the terminal, and make a note of what it says (something like /usr/bin/gcc or /usr/local/bin/gcc)
    
    2b. Open Compile.sh in an editor of your choice (nano, gedit, vim, etc.) and set the GCC_FOLDER_NAME variable at the top to be the part before "bin" (e.g. /usr or /usr/local, without the last slash). Do the same thing for BINUTILS_FOLDER_NAME, except use the output of "which ld" to get the directory path preceding "bin" instead.
    
    2c. Now set the terminal to the Simple-Kernel folder and run "./Compile.sh", which should work and output Kernel64.elf in the Backend folder. *That's it!*

3. Looks like we need to build GCC & Binutils. Navigate to the "Backend" folder in terminal and do "git clone git://gcc.gnu.org/git/gcc.git" there. This will download a copy of GCC 8.0.0, which is necessary for "static-pie" support (when combined wiht Binutils 2.29.1 or later, it allows  statically-linked, position-independent executables to be created; earlier versions do not). If that git link ever changes, you'll need to find wherever the official GCC git repository ran off to.

4. Once GCC has been cloned, in the cloned folder do "./configure -v --build=x86_64-linux-gnu --host=x86_64-linux-gnu --target=x86_64-linux-gnu --prefix=gcc-8 --enable-checking=release --enable-languages=c --disable-multilib"

    NOTE: If you want, you can enable other languages like c++, fortran, objective-c (objc), go, etc. with enable-languages. You can also change the name of the folder it will built into by changing --prefix=[desired folder]. The above command line will configure GCC to be made in a folder called gcc-8 inside the "Backend" folder.

5. After configuration completes, do "make -j [twice the number of cores of your CPU]" and go eat lunch. Unfortunately, sometimes building the latest GCC produces build-time errors; I ran into an "aclocal-1.15" issue when building via Linux on Windows (fixed by installing the latest version of Ubuntu on Windows and using the latest autoconf).

6. Now just do "make install" and GCC will be put into the gcc-8 folder from step 4.

7. Next, grab binutils 2.29.1 or later from https://ftp.gnu.org/gnu/binutils/ and extract the archive to Backend.

8. In the extracted Binutils folder, do "mkdir build" and "cd build" before configuring with "../configure --prefix=binutils-binaries --enable-gold --enable-ld=default --enable-plugins --enable-shared --disable-werror"

    NOTE: The "prefix" flag means the same thing as GCC's.

9. Once configuration is finished, do "make -j [twice the number of CPU cores]" and go have dinner.

10. Once make is done making, do "make -k check" and do a crossword or something. There should be a very small number of errors, if any.

11. Finally, do "make install" to install the package into binutils-binaries. Congrats, you just built some of the biggest Linux sources ever.

12. Open Compile.sh in an editor of your choice (nano, gedit, vim, etc.) and set the GCC_FOLDER_NAME variable at the top (e.g. gcc-8 without any slashes). Do the same thing for the BINUTILS_FOLDER_NAME, except use the binutils-binaries folder.

13. At long last, you should be able to run "./Compile.sh" from within the "Simple-Kernel" folder.

    *That's it! It should compile and a binary called "Kernel64.elf" will be output into the "Backend" folder.*

    For more information about building GCC and Binutils, see these: http://www.linuxfromscratch.org/blfs/view/cvs/general/gcc.html & http://www.linuxfromscratch.org/lfs/view/development/chapter06/binutils.html  
