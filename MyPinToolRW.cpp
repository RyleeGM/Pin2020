#include <iostream>
#include <fstream>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <sstream>
//#include "zlib.h"
#include "time.h"
#include "pin.H"

using std::string;
using std::cerr;
using std::endl;
using std::cout;

//Determine OS to use proper binary mode
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) _setmode(_fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 256 * 1024    //The size of the buffer used in compression
#define MAXINSTRSIZE 97        //The maximum size (in bytes) that a single instruction trace could occupy

/* ================================================================== */
// Global variables
/* ================================================================== */
FILE * outputFile;                        //The external file which contains the header and compressed trace information
UINT64 numInstr = 0;                    //The number of instructions to include in the output file
UINT64 skipPt = 0;                        //The number of initial instructions to exclude from the output file
UINT64 instrCounter = 0;                //The counter used for the total number of instructions traced so far
int compLevel = 0;                        //The int used to tell zLib how much to compress
string rawTraceString;                    //The string which holds traces before they are compressed
string compressedTraceString;            //The string which holds compressed traces before they are written to the file
void * rawTraceBuff;                    //The BYTE array which is compressed by zLib
void * compressedTraceBuff;                //The compressed BYTE array returned by zLib
char * buildBuff;                        //The char buffer used to store traces prior to compression
char * current_spot_in_buildBuff;        //The pointer used in building a trace in the buildBuffer
char * compressionBuff;                    //The char buffer used to store multiple traces before compression
char * current_spot_in_compressionBuff;    //The pointer used to keep track of where to insert new instructions
char * compressedTraces;                //The char buffer used to hold the compressed traces
UINT32 sizeOfCompressionBuff;            //The integer variable used to keep track of how full the compression buffer is
//z_stream strm;                        //The stream initialized at the beginning of the program for compression
int ret;                                //The return value modified by the compression method

const UINT32 MAX_INDEX = 4096;    // enough even for the IA-64 architecture

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,  "pintool",        //Allows for specific naming of the output file
    "o", "Trace.out", "specify file name for MyPinTool output");
KNOB<UINT64> KnobSkipPoint(KNOB_MODE_WRITEONCE, "pintool",            //The knob for telling the program how many instructions to skip
    "skip", "0", "Specify the number of instructions to skip generating a trace for");
KNOB<UINT64> KnobInstrCount(KNOB_MODE_WRITEONCE, "pintool",            //The knob for telling the program how many instructions to generate traces for
    "num", "100000", "Specify the number of instructions to generate a trace for");
KNOB<int> KnobCompressionLevel(KNOB_MODE_WRITEONCE, "pintool",        //The knob for setting the level of compression - defaults to max
    "cmp", "9", "Specify the level of compression (0-9) that is desired");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool generates instruction traces. Included information is:" << endl <<
            "execution bit, instruction pointer, opcode, opcode size," << endl <<
            "registers read, memory locations and sizes read, memory" << endl <<
            "locations and sizes written, and registers written." << endl << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

/**
The method for compressing the traces and writing them to the external file
@param lastTime The boolean that determines whether the call to this method is the final one
*/
//int CompressAndExport(bool lastTime)
//{
//    int newCompSize = 0;
//
//    //Compress the traces
//    int flush;
//    unsigned have;
//
//    unsigned char in[CHUNK];
//    unsigned char out[CHUNK];
//
//    //Copy the contents of the compression buffer to the uchar array
//    for(int i = 0; i < CHUNK; ++i)
//    {
//        in[i] = compressionBuff[i];
//    }
//
//    /* compress until end of file */
//        strm.avail_in = CHUNK;
//
//        if(!lastTime)
//        {
//            flush = Z_NO_FLUSH;
//        }
//        else
//        {
//            flush = Z_FINISH;
//        }
//        strm.next_in = in;
//
//        /* run deflate() on input until output buffer not full, finish
//           compression if all of source has been read in */
//        do {
//            strm.avail_out = CHUNK;
//            strm.next_out = out;
//            ret = deflate(&strm, flush);    /* no bad return value */
//            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
//            have = CHUNK - strm.avail_out;
//            if (fwrite(out, 1, have, outputFile) != have || ferror(outputFile)) {
//                (void)deflateEnd(&strm);
//                return Z_ERRNO;
//            }
//        } while (strm.avail_out == 0);
//        assert(strm.avail_in == 0);     /* all input will be used */
//
//    //Reset the compression buffer & copy overflow to beginning
//    for(int i = 0, j = 0; i < sizeOfCompressionBuff; ++i)
//    {
//        if(i < CHUNK)
//        {
//            compressionBuff[i] = 0;
//        }
//        else
//        {
//            compressionBuff[j] = compressionBuff[i];
//            compressionBuff[i] = 0;
//            ++j;
//            ++newCompSize;
//        }
//    }
//    sizeOfCompressionBuff = newCompSize;
//
//    current_spot_in_compressionBuff = (compressionBuff + sizeOfCompressionBuff);
//}
//
///**
//This is called after each instruction trace. Copies the instruction build buffer to the compression buffer
//*/
//VOID CopyBuffer()
//{
//    //Copy the new instruction to the compression buffer
//    for(char * buildBuffChar = buildBuff; buildBuffChar != current_spot_in_buildBuff; ++buildBuffChar, ++current_spot_in_compressionBuff)
//    {
//        *current_spot_in_compressionBuff = *buildBuffChar;
//        ++sizeOfCompressionBuff;
//    }
//
//    //Check to see if there is room in the compression Buffer for another instruction
//    if(((CHUNK + MAXINSTRSIZE) - sizeOfCompressionBuff) < MAXINSTRSIZE)
//    {
//        //There isn't room, so call CompressAndExport
//        CompressAndExport(false);
//    }
//
//    //Reset the buildBuffer to handle the new instruction
//    for(int i = 0; i < MAXINSTRSIZE; ++i)
//        buildBuff[i] = 0;
//
//    current_spot_in_buildBuff = buildBuff;
//}

/**
Called every instruction trace - used to keep track of how many have been skipped and generated
*/
VOID IncrementCounter()
{
    instrCounter++;
}

/**
Function used to print the execution bit, Instruction Address, Opcode Size, and Opcode.
No registers written after that.
@param ex            The boolean that signifies whether or not the instruction was actually executed
@param *ip            The pointer the instruction's physical address space
@param index        The opcode number
@param instrSize    The opcode size (in bytes)
*/
VOID Reg0Print(BOOL ex, VOID *ip, UINT32 index, UINT32 instrSize)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "%u %p %u %u 0 ", ex, ip, instrSize, index);
        
        //Write execution bit and advance 1 byte in buffer
        //*((bool *)current_spot_in_buildBuff) = ex;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(bool);
        
        //Write instruction address and advance 8 bytes in buffer
        //*((void **)current_spot_in_buildBuff) = ip;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(void *);
        
        //Write Opcode size and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = instrSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write Opcode and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = index;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //No registers written, so write a 0 and advance 1 byte
        //*current_spot_in_buildBuff = 0;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);
    }
}

/**
Function used to print the execution bit, Instruction Address, Opcode Size, and Opcode.
1 register written after that.
@param ex            The boolean that signifies whether or not the instruction was actually executed
@param *ip            The pointer the instruction's physical address space
@param index        The opcode number
@param instrSize    The opcode size (in bytes)
@param reg1num        The number of the register that was read by the instruction
*/
VOID Reg1Print(BOOL ex, VOID *ip, UINT32 index, UINT32 instrSize, UINT32 reg1num)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "%u %p %u %u 1 %u ", ex, ip, instrSize, index, reg1num);
        //Write execution bit and advance 1 byte in buffer
        //*((bool *)current_spot_in_buildBuff) = ex;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(bool);
        
        //Write instruction address and advance 8 bytes in buffer
        //*((void **)current_spot_in_buildBuff) = ip;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(void *);
        
        //Write Opcode size and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = instrSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
        
        //Write Opcode and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = index;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
        
        //1 register written, so write a 1 and advance 1 byte
        //*current_spot_in_buildBuff = 1;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);
        
        //Write the register value and advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = reg1num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
    }
}

/**
Function used to print the execution bit, Instruction Address, Opcode Size, and Opcode.
2 registers written after that.
@param ex            The boolean that signifies whether or not the instruction was actually executed
@param *ip            The pointer the instruction's physical address space
@param index        The opcode number
@param instrSize    The opcode size (in bytes)
@param reg1num        The number of the first register that was read by the instruction
@param reg2num        The number of the second register that was read by the instruction
*/
VOID Reg2Print(BOOL ex, VOID *ip, UINT32 index, UINT32 instrSize, UINT32 reg1num, UINT32 reg2num)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "%u %p %u %u 2 %u %u ", ex, ip, instrSize, index, reg1num, reg2num);
        //Write execution bit and advance 1 byte in buffer
        //*((bool *)current_spot_in_buildBuff) = ex;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(bool);
        
        //Write instruction address and advance 8 bytes in buffer
        //*((void **)current_spot_in_buildBuff) = ip;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(void *);
        
        //Write Opcode size and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = instrSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
        
        //Write Opcode and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = index;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
        
        //2 registers written, so write a 2 and advance 1 byte
        //*current_spot_in_buildBuff = 2;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);
        
        //Write the register values, advancing 4 bytes each value
        //*((UINT32 *)current_spot_in_buildBuff) = reg1num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
        
        //*((UINT32 *)current_spot_in_buildBuff) = reg2num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
    }
}

/**
Function used to print the execution bit, Instruction Address, Opcode Size, and Opcode.
3 registers written after that.
@param ex            The boolean that signifies whether or not the instruction was actually executed
@param *ip            The pointer the instruction's physical address space
@param index        The opcode number
@param instrSize    The opcode size (in bytes)
@param reg1num        The number of the first register that was read by the instruction
@param reg2num        The number of the second register that was read by the instruction
@param reg3num        The number of the third register that was read by the instruction
*/
VOID Reg3Print(BOOL ex, VOID *ip, UINT32 index, UINT32 instrSize, UINT32 reg1num, UINT32 reg2num, UINT32 reg3num)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "%u %p %u %u 3 %u %u %u ", ex, ip, instrSize, index, reg1num, reg2num, reg3num);
        //Write execution bit and advance 1 byte in buffer
        //*((bool *)current_spot_in_buildBuff) = ex;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(bool);
        
        //Write instruction address and advance 8 bytes in buffer
        //*((void **)current_spot_in_buildBuff) = ip;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(void *);

        //Write Opcode size and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = instrSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
        
        //Write Opcode and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = index;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //3 registers written, so write a 3 and advance 1 byte
        //*current_spot_in_buildBuff = 3;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the register values, advancing 4 bytes each value
        //*((UINT32 *)current_spot_in_buildBuff) = reg1num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //*((UINT32 *)current_spot_in_buildBuff) = reg2num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //*((UINT32 *)current_spot_in_buildBuff) = reg3num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
    }
}
/**
Function used to print the execution bit, Instruction Address, Opcode Size, and Opcode.
4 registers written after that.
@param ex            The boolean that signifies whether or not the instruction was actually executed
@param *ip            The pointer the instruction's physical address space
@param index        The opcode number
@param instrSize    The opcode size (in bytes)
@param reg1num        The number of the first register that was read by the instruction
@param reg2num        The number of the second register that was read by the instruction
@param reg3num        The number of the third register that was read by the instruction
@param reg4num        The number of the fourth register that was read by the instruction
*/
VOID Reg4Print(BOOL ex, VOID *ip, UINT32 index, UINT32 instrSize, UINT32 reg1num, UINT32 reg2num, UINT32 reg3num, UINT32 reg4num)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "%u %p %u %u 4 %u %u %u %u ", ex, ip, instrSize, index, reg1num, reg2num, reg3num, reg4num);
        //Write execution bit and advance 1 byte in buffer
        //*((bool *)current_spot_in_buildBuff) = ex;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(bool);
        
        //Write instruction address and advance 8 bytes in buffer
        //*((void **)current_spot_in_buildBuff) = ip;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(void *);

        //Write Opcode size and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = instrSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write Opcode and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = index;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //4 registers written, so write a 4 and advance 1 byte
        //*current_spot_in_buildBuff = 4;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the register values, advancing 4 bytes each value
        //*((UINT32 *)current_spot_in_buildBuff) = reg1num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
        
        //*((UINT32 *)current_spot_in_buildBuff) = reg2num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
        
        //*((UINT32 *)current_spot_in_buildBuff) = reg3num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //*((UINT32 *)current_spot_in_buildBuff) = reg4num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
    }
}

/**
Function used to print the execution bit, Instruction Address, Opcode Size, and Opcode.
5 registers written after that.
@param ex            The boolean that signifies whether or not the instruction was actually executed
@param *ip            The pointer the instruction's physical address space
@param index        The opcode number
@param instrSize    The opcode size (in bytes)
@param reg1num        The number of the first register that was read by the instruction
@param reg2num        The number of the second register that was read by the instruction
@param reg3num        The number of the third register that was read by the instruction
@param reg4num        The number of the fourth register that was read by the instruction
@param reg5num        The number of the fifth register that was read by the instruction
*/
VOID Reg5Print(BOOL ex, VOID *ip, UINT32 index, UINT32 instrSize, UINT32 reg1num, UINT32 reg2num, UINT32 reg3num, UINT32 reg4num, UINT32 reg5num)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "%u %p %u %u 5 %u %u %u %u %u ", ex, ip, instrSize, index, reg1num, reg2num, reg3num, reg4num, reg5num);
        //Write execution bit and advance 1 byte in buffer
        //*((bool *)current_spot_in_buildBuff) = ex;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(bool);
        
        //Write instruction address and advance 8 bytes in buffer
        //*((void **)current_spot_in_buildBuff) = ip;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(void *);

        //Write Opcode size and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = instrSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
        
        //Write Opcode and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = index;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //5 registers written, so write a 5 and advance 1 byte
        //*current_spot_in_buildBuff = 5;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the register values, advancing 4 bytes each value
        //*((UINT32 *)current_spot_in_buildBuff) = reg1num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //*((UINT32 *)current_spot_in_buildBuff) = reg2num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //*((UINT32 *)current_spot_in_buildBuff) = reg3num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //*((UINT32 *)current_spot_in_buildBuff) = reg4num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //*((UINT32 *)current_spot_in_buildBuff) = reg5num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
    }
}

/**
Function used to print the execution bit, Instruction Address, Opcode Size, and Opcode.
6 registers written after that.
@param ex            The boolean that signifies whether or not the instruction was actually executed
@param *ip            The pointer the instruction's physical address space
@param index        The opcode number
@param instrSize    The opcode size (in bytes)
@param reg1num        The number of the first register that was read by the instruction
@param reg2num        The number of the second register that was read by the instruction
@param reg3num        The number of the third register that was read by the instruction
@param reg4num        The number of the fourth register that was read by the instruction
@param reg5num        The number of the fifth register that was read by the instruction
@param reg6num        The number of the sixth register that was read by the instruction
*/
VOID Reg6Print(BOOL ex, VOID *ip, UINT32 index, UINT32 instrSize, UINT32 reg1num, UINT32 reg2num, UINT32 reg3num, UINT32 reg4num, UINT32 reg5num, UINT32 reg6num)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "%u %p %u %u 6 %u %u %u %u %u %u ", ex, ip, instrSize, index, reg1num, reg2num, reg3num, reg4num, reg5num, reg6num);
        //Write execution bit and advance 1 byte in buffer
        //*((bool *)current_spot_in_buildBuff) = ex;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(bool);
        
        //Write instruction address and advance 8 bytes in buffer
        //*((void **)current_spot_in_buildBuff) = ip;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(void *);

        //Write Opcode size and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = instrSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write Opcode and advance 4 bytes in buffer
        //*((UINT32 *)current_spot_in_buildBuff) = index;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //6 registers written, so write a 6 and advance 1 byte
        //*current_spot_in_buildBuff = 6;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the register values, advancing 4 bytes each value
        //*((UINT32 *)current_spot_in_buildBuff) = reg1num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //*((UINT32 *)current_spot_in_buildBuff) = reg2num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //*((UINT32 *)current_spot_in_buildBuff) = reg3num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //*((UINT32 *)current_spot_in_buildBuff) = reg4num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //*((UINT32 *)current_spot_in_buildBuff) = reg5num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //*((UINT32 *)current_spot_in_buildBuff) = reg6num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
    }
}

/**
Function called when there are no memory operands (read or write)
*/
VOID Mem0Print()
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "0 0 ");
        //Write a zero to designate 0 memory reads and advance 1 byte
        //*current_spot_in_buildBuff = 0;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write a zero to designate 0 memory writes and advance 1 byte
        //*current_spot_in_buildBuff = 0;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);
    }
}

/**
Function called when 1 memory address is read, none written
@param memRAddr    The address of the memory read
@param memRSize    The size of memory that was read in (in bytes)
*/
VOID Mem1RPrint(ADDRINT memRAddr, UINT32 memRSize)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "1 %lx %u 0 ", memRAddr, memRSize);
        //1 memory location read, write a 1 and advance 1 byte
        //*current_spot_in_buildBuff = 1;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the address of the memory read and advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = memRAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the memory read and advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = memRSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //No memory written, write a 0 and advance 1 byte
        //*current_spot_in_buildBuff = 0;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);
    }
}

/**
The function called when 0 memory reads, 1 memory write
@param memWAddr The address of the memory being written
@param memWSize The size of the memory write (in bytes)
*/
VOID Mem1WPrint(ADDRINT memWAddr, UINT32 memWSize)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "0 1 %lx %u ", memWAddr, memWSize);
        //No memory read, write a 0 and advance 1 byte
        //*current_spot_in_buildBuff = 0;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //1 memory write, write a 1 and advance 1 byte
        //*current_spot_in_buildBuff = 1;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the address of the memory write and advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = memWAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the memory write and advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = memWSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
    }
}

/**
Function used when there are 2 memory reads, no writes
@param mem1RAddr    The address of the first memory read
@param mem1RSize    The size of the first memory read
@param mem2RAddr    The address of the second memory read
@param mem2RSize    The size of the second memory read
*/
VOID Mem2RRPrint(ADDRINT mem1RAddr, UINT32 mem1RSize, ADDRINT mem2RAddr, UINT32 mem2RSize)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "2 %lx %u %lx %u 0 ", mem1RAddr, mem1RSize, mem2RAddr, mem2RSize);
        //2 memory reads, write a 2 and advance 1 byte
        //*current_spot_in_buildBuff = 2;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the address of the first memory read and advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem1RAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the first memory read and advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem1RSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write the address of the second memory read and advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem2RAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the second memory read and advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem2RSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //No memory writes, write a 0 and advance 1 byte
        //*current_spot_in_buildBuff = 0;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);
    }
}

/**
Function used when the memory operand is read, the second is written
@param mem1RAddr    The address of the first memory operand (read)
@param mem1RSize    The size of the first memory operand (read)
@param mem2WAddr    The address of the second memory operand (written)
@param mem2WSize    The size of the second memory operand (written)
*/
VOID Mem2RWPrint(ADDRINT mem1RAddr, UINT32 mem1RSize, ADDRINT mem2WAddr, UINT32 mem2WSize)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "1 %lx %u 1 %lx %u ", mem1RAddr, mem1RSize, mem2WAddr, mem2WSize);
        //1 memory read, write a 1 and advance 1 byte
        //*current_spot_in_buildBuff = 1;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the address of the first memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem1RAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the first memory operand, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem1RSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //1 memory write, write a 1 and advance 1 byte
        //*current_spot_in_buildBuff = 1;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the address of the second memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem2WAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the second memory operand, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem2WSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
    }
}

/**
Function used when the first memory operand is written, second is read
@param mem1WAddr    The address of the first memory operand (written)
@param mem1WSize    The size of the first memory operand (written)
@param mem2RAddr    The address of the second memory operand (read)
@param mem2RSize    The size of the second memory operand (read)
*/
VOID Mem2WRPrint(ADDRINT mem1WAddr, UINT32 mem1WSize, ADDRINT mem2RAddr, UINT32 mem2RSize)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "1 %lx %u 1 %lx %u ", mem2RAddr, mem2RSize, mem1WAddr, mem1WSize);
        //1 memory read, write a 1 and advance 1 byte
        //*current_spot_in_buildBuff = 1;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the address  of the second memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem2RAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the second memory operand, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem2RSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //1 memory write, write a 1 and advance 1 byte
        //*current_spot_in_buildBuff = 1;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the address of the first memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem1WAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the first memory operand, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem1WSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
    }
}

/**
Function used when memory operands 1 and 2 are read, 3 is written
@param mem1RAddr    The address of the first memory operand (read)
@param mem1RSize    The size of the first memory operand (read)
@param mem2RAddr    The address of the second memory operand (read)
@param mem2RSize    The size of the second memory operand (read)
@param mem3WAddr    The address of the third memory operand (written)
@param mem3WSize    The size of the third memory operand (written)
*/
VOID Mem3RRWPrint(ADDRINT mem1RAddr, UINT32 mem1RSize, ADDRINT mem2RAddr, UINT32 mem2RSize, ADDRINT mem3WAddr, UINT32 mem3WSize)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "2 %lx %u %lx %u 1 %lx %u ", mem1RAddr, mem1RSize, mem2RAddr, mem2RSize, mem3WAddr, mem3WSize);
        //2 memory reads, write a 2 and advance 1 byte
        //*current_spot_in_buildBuff = 2;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);
        
        //Write the address of the first memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem1RAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write hte size of the first memory operand, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem1RSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write the address of the second memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem2RAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the second memory operand, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem2RSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //1 memory write, write a 1 and advance 1 byte
        //*current_spot_in_buildBuff = 1;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the address of the third memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem3WAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the third memory operand, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem3WSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
    }
}

/**
Function used when memory operands 1 and 3 are read, 2 is written
@param mem1RAddr    The address of the first memory operand (read)
@param mem1RSize    The size of the first memory operand (read)
@param mem2WAddr    The address of the second memory operand (written)
@param mem2WSize    The size of the second memory operand (written)
@param mem3RAddr    The address of the third memory operand (read)
@param mem3RSize    The size of the third memory operand (read)
*/
VOID Mem3RWRPrint(ADDRINT mem1RAddr, UINT32 mem1RSize, ADDRINT mem2WAddr, UINT32 mem2WSize, ADDRINT mem3RAddr, UINT32 mem3RSize)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "2 %lx %u %lx %u 1 %lx %u ", mem1RAddr, mem1RSize, mem3RAddr, mem3RSize, mem2WAddr, mem2WSize);
        //2 memory reads, write a 2 and advance 1 byte
        //*current_spot_in_buildBuff = 2;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the address of the first memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem1RAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the first memory operand, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem1RSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write the address of the third memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem3RAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the third memory operand, advance 8 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem3RSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //1 memory write, write a 1 and advance 1 byte
        //*current_spot_in_buildBuff = 1;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the address of the second memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem2WAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the second memory operand, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem2WSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
    }
}

/**
Function used when memory operands 2 and 3 are read, 1 is written
@param mem1WAddr    The address of the first memory operand (written)
@param mem1WSize    The size of the first memory operand (written)
@param mem2RAddr    The address of the second memory operand (read)
@param mem2RSize    The size of the second memory operand (read)
@param mem3RAddr    The address of the third memory operand (read)
@param mem3RSize    the size of the third memory operand (read)
*/
VOID Mem3WRRPrint(ADDRINT mem1WAddr, UINT32 mem1WSize, ADDRINT mem2RAddr, UINT32 mem2RSize, ADDRINT mem3RAddr, UINT32 mem3RSize)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "2 %lx %u %lx %u 1 %lx %u ", mem2RAddr, mem2RSize, mem3RAddr, mem3RSize, mem3RAddr, mem3RSize);
        //2 memory reads, write a 2 and advance 1 byte
        //*current_spot_in_buildBuff = 2;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the address of the second memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem2RAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the second memory operand, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem2RSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write the address of the third memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem3RAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the third memory operand, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem3RSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //1 memory write, write a 1 and advance 1 byte
        //*current_spot_in_buildBuff = 1;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the address of the first memory operand, advance 8 bytes
        //*((ADDRINT *)current_spot_in_buildBuff) = mem1WAddr;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(ADDRINT);

        //Write the size of the first memory operand, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = mem1WSize;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);
    }
}

/**
Function used when there are no registers written
*/
VOID Reg0W()
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "0\n");
        //0 register writes, write a 0 and advance 1 byte
        //*current_spot_in_buildBuff = 0;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Instruction is finished, copy the buffer
        //CopyBuffer();
    }
}

/**
Function used when there is 1 register written
@param reg1Num    The first register that is written
*/
VOID Reg1W (UINT32 reg1Num)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "1 %u\n", reg1Num);
        //1 register write, write a 1 and advance 1 byte
        //*current_spot_in_buildBuff = 1;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the register number and advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = reg1Num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Instruction is finished, copy the buffer
        //CopyBuffer();
    }
}

/**
Function used when 2 registers are written
@param reg1Num    The first register that is written
@param reg2Num    The second register that is written
*/
VOID Reg2W(UINT32 reg1Num, UINT32 reg2Num)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "2 %u %u \n", reg1Num, reg2Num);
        //2 register writes, write a 2 and advance 1 byte
        //*current_spot_in_buildBuff = 2;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the number of the first register, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = reg1Num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write the number of the second register, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = reg2Num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Instruction is finished, copy the buffer
        //CopyBuffer();
    }
}

/**
Function used when there are 3 registers written
@param reg1Num    The first register that is written
@param reg2Num    The second register that is written
@param reg3Num    The third register that is written
*/
VOID Reg3W(UINT32 reg1Num, UINT32 reg2Num, UINT32 reg3Num)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "3 %u %u %u \n", reg1Num, reg2Num, reg3Num);
        //3 register writes, write a 3 and advance 1 byte
        //*current_spot_in_buildBuff = 3;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the number of the first register, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = reg1Num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write the number of the second register, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = reg2Num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write the number of the third register, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = reg3Num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Instruction is finished, copy the buffer
        //CopyBuffer();
    }
}

/**
Function used when there are 4 registers written
@param reg1Num    The first register that is written
@param reg2Num    The second register that is written
@param reg3Num    The third register that is written
@param reg4Num    The fourth register that is written
*/
VOID Reg4W(UINT32 reg1Num, UINT32 reg2Num, UINT32 reg3Num, UINT32 reg4Num)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "4 %u %u %u %u \n", reg1Num, reg2Num, reg3Num, reg4Num);
        //4 register writes, write a 4 and advance 1 byte
        //*current_spot_in_buildBuff = 4;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(char);

        //Write the number of the first register, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = reg1Num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write the number of the second register, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = reg2Num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write the number of the third register, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = reg3Num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Write the number of the fourth register, advance 4 bytes
        //*((UINT32 *)current_spot_in_buildBuff) = reg4Num;
        //current_spot_in_buildBuff = current_spot_in_buildBuff + sizeof(UINT32);

        //Instruction is finished, copy the buffer
        //CopyBuffer();
    }
}

/**
The Instrumentation Function used by the program
@param ins    The instruction that is being inspected
@param *v    Value specified by the tool
*/
VOID Test(INS ins, VOID *v)
{
    int maxR;    //The variable to hold the number of registers that are read in the instruction
    int maxW;    //The variable to hold the number of registers that are written in the instruction
    int maxMem;    //The variable to hold the number of memory operands

    //Make the call to increment the instruction counter
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)IncrementCounter, IARG_END);

    //Determine the number of register that are read
    maxR = INS_MaxNumRRegs(ins);

    //Run a check to make sure all registers were accounted for
    if(maxR > 6)
    {
        cout << "ERROR: MORE THAN 6 REGISTERS WERE READ";
    }
    else if(maxR == 0)
    {
        //0 registers read
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Reg0Print, IARG_EXECUTING, IARG_INST_PTR, IARG_UINT32, INS_Opcode(ins), IARG_UINT32, INS_Size(ins), IARG_END);
    }
    else if(maxR == 1)
    {
        //1 register read
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Reg1Print, IARG_EXECUTING, IARG_INST_PTR, IARG_UINT32, INS_Opcode(ins), IARG_UINT32, INS_Size(ins), IARG_UINT32, INS_RegR(ins, 0), IARG_END);
    }
    else if(maxR == 2)
    {
        //2 registers read
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Reg2Print, IARG_EXECUTING, IARG_INST_PTR, IARG_UINT32, INS_Opcode(ins), IARG_UINT32, INS_Size(ins), IARG_UINT32, INS_RegR(ins, 0), IARG_UINT32, INS_RegR(ins, 1), IARG_END);
    }
    else if(maxR == 3)
    {
        //3 registers read
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Reg3Print, IARG_EXECUTING, IARG_INST_PTR, IARG_UINT32, INS_Opcode(ins), IARG_UINT32, INS_Size(ins), IARG_UINT32, INS_RegR(ins, 0), IARG_UINT32, INS_RegR(ins, 1), IARG_UINT32, INS_RegR(ins, 2), IARG_END);
    }
    else if(maxR == 4)
    {
        //4 registers read
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Reg4Print, IARG_EXECUTING, IARG_INST_PTR, IARG_UINT32, INS_Opcode(ins), IARG_UINT32, INS_Size(ins), IARG_UINT32, INS_RegR(ins, 0), IARG_UINT32, INS_RegR(ins, 1), IARG_UINT32, INS_RegR(ins, 2), IARG_UINT32, INS_RegR(ins, 3), IARG_END);
    }
    else if(maxR == 5)
    {
        //5 registers read
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Reg5Print, IARG_EXECUTING, IARG_INST_PTR, IARG_UINT32, INS_Opcode(ins), IARG_UINT32, INS_Size(ins), IARG_UINT32, INS_RegR(ins, 0), IARG_UINT32, INS_RegR(ins, 1), IARG_UINT32, INS_RegR(ins, 2), IARG_UINT32, INS_RegR(ins, 3), IARG_UINT32, INS_RegR(ins, 4), IARG_END);
    }
    else
    {
        //6 registers read
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Reg6Print, IARG_EXECUTING, IARG_INST_PTR, IARG_UINT32, INS_Opcode(ins), IARG_UINT32, INS_Size(ins), IARG_UINT32, INS_RegR(ins, 0), IARG_UINT32, INS_RegR(ins, 1), IARG_UINT32, INS_RegR(ins, 2), IARG_UINT32, INS_RegR(ins, 3), IARG_UINT32, INS_RegR(ins, 4), IARG_UINT32, INS_RegR(ins,5), IARG_END);
    }

    //Determine the number of memory operands
    maxMem = INS_MemoryOperandCount(ins);

    if(maxMem == 0)
    {
        //0 memory reads, 0 memory writes
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Mem0Print, IARG_END);
    }

    if(maxMem == 1)
    {
        //1 memory operand
        if(INS_MemoryOperandIsRead(ins, 0))
        {
            //1 memory read, 0 memory write
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Mem1RPrint, IARG_MEMORYREAD_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 0), IARG_END);
        }
        else
        {
            //0 memory read, 1 memory write
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Mem1WPrint, IARG_MEMORYWRITE_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 0), IARG_END);
        }
    }

    if(maxMem == 2)
    {
        //2 memory operands
        if(INS_MemoryOperandIsRead(ins, 0) && INS_MemoryOperandIsRead(ins, 1))
        {
            //Memory operands 1 and 2 read
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Mem2RRPrint, IARG_MEMORYREAD_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 0), IARG_MEMORYREAD2_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 1), IARG_END);
        }

        else if(INS_MemoryOperandIsRead(ins, 0) && INS_MemoryOperandIsWritten(ins, 1))
        {
            //Memory operand 1 read, 2 written
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Mem2RWPrint, IARG_MEMORYREAD_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 0), IARG_MEMORYWRITE_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 1), IARG_END);
        }

        else
        {
            //Memory operand 2 read, 1 written
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Mem2WRPrint, IARG_MEMORYWRITE_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 0), IARG_MEMORYREAD_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 1), IARG_END);
        }
    }

    if(maxMem == 3)
    {
        //3 memory operands
        if(INS_MemoryOperandIsRead(ins, 0) && INS_MemoryOperandIsRead(ins, 1) && INS_MemoryOperandIsWritten(ins, 2))
        {
            //Memory operands 1 & 2 read, 3 written
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Mem3RRWPrint, IARG_MEMORYREAD_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 0), IARG_MEMORYREAD2_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 1), IARG_MEMORYWRITE_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 2), IARG_END);
        }
        
        else if(INS_MemoryOperandIsRead(ins, 0) && INS_MemoryOperandIsWritten(ins, 1) && INS_MemoryOperandIsRead(ins, 2))
        {
            //Memory operands 1 & 3 read, 2 written
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Mem3RWRPrint, IARG_MEMORYREAD_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 0), IARG_MEMORYWRITE_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 1), IARG_MEMORYREAD2_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 2), IARG_END);
        }

        else
        {
            //Memory operands 2 & 3 read, 1 written
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Mem3WRRPrint, IARG_MEMORYWRITE_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 0), IARG_MEMORYREAD_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 1), IARG_MEMORYREAD2_EA, IARG_UINT32, INS_MemoryOperandSize(ins, 2), IARG_END);
        }
    }

    //Determine the number of registers written
    maxW = INS_MaxNumWRegs(ins);

    //Run a check to make sure all registers are accounted for
    if(maxW > 4)
    {
        cout << "ERROR: MORE THAN FOUR REGISTERS WERE WRITTEN";
    }
    else if(maxW == 0)
    {
        //0 register writes
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Reg0W, IARG_END);
    }
    else if(maxW == 1)
    {
        //1 register write
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Reg1W, IARG_UINT32, INS_RegR(ins, 0), IARG_END);
    }
    else if(maxW == 2)
    {
        //2 register writes
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Reg2W, IARG_UINT32, INS_RegR(ins, 0), IARG_UINT32, INS_RegR(ins, 1), IARG_END);
    }
    else if(maxW == 3)
    {
        //3 register writes
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Reg3W, IARG_UINT32, INS_RegR(ins, 0), IARG_UINT32, INS_RegR(ins, 1), IARG_UINT32, INS_RegR(ins, 2), IARG_END);
    }
    else if(maxW == 4)
    {
        //4 register writes
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Reg4W, IARG_UINT32, INS_RegR(ins, 0), IARG_UINT32, INS_RegR(ins, 1), IARG_UINT32, INS_RegR(ins, 2), IARG_UINT32, INS_RegR(ins, 3), IARG_END);
    }
}

/*!
 * Function used to compress the last of the traces and close output files.
 * This function is called when the application exits.
 * @param[in]   code            Exit code of the application
 * @param[in]   v               Value specified by the tool
 */
VOID Fini(INT32 code, VOID *v)
{
    //Compress the last of the traces and export them before closing the file
    //CompressAndExport(true);

    //Check to make sure everything closed OK
    //assert(ret == Z_STREAM_END);

    //Clean up the compression stream
    //(void)deflateEnd(&strm);

    //Close the output file
    fclose(outputFile);

    //Delete any memory that was allocated
    //delete buildBuff;
    //delete compressionBuff;
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments,
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    //Account for any user specs
    numInstr = KnobInstrCount;
    skipPt = KnobSkipPoint;
    compLevel = KnobCompressionLevel;
    
    // Register the function to be called to instrument traces
    INS_AddInstrumentFunction(Test, 0);

    // Register the function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    //================Output the header================

    //Create the output file
    outputFile = fopen((KnobOutputFile.Value()).c_str(), "w");

    char header1[] = "===============================================\n";
    fwrite(header1,1,strlen(header1),outputFile);

    if (!KnobOutputFile.Value().empty())
    {
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    }

    //Print out the date and time of the trace generation
    time_t rawtime;
    struct tm * timeinfo;

    time (&rawtime);
    timeinfo = localtime (&rawtime);

    char header2[100];
    strcpy(header2, "Trace generated on: ");
    strcat(header2, asctime(timeinfo));
    fwrite(header2,1,strlen(header2),outputFile);
    
    //Print out the PinTool version number
    char header3[] = "PinTool V1.0\n";
    fwrite(header3,1,strlen(header3),outputFile);

    //Print out the user-specified program name
    int i;
    for (i = 0; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--"))
        {
            break;
        }
    }

    ++i;
    char header4[100];
    strcpy(header4,"Program name: ");
    strcat(header4, argv[i]);
    strcat(header4, "\n");
    fwrite(header4,1,strlen(header4),outputFile);
    ++i;

    //Print out the user-specified input string
    char header5[500];
    strcpy(header5, "Input String: ");
    for (; i < argc; ++i)
    {
        strcat(header5, argv[i]);
        strcat(header5, " ");
    }
        
    strcat(header5, "\n");
    fwrite(header5,1,strlen(header5),outputFile);

    std::stringstream ss;

    //Print out the user-specified number of instructions to skip
    char header6[100];
    strcpy(header6, "Number of instructions to skip: ");
    ss << skipPt;
    strcat(header6, (ss.str()).c_str());
    strcat(header6, "\n");
    fwrite(header6,1,strlen(header6),outputFile);

    ss.str("");

    //Print out the user-specified number of instructions to execute
    char header7[100];
    strcpy(header7, "Number of instructions to generate trace: ");
    ss << numInstr;
    strcat(header7, (ss.str()).c_str());
    strcat(header7, "\n");
    fwrite(header7,1,strlen(header7),outputFile);

    ss.str("");

    //Print out the compression method and user-specified compression level
    char header8[100];
    strcpy(header8, "Compressed using zLib, level ");
    ss << KnobCompressionLevel.Value();
    strcat(header8, (ss.str()).c_str());
    strcat(header8, "\n");
    fwrite(header8,1,strlen(header8),outputFile);

    char header9[] = "===============================================\n";
    fwrite(header9,1,strlen(header9),outputFile);

    //fflush(outputFile);
    //SET_BINARY_MODE(outputFile);

    //Initialize the buffers used for building and compression
    //buildBuff = new char[MAXINSTRSIZE];
    //compressionBuff = new char[(CHUNK + MAXINSTRSIZE)];
    //current_spot_in_buildBuff = buildBuff;
    //current_spot_in_compressionBuff = compressionBuff;

    //Allocate the deflate state
    //strm.zalloc = Z_NULL;
    //strm.zfree = Z_NULL;
    //strm.opaque = Z_NULL;
    //ret = deflateInit(&strm, KnobCompressionLevel.Value());
    //    if (ret != Z_OK)
    //    return ret;

    //Everything is ready, so start the program - never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
