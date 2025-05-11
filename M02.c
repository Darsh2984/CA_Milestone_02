#include <stdio.h>
#include <stdint.h>


int pc = 0; // Program Counter

uint8_t SREG = 0b00000000; // 8-bit status register


// Simulated Register File (8-bit addressing → 64 registers)
int reg[64] = {0};

// Simulated Data Memory
int data_memory[2048] = {0};

// Simulated Instruction Memory
int instructionMemory[1024] = {0};
int NumberOfInstructions = sizeof(instructionMemory) / sizeof(int);

// Execute the instruction based on opcode and fields
void instruction_execute(int opcode, int r1, int r2, int immediate) {
    switch (opcode) {
        case 0: // ADD
            reg[r1] = reg[r1] + reg[r2];
            break;
        case 1: // SUB
            reg[r1] = reg[r1] - reg[r2];
            break;
        case 2: // MUL
            reg[r1] = reg[r1] * reg[r2];
            break;
        case 3: // MOV Immediate
            reg[r1] = immediate;
            break;
        case 4: // BEQZ
            if (reg[r1] == 0) {
                pc = pc + 1 + immediate;
                return; // Skip increment after execution
            }
            break;
        case 5: // ANDI
            reg[r1] = reg[r1] & immediate;
            break;
        case 6: // XOR
            reg[r1] = reg[r1] ^ reg[r2];
            break;
        case 7: // JUMP (concatenate R1 and R2)
            pc = (reg[r1] << 6) | reg[r2];
            return; // Skip increment
        case 8: // LS
            reg[r1] = reg[r1] << immediate;
            break;
        case 9: // RS
            reg[r1] = reg[r1] >> immediate;
            break;
        case 10: // LDR
            reg[r1] = data_memory[immediate];
            break;
        case 11: // STR
            data_memory[immediate] = reg[r1];
            break;
        default:
            printf("Unknown opcode: %d\n", opcode);
    }
    pc++; // Advance PC unless modified (BEQZ, JUMP)
}

// Decode the 16-bit instruction
void instruction_decode(int instruction) {
    int opcode = (instruction >> 12) & 0b1111;      // bits 15–12
    int r1     = (instruction >> 6)  & 0b111111;    // bits 11–6
    int r2     = instruction & 0b111111;            // bits 5–0
    int immediate = r2;

    instruction_execute(opcode, r1, r2, immediate);
}

// Instruction fetch loop
void instruction_fetch() {
    while (pc < NumberOfInstructions) {
        int instruction = instructionMemory[pc];
        instruction_decode(instruction);
    }
}

int main() {
    // Optional: Initialize registers before execution
    reg[1] = 10;
    reg[2] = 0;

    // Run simulation
    instruction_fetch();

    // Output results
    for (int i = 0; i < 4; i++) {
        printf("reg[%d] = %d\n", i, reg[i]);
    }

    printf("Memory[10] = %d\n", data_memory[10]);

    return 0;
}
