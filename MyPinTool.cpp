#include <iostream>
#include <fstream>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <sstream>
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
FILE * outputFile;                     //The external file which contains the header and compressed trace information
FILE * instructionFile;                //File of disassembled instructions
FILE * regFile;                        //File to dump the registers.
UINT64 numInstr = 0;                   //The number of instructions to include in the output file
UINT64 skipPt = 0;                     //The number of initial instructions to exclude from the output file
UINT64 instrCounter = 0;               //The counter used for the total number of instructions traced so far
bool opcodePrint;                      //Rather or not to print the opcodes as strings.
bool disassemblePrint;                 //Rather or not to print the disassembled instructions.
bool regPrint;                         //Rather or not to print the regs as numbers or as strings.
int ret;                               //The return value modified by the compression method
 
string regRString[200];
string regWString[200];

/* ===================================================================== */
// MARK: - Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,  "pintool",          //Allows for specific naming of the output file
    "o", "Trace.out", "specify file name for MyPinTool output");
KNOB<UINT64> KnobSkipPoint(KNOB_MODE_WRITEONCE, "pintool",            //The knob for telling the program how many instructions to skip
    "skip", "0", "Specify the number of instructions to skip generating a trace for");
KNOB<UINT64> KnobInstrCount(KNOB_MODE_WRITEONCE, "pintool",           //The knob for telling the program how many instructions to generate traces for
    "num", "100000", "Specify the number of instructions to generate a trace for");
KNOB<int> KnobCompressionLevel(KNOB_MODE_WRITEONCE, "pintool",       //The knob for setting the level of compression - defaults to max
    "cmp", "9", "Specify the level of compression (0-9) that is desired");
KNOB<bool> KnobOpcodePrint(KNOB_MODE_WRITEONCE, "pintool", "opcode", "true",
    "If true prints opcode as a string instead of hex");
KNOB<bool> KnobRegPrint(KNOB_MODE_WRITEONCE, "pintool", "reg", "false", "If true print regs as a string instead of a int");
KNOB<bool> KnobDisassemble(KNOB_MODE_WRITEONCE, "pintool", "disassemble", "false", "If true, prints disassembled instructions instructions.out");

/* ===================================================================== */
// MARK: - Utilities
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
// MARK: - Instrumentation callbacks
/* ===================================================================== */

/**
Called every instruction trace - used to keep track of how many have been skipped and generated
*/
VOID IncrementCounter()
{
    instrCounter++;
}

/* ===================================================================== */
// MARK: Info, Opcode, and Repeat
/* ===================================================================== */

/**
Function used to print the execution bit, Instruction Address, Opcode Size, and Opcode.
@param ex            The boolean that signifies whether or not the instruction was actually executed
@param *ip            The pointer the instruction's physical address space
@param index        The opcode number
@param instrSize    The opcode size (in bytes)
*/
VOID InfoPrint(BOOL ex, VOID *ip, UINT32 index, UINT32 instrSize)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        if(opcodePrint)
            fprintf(outputFile, "%u %p %u %s ", ex, ip, instrSize, OPCODE_StringShort(index).c_str());
        else
            fprintf(outputFile, "%u %p %u %u ", ex, ip, instrSize, index);
        
    }
}

/**
 Function to print rather an instruction was a repeat.
 If it is a repeat then print # of repeats.
 @param rep                 The boolean that signifies if the instrcution repeats.
 @param repeats         Value in the repeat register.
 */
VOID RepPrint(BOOL rep, ADDRINT repeats){
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        if(rep)
            fprintf(outputFile, "%u %lu ", rep, repeats);
        else
            fprintf(outputFile, "%u ", rep);
    }
}

/**
 Function that prints a single number.
 @param val         Value to be printed.
 @param newLine Check to print a new line char.
*/
VOID ValPrint(UINT32 val, BOOL newLine){
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        if(newLine && val == 0){
            fprintf(outputFile, "%u \n", val);
        } else {
            //fprintf(outputFile, "");
            fprintf(outputFile, "%u ", val);
        }
    }
}


/* ===================================================================== */
// MARK: Registers
/* ===================================================================== */

/**
Function used to print a register that is read by the instruction.
@param reg1num        The number of the register that was read by the instruction
*/
VOID RegPrint(UINT32 reg1num, UINT32 size, BOOL isLast)
{
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        if(regPrint){
            fprintf(outputFile, "%s %u ", regRString[reg1num].c_str(), size);
        } else {
            fprintf(outputFile, "%u %u ", reg1num, size);
        }
        
        if(isLast){
            fprintf(outputFile, "\n");
        }
    }
}

/* ===================================================================== */
// MARK: Memory
/* ===================================================================== */

/**
 Function to print a memory operand.
 @param address     The address of the access.
 @param size            The size of the resulting data.
 */
VOID MemPrint(ADDRINT address, UINT32 size){
    if (instrCounter > skipPt && instrCounter <= (skipPt + numInstr))
    {
        fprintf(outputFile, "%lu %u ", address, size);
    }
}

/* ===================================================================== */
// MARK: - Instrumentation Function
/* ===================================================================== */

/**
The Instrumentation Function used by the program
@param ins    The instruction that is being inspected
@param *v    Value specified by the tool
*/
VOID Test(INS ins, VOID *v)
{
    int maxR;      //The variable to hold the number of registers that are read in the instruction
    int maxW;      //The variable to hold the number of registers that are written in the instruction
    int maxMem;    //The variable to hold the number of memory operands

    //Dump the disassembled instructions if needed.
    if(disassemblePrint){
        fprintf(instructionFile, "0x%lx ", INS_Address(ins));
        fprintf(instructionFile, "%s\n", INS_Disassemble(ins).c_str());
    }

    //Make the call to increment the instruction counter
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)IncrementCounter, IARG_END);

    //Make the call to print opcode and info.
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InfoPrint, IARG_EXECUTING, IARG_INST_PTR, IARG_UINT32, INS_Opcode(ins), IARG_UINT32, INS_Size(ins), IARG_END);
    
    
    //Make the call to the repeat function.
    if(INS_HasRealRep(ins)){
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RepPrint, IARG_BOOL, TRUE, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
    } else {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RepPrint, IARG_BOOL, FALSE, IARG_ADDRINT, 0, IARG_END);
        
    }
    
    
    //Determine the number of register that are read
    maxR = INS_MaxNumRRegs(ins);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ValPrint, IARG_UINT32, maxR, IARG_BOOL, FALSE, IARG_END);
    
    //Run a check to make sure all registers were accounted for
    if(maxR > 6) {
        cout << "ERROR: MORE THAN 6 REGISTERS WERE READ";
    } else {
        for(int i = 0; i < maxR; i++){
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RegPrint, IARG_UINT32, INS_RegR(ins, i), IARG_UINT32, REG_Size(INS_RegR(ins, i)), IARG_BOOL, FALSE, IARG_END);
        }
    }

    //Determine the number of memory operands
    maxMem = INS_MemoryOperandCount(ins);

    //Variables to store the number of each type of memory.
    int readCount = 0;
    int writeCount = 0;
    int reads[2];
    int writes = 0;
    
    //Categorize Mem Operations
    for(int i = 0; i < maxMem; i++){
        if(INS_MemoryOperandIsRead(ins, i)){
            reads[readCount] = i;
            readCount++;
        } else {
            writes = i;
            writeCount++;
        }
    }
    
    //Print Num Reads
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ValPrint, IARG_UINT32, readCount, IARG_BOOL, FALSE, IARG_END);
    
    //Print Reads
    for(int i = 0; i < readCount; i++){
        if(i == 0){
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)MemPrint, IARG_MEMORYREAD_EA, IARG_UINT32, INS_MemoryOperandSize(ins, reads[0]), IARG_END);
        } else if(i == 1){
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)MemPrint, IARG_MEMORYREAD2_EA, IARG_UINT32, INS_MemoryOperandSize(ins, reads[1]), IARG_END);
        }
    }
    //Print Number of Writes
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ValPrint, IARG_UINT32, writeCount, IARG_BOOL, FALSE, IARG_END);
    //Print Writes
    if(writeCount == 1){
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)MemPrint, IARG_MEMORYWRITE_EA, IARG_UINT32, INS_MemoryOperandSize(ins, writes), IARG_END);
    }
    
    //Determine the number of registers written
    maxW = INS_MaxNumWRegs(ins);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ValPrint, IARG_UINT32, maxW, IARG_BOOL, TRUE, IARG_END);
    
    //Run a check to make sure all registers are accounted for
    if(maxW > 4)
    {
        cout << "ERROR: MORE THAN FOUR REGISTERS WERE WRITTEN";
    } else {
        for(int i = 0; i < maxW; i++){
            if(i == (maxW - 1)){
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RegPrint, IARG_UINT32, INS_RegW(ins, i), IARG_UINT32, REG_Size(INS_RegW(ins, i)), IARG_BOOL, TRUE, IARG_END);
            } else {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RegPrint, IARG_UINT32, INS_RegW(ins, i), IARG_UINT32, REG_Size(INS_RegW(ins, i)), IARG_BOOL, FALSE, IARG_END);
            }
        }
    }
    

    if(regPrint){
        //Iterate over the read registers.
        for(int i = 0; i < maxR; i++){
            regRString[(UINT32)INS_RegR(ins, i)] = REG_StringShort(INS_RegR(ins, i));
            fprintf(regFile, "%i, %s\n", (UINT32)INS_RegR(ins, i), regRString[(UINT32)INS_RegR(ins, i)].c_str());
        }
        for(int i = 0; i < maxW; i++){
            regWString[(UINT32)INS_RegW(ins, i)] = REG_StringShort(INS_RegW(ins, i));
            fprintf(regFile, "%i, %s\n", (UINT32)INS_RegW(ins, i), regWString[(UINT32)INS_RegW(ins, i)].c_str());
        }
        
    }
        
}

/* ===================================================================== */
// MARK: - Setup/Exit
/* ===================================================================== */

/*!
* Function used to compress the last of the traces and close output files.
* This function is called when the application exits.
* @param[in]   code            Exit code of the application
* @param[in]   v               Value specified by the tool
*/
VOID Fini(INT32 code, VOID *v)
{
    //Print out number of instructions
    fprintf(outputFile, "**End of Trace**");
    fprintf(outputFile, "%lu", instrCounter);
    
    //Close the output file
    fclose(outputFile);
    if(disassemblePrint){
        fclose(instructionFile);
    }
    if(regPrint){
        fclose(regFile);
    }
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
    opcodePrint = KnobOpcodePrint;
    disassemblePrint = KnobDisassemble;
    regPrint = KnobRegPrint;

    // Register the function to be called to instrument traces
    INS_AddInstrumentFunction(Test, 0);

    // Register the function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    //========Ouput Disassembled Instructions==========
    if (disassemblePrint){
        instructionFile = fopen("instructions.out", "w");
        fprintf(instructionFile, "===============================================\n");
        //Print out the date and time of the trace generation
            time_t rawtime;
            struct tm * timeinfo;

            time (&rawtime);
           timeinfo = localtime (&rawtime);

           char header2[100];
           strcpy(header2, "Instructions generated on: ");
            strcat(header2, asctime(timeinfo));
            fwrite(header2, 1, strlen(header2), instructionFile);

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
        fwrite(header4,1,strlen(header4),instructionFile);
        ++i;
        fprintf(instructionFile, "===============================================\n");
    }

    //==============Ouput Register Strings====================
    if (regPrint){
        regFile = fopen("registers.out", "w");
        fprintf(regFile, "===============================================\n");
        //Print out the date and time of the trace generation
        time_t rawtime;
        struct tm * timeinfo;

        time (&rawtime);
           timeinfo = localtime (&rawtime);

           char header2[100];
           strcpy(header2, "Instructions generated on: ");
        strcat(header2, asctime(timeinfo));
        fwrite(header2, 1, strlen(header2), regFile);

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
        fwrite(header4,1,strlen(header4),regFile);
        ++i;
        fprintf(regFile, "===============================================\n");
    }

    //================Output the Output header================

    //Create the output file
    outputFile = fopen("Trace.out", "w");

    char header1[] = "===============================================\n";
    fwrite(header1,1,strlen(header1),outputFile);

    if (!KnobOutputFile.Value().empty())
    {
        cerr << "See file trace.out for analysis results" << endl;
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

    char header9[] = "===============================================\n";
    fwrite(header9,1,strlen(header9),outputFile);

    
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
