#include <stdio.h>
#include <stdint.h>


int pc = 0; // Program Counter

uint8_t SREG = 0b00000000; // 8-bit status register


int reg[64] = {0}; // Simulated Register File (8-bit addressing -->  64 registers)


int data_memory[2048] = {0}; // Simulated Data Memory


int instructionMemory[1024] = {0}; // Simulated Instruction Memory
int NumberOfInstructions = sizeof(instructionMemory) / sizeof(int);

void update_flags(int result, int val1, int val2, char operation) {
    // Clear bits 0–4 (Z, S, N, V, C) — keep bits 5–7 at 0
        SREG = SREG & 0b11100000;

    // ----- Zero Flag (Z, bit 0) -----
    if (result == 0)
        SREG = SREG | (1 << 0); // Set Z if result is zero

    // ----- Negative Flag (N, bit 2) -----
    if (result < 0)
        SREG = SREG | (1 << 2); // Set N if result is negative

    // ----- Carry Flag (C, bit 4) -----
    if (operation == '+') {
        unsigned int ures = (unsigned int)val1 + (unsigned int)val2;
        if ((ures >> 8) & 1) // check 9th bit (carry out)
            SREG = SREG | (1 << 4); // Set Carry
    } else if (operation == '-') {
        if ((unsigned int)val1 < (unsigned int)val2) // borrow occurred
            SREG = SREG | (1 << 4); // Set Carry
    }

    // ----- Overflow Flag (V, bit 3) -----
    if (operation == '+' || operation == '-') {
        int sign1 = (val1 >> 31) & 1;
        int sign2 = (val2 >> 31) & 1;
        int signr = (result >> 31) & 1;

        if (operation == '+' && sign1 == sign2 && signr != sign1)
            SREG = SREG | (1 << 3); // Overflow in addition
        else if (operation == '-' && sign1 != sign2 && signr == sign2)
            SREG = SREG | (1 << 3); // Overflow in subtraction
    }

    // ----- Sign Flag (S, bit 1) = N XOR V -----
    int N = (SREG >> 2) & 1;
    int V = (SREG >> 3) & 1;
    int S = N ^ V;

    SREG = SREG | (S << 1); // Set Sign Flag
}




// Execute the instruction based on opcode and fields
void instruction_execute(int opcode, int r1, int r2, int immediate) {
    switch (opcode) {
        case 0: // ADD
            reg[r1] = reg[r1] + reg[r2];
            update_flags(reg[r1], reg[r1]-reg[r2], reg[r2], '+');
            break;
        case 1: // SUB
            reg[r1] = reg[r1] - reg[r2];
            update_flags(reg[r1], reg[r1]+reg[r2], reg[r2], '-');
            break;
        case 2: // MUL
            reg[r1] = reg[r1] * reg[r2];
            // Manually update relevant flags (Z, N, S)
            SREG = SREG & 0b11100000; // Clear bits 0–4
            if (reg[r1] == 0)
                SREG = SREG | (1 << 0); // Zero flag

            if (reg[r1] < 0)
                SREG = SREG | (1 << 2); // Negative flag
            // Sign flag: S = N XOR V, but since V = 0, S = N
            int N = (SREG >> 2) & 1;
            SREG = SREG | (N << 1);  // Set S = N
            break;
        case 3: // MOV Immediate (MOVI)
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
            update_flags(reg[r1],0,0,' ');
            break;
        case 6: // EOR
            reg[r1] = reg[r1] ^ reg[r2];
            update_flags(reg[r1],0,0,' ');
            break;
        case 7: // JUMP (concatenate R1 and R2) (BR)
            pc = (reg[r1] << 6) | reg[r2];
            return; // Skip increment
        case 8: // SAL
            reg[r1] = reg[r1] << immediate;
            update_flags(reg[r1],0,0,' ');
            break;
        case 9: // SAR
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
