/*
CS-UY 2214
Adapted from Jeff Epstein
Starter code for E20 simulator
sim.cpp
*/

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <regex>
#include <cstdlib>
#include <cmath>

using namespace std;

// Some helpful constant values that we'll be using.
size_t const static NUM_REGS = 8;
size_t const static MEM_SIZE = 1<<13;
size_t const static REG_SIZE = 1<<16;

/*
    Loads an E20 machine code file into the list
    provided by mem. We assume that mem is
    large enough to hold the values in the machine
    code file.

    @param f Open file to read from
    @param mem Array represetnting memory into which to read program
*/
void load_machine_code(ifstream &f, unsigned short mem[]) {
    regex machine_code_re("^ram\\[(\\d+)\\] = 16'b(\\d+);.*$");
    size_t expectedaddr = 0;
    string line;
    while (getline(f, line)) {
        smatch sm;
        if (!regex_match(line, sm, machine_code_re)) {
            cerr << "Can't parse line: " << line << endl;
            exit(1);
        }
        size_t addr = stoi(sm[1], nullptr, 10);
        unsigned instr = stoi(sm[2], nullptr, 2);
        if (addr != expectedaddr) {
            cerr << "Memory addresses encountered out of sequence: " << addr << endl;
            exit(1);
        }
        if (addr >= MEM_SIZE) {
            cerr << "Program too big for memory" << endl;
            exit(1);
        }
        expectedaddr ++;
        mem[addr] = instr;
    }
}

/*
    Prints the current state of the simulator, including
    the current program counter, the current register values,
    and the first memquantity elements of memory.

    @param pc The final value of the program counter
    @param regs Final value of all registers
    @param memory Final value of memory
    @param memquantity How many words of memory to dump
*/
void print_state(unsigned pc, unsigned short regs[], unsigned short memory[], size_t memquantity) {
    cout << setfill(' ');
    cout << "Final state:" << endl;
    cout << "\tpc=" <<setw(5)<< pc << endl;

    for (size_t reg=0; reg<NUM_REGS; reg++)
        cout << "\t$" << reg << "="<<setw(5)<<regs[reg]<<endl;

    cout << setfill('0');
    bool cr = false;
    for (size_t count=0; count<memquantity; count++) {
        cout << hex << setw(4) << memory[count] << " ";
        cr = true;
        if (count % 8 == 7) {
            cout << endl;
            cr = false;
        }
    }
    if (cr)
        cout << endl;
}

// extract specific bit blocks, resource: geeksforgeeks
// position: position of the bit block to be extracted
// num_bits: amount of bits in the bit block to be extracted
unsigned int extract_bits(unsigned short curr_ins, unsigned short position, unsigned short num_bits) {
    // right shift by position value
    unsigned int shifted = curr_ins >> position;
    // left shift 1 by num_bits to get 100... then subtract one to get one less bit and all 1s for the mask
    unsigned int mask = (1 << num_bits) - 1;

    return shifted & mask;
}

// turns binary value to 16-bit signed integer value, taking care of negatives, resource: stackoverflow
// val: original unsigned value
// bit_length: length of val
signed short binary_to_int(unsigned short val, unsigned short bit_length) {
    // Check if MSB is 1
    signed short result;
    if (val & (1 << (bit_length - 1))) {
        // calculate 2s complement mathematically
        result = val - (1 << bit_length);
        return result;
    }
    else {
        // return original val unchanged
        return val;
    }
}


// Instruction class
class Instruction {
public:
    unsigned short opcode = 0;
    unsigned short operands[4];
    bool isHalt = false;

    Instruction() {
        for (int i = 0; i < 4; ++i) {
            operands[i] = 0;
        }
    }

    void parse_instruction(unsigned short curr_instruction, unsigned int curr_pc) {
        // Extract opcode first
        opcode = extract_bits(curr_instruction, 13, 3);

        // 3 register arguments - 4 operands (add, sub, or, and, slt, jr)
        if (opcode == 0b000) {
            // regSrcA
            operands[0] = extract_bits(curr_instruction, 10, 3);
            // regSrcB
            operands[1] = extract_bits(curr_instruction, 7, 3);
            // regDst
            operands[2] = extract_bits(curr_instruction, 4, 3);
            // 4-bit immediate value
            operands[3] = extract_bits(curr_instruction, 0, 4);

        }

        // 2 register arguments - 3 operands (slti, lw, sw, jeq, addi)
        if (opcode == 0b111 || opcode == 0b100 || opcode == 0b101 || opcode == 0b110 || opcode == 0b001
            || opcode == 0b010 || opcode == 0b011) {
            // regSrc
            operands[0] = extract_bits(curr_instruction, 10, 3);
            // regDst
            operands[1] = extract_bits(curr_instruction, 7, 3);
            // 7-bit immediate value
            operands[2] = extract_bits(curr_instruction, 0, 7);
            }

        // No register arguments - 1 operand (j, jal)
        if (opcode == 0b010 || opcode == 0b011) {
            // 13-bit immediate value
            operands[0] = extract_bits(curr_instruction, 0, 13);

            if (operands[0] == curr_pc) {
                isHalt = true;
            }
        }
    }

    void reinitialize() {
        opcode = 0;
        for (int i = 0; i < 4; ++i) {
            operands[i] = 0;
        }
        bool isHalt = false;
    }

};

// E20 class
class E20 {
public:
    unsigned short memory_arr[MEM_SIZE];  // 8192 mem cells that each hold 16-bits
    unsigned short registers[NUM_REGS];  // unsigned short range: [0, 65535]
    Instruction instruction;
    unsigned short pc;

    E20() {
        // initialize every mem cell to 0
        for (int i = 0; i < MEM_SIZE; ++i) {
            memory_arr[i] = 0;
        }
        // initialize every register to 0
        for (int i = 0; i < NUM_REGS; ++i) {
            registers[i] = 0;
        }
        pc = 0;
    };

    void execute_instruction(Instruction& curr_instruction) {
        // 3 register args
        if (curr_instruction.opcode == 0b000) {
            unsigned short regSrcA = curr_instruction.operands[0];
            unsigned short regSrcB = curr_instruction.operands[1];
            unsigned short regDst = curr_instruction.operands[2];
            unsigned short imm_val = curr_instruction.operands[3];

            // add
            if (imm_val == 0b0000) {
                // ensure that $0 always has the value of 0
                if (regDst != 0b000) {
                    registers[regDst] = registers[regSrcA] + registers[regSrcB];
                }
                else {
                    registers[regDst] = 0;
                }
                pc++;
            }
            // sub
            if (imm_val == 0b0001) {
                // ensure that $0 always has the value of 0
                if (regDst != 0b000) {
                    registers[regDst] = registers[regSrcA] - registers[regSrcB];
                }
                else {
                    registers[regDst] = 0;
                }
                pc++;
            }
            // or
            if (imm_val == 0b0010) {
                // ensure that $0 always has the value of 0
                if (regDst != 0b000) {
                    registers[regDst] = registers[regSrcA] | registers[regSrcB];
                }
                else {
                    registers[regDst] = 0;
                }
                pc++;
            }
            // and
            if (imm_val == 0b0011) {
                // ensure that $0 always has the value of 0
                if (regDst != 0b000) {
                    registers[regDst] = registers[regSrcA] & registers[regSrcB];
                }
                else {
                    registers[regDst] = 0;
                }
                pc++;
            }
            // slt
            if (imm_val == 0b0100) {
                // ensure that $0 always has the value of 0
                if (regDst != 0b000) {
                    if (registers[regSrcA] < registers[regSrcB]) {
                        registers[regDst] = 1;
                    }
                    else {
                        registers[regDst] = 0;
                    }
                }
                else {
                    registers[regDst] = 0;
                }
                pc++;
            }
            // jr
            if (imm_val == 0b1000) {
                pc = registers[regSrcA];
            }

        }

        // 2 register args (slti)
        if (curr_instruction.opcode == 0b111) {
            unsigned short regSrc = curr_instruction.operands[0];
            unsigned short regDst = curr_instruction.operands[1];
            unsigned short imm_val = curr_instruction.operands[2];

            signed short signed_imm = binary_to_int(imm_val, 7);

            // ensure that $0 always has the value of 0
            if (regDst != 0b000) {
                if (registers[regSrc] < signed_imm) {
                    registers[regDst] = 1;
                }
                else {
                    registers[regDst] = 0;
                }
            }
            else {
                registers[regDst] = 0;
            }
            pc++;
        }

        // 2 register args (lw)
        if (curr_instruction.opcode == 0b100) {
            unsigned short regAddr = curr_instruction.operands[0];
            unsigned short regDst = curr_instruction.operands[1];
            unsigned short imm_val = curr_instruction.operands[2];

            // ensure that $0 always has the value of 0
            if (regDst != 0b000) {
                signed short signed_imm = binary_to_int(imm_val, 7);
                unsigned short destination_addr = registers[regAddr] + signed_imm;
                registers[regDst] = memory_arr[destination_addr];
            }
            else {
                registers[regDst] = 0;
            }
            pc++;
        }

        // 2 register args (sw)
        if (curr_instruction.opcode == 0b101) {
            unsigned short regAddr = curr_instruction.operands[0];
            unsigned short regSrc = curr_instruction.operands[1];
            unsigned short imm_val = curr_instruction.operands[2];

            signed short signed_imm = binary_to_int(imm_val, 7);
            unsigned short source_addr = registers[regAddr] + signed_imm;
            memory_arr[source_addr] = registers[regSrc];
            pc++;
        }

        // 2 register args (jeq)
        if (curr_instruction.opcode == 0b110) {
            unsigned short regA = curr_instruction.operands[0];
            unsigned short regB = curr_instruction.operands[1];
            unsigned short rel_imm = curr_instruction.operands[2];

            signed short signed_rel_imm = binary_to_int(rel_imm, 7);
            if (registers[regA] == registers[regB]) {
                pc = pc + 1 + signed_rel_imm;
            }
            else {
                pc++;
            }
        }

        // 2 register args (addi)
        if (curr_instruction.opcode == 0b001) {
            unsigned short regSrc = curr_instruction.operands[0];
            unsigned short regDst = curr_instruction.operands[1];
            unsigned short imm_val = curr_instruction.operands[2];

            // ensure that $0 always has the value of 0
            if (regDst != 0b000) {
                signed short signed_imm = binary_to_int(imm_val, 7);
                registers[regDst] = registers[regSrc] + signed_imm;
            }
            else {
                registers[regDst] = 0;
            }
            pc++;
        }

        // no register args (j)
        if (curr_instruction.opcode == 0b010) {
            unsigned short imm_val = curr_instruction.operands[0];
            pc = imm_val;
        }

        // no register args (jal)
        if (curr_instruction.opcode == 0b011) {
            unsigned short imm_val = curr_instruction.operands[0];
            registers[7] = pc + 1;
            pc = imm_val;
        }

    }

    void run() {
        unsigned short curr_instruction;

        while (true) {
            curr_instruction = memory_arr[pc];
            // reset instruction for every new instruction
            instruction.reinitialize();
            instruction.parse_instruction(curr_instruction, pc);

            if (instruction.isHalt) {
                break;
            }
            else {
                execute_instruction(instruction);
            }
        }
    }

};

/**
    Main function
    Takes command-line args as documented below
*/
int main(int argc, char *argv[]) {
    /*
        Parse the command-line arguments
    */
    char *filename = nullptr;
    bool do_help = false;
    bool arg_error = false;
    for (int i=1; i<argc; i++) {
        string arg(argv[i]);
        if (arg.rfind("-",0)==0) {
            if (arg== "-h" || arg == "--help")
                do_help = true;
            else
                arg_error = true;
        } else {
            if (filename == nullptr)
                filename = argv[i];
            else
                arg_error = true;
        }
    }
    /* Display error message if appropriate */
    if (arg_error || do_help || filename == nullptr) {
        cerr << "usage " << argv[0] << " [-h] filename" << endl << endl;
        cerr << "Simulate E20 machine" << endl << endl;
        cerr << "positional arguments:" << endl;
        cerr << "  filename    The file containing machine code, typically with .bin suffix" << endl<<endl;
        cerr << "optional arguments:"<<endl;
        cerr << "  -h, --help  show this help message and exit"<<endl;
        return 1;
    }

    ifstream f(filename);
    if (!f.is_open()) {
        cerr << "Can't open file "<<filename<<endl;
        return 1;
    }
    // TODO: your code here. Load f and parse using load_machine_code
    E20 simulator;
    load_machine_code(f, simulator.memory_arr);

    // TODO: your code here. Do simulation.
    simulator.run();

    // TODO: your code here. print the final state of the simulator before ending, using print_state
    print_state(simulator.pc, simulator.registers, simulator.memory_arr, 128);
    return 0;
}
//ra0Eequ6ucie6Jei0koh6phishohm9
