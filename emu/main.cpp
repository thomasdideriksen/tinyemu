#include <iostream>
#include <vector>
#include <stack>
#include <bitset>
#include <intrin.h>

#include "common.h"
#include "machinestate.h"


/*
http://www.easy68k.com/
https://www.nxp.com/files-static/archives/doc/ref_manual/M68000PRM.pdf
http://www.easy68k.com/paulrsm/doc/dpbm68k1.htm
https://wiki.neogeodev.org/index.php?title=68k
https://wiki.neogeodev.org/index.php?title=68k_instructions_timings
http://68k.hax.com/NEGX <--- 
http://alanclements.org/68kusersupervisor.html

https://www.ticalc.org/pub/text/68k/68k2x86.txt
https://www.ticalc.org/pub/text/68k/68kguide.txt
http://goldencrystal.free.fr/M68kOpcodes-v2.3.pdf <---
http://info.sonicretro.org/SCHG:68000_ASM-to-Hex_Code_Reference
http://mrjester.hapisan.com/04_MC68/
https://darkdust.net/writings/megadrive/crosscompiler

http://ocw.utm.my/pluginfile.php/1340/mod_resource/content/0/04-68k-Addressing.Modes.ppt.pdf <-- Adressing examples
https://www.cs.mcgill.ca/~cs573/fall2002/notes/lec273/lecture10/

http://www.cse.dmu.ac.uk/~sexton/WWWPages/exceptions.html <--- TRAP, reset vectors, exception prpocessing

http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt <-- Overflow, carry

http://www.cse.dmu.ac.uk/~sexton/WWWPages/exceptions.html <-- Vector table

8, 32-bit data registers and 8, 32-bit address registers
7 interrupt levels
56 instructions
14 addressing modes

Instruction format: 1word- 11words (2 bytes - 22 bytes)
 
 --  The first word of the instruction, called the [simple/single(?) effective address operation word], specifies:
        1) the length of the instruction
        2) the effective addressing mode
        3) and the operation to be performed

-- The remaining words, called [brief extension word] and [full extension word], further specify the instruction and operands



Adressing modes:
    0: Register direct 
    1: Absolute data
    2: PC relative 
    3: Register indirect
    4: Immediate data
    5: Implied

--> Note: The "Effective Address" is a combination of REGISTER (3 bits) and MODE (3 bits)

--> Note: TRAP is a software interrupt

*/



int main(void)
{
    uint32_t a = 0;
    auto b = negate(a);


    try
    {
        machine_state machine;

        FILE* fp = nullptr;
        ::fopen_s(&fp, "C:\\Users\\dideriks\\Desktop\\EASy68K\\EASy68K\\test.bin", "rb");
        if (fp)
        {
            ::fseek(fp, 0, SEEK_END);
            size_t size = ::ftell(fp);
            ::fseek(fp, 0, SEEK_SET);
            void* buffer = ::malloc(size);
            ::fread(buffer, size, 1, fp);
            ::fclose(fp);
            machine.load_program(0x1000, buffer, size, 0x1000);
            ::free(buffer);

            while (true)
            {
                machine.tick();
            }
        }
    }
    catch (std::exception& ex)
    {
        std::cout << "Exception: " << ex.what() << std::endl;
    }

    return 0;
}