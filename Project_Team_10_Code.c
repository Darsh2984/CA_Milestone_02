#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint16_t pc = 0;

uint8_t SREG = 0b00000000;

int8_t reg[64] = {0};

int8_t data_memory[2048] = {0};

uint16_t instruction_memory[1024] = {0};

int clock_cycle = 0;

typedef struct {
    int active;
    uint16_t instruction;
    int pc;
    char name[20];
    int opcode;
    int r1;
    int r2;
} PipelineStage;

PipelineStage fetch_stage = {0};
PipelineStage decode_stage = {0};
PipelineStage execute_stage = {0};

int branch_taken = 0;
int tmp_PC_branch = 0;

int end_of_program = 0;

// Print binary representation of a byte
void print_binary(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (byte >> i) & 1);
    }
}

void print_sreg() {
    printf("SREG = ");
    print_binary(SREG);
    printf(" (");
    if (SREG & (1 << 0)) printf("Z");
    if (SREG & (1 << 1)) printf("S");
    if (SREG & (1 << 2)) printf("N");
    if (SREG & (1 << 3)) printf("V");
    if (SREG & (1 << 4)) printf("C");
    printf(")\n");
}

void update_flags(int result, int val1, int val2, char operation) {
    SREG = SREG & 0b11100000;  //clear

    // ----- Zero Flag (Z, bit 0) -----
    if ((result & 0xFF) == 0)
        SREG = SREG | (1 << 0);

    // ----- Negative Flag (N, bit 2) -----
    if (result & 0x80)
        SREG = SREG | (1 << 2);

    // ----- Carry Flag (C, bit 4) -----
    if (operation == '+') {
        if (((uint16_t)(val1 & 0xFF) + (uint16_t)(val2 & 0xFF)) > 0xFF)
            SREG = SREG | (1 << 4);
    }

    // ----- Overflow Flag (V, bit 3) -----
    int sign1 = (val1 & 0x80) != 0;
    int sign2 = (val2 & 0x80) != 0;
    int signr = (result & 0x80) != 0;

    if ((operation == '+' && sign1 == sign2 && sign1 != signr) || (operation == '-' && sign1 != sign2 && signr == sign2)) {
        SREG = SREG | (1 << 3);
    }

    // ----- Sign Flag (S, bit 1) = N XOR V -----
    if (operation == '+' || operation == '-' ){
        int N = (SREG >> 2) & 1;
        int V = (SREG >> 3) & 1;
        if (N ^ V)
            SREG = SREG | (1 << 1);
    }
    
}

int8_t sign_extend_immediate(uint8_t imm) {
    if (imm & 0x20) {
        return (int8_t)(imm | 0xC0);
    } else {
        return (int8_t)imm;
    }
}

void decode_instruction(PipelineStage *stage) {
    uint16_t instruction = stage->instruction;
    
    stage->opcode = (instruction >> 12) & 0b1111;   // bits 15–12
    stage->r1 = (instruction >> 6)  & 0b111111;     // bits 11–6
    stage->r2 = instruction & 0b111111;             // bits 5–0
    
    switch(stage->opcode) {
        case 0: strcpy(stage->name, "ADD"); break;
        case 1: strcpy(stage->name, "SUB"); break;
        case 2: strcpy(stage->name, "MUL"); break;
        case 3: strcpy(stage->name, "MOVI"); break;
        case 4: strcpy(stage->name, "BEQZ"); break;
        case 5: strcpy(stage->name, "ANDI"); break;
        case 6: strcpy(stage->name, "EOR"); break;
        case 7: strcpy(stage->name, "BR"); break;
        case 8: strcpy(stage->name, "SAL"); break;
        case 9: strcpy(stage->name, "SAR"); break;
        case 10: strcpy(stage->name, "LDR"); break;
        case 11: strcpy(stage->name, "STR"); break;
        default: strcpy(stage->name, "UNKNOWN"); break;
    }
    
    printf("  Decode: %s (opcode=%d, r1=%d, r2/imm=%d", 
           stage->name, stage->opcode, stage->r1, stage->r2);
           
    if (stage->opcode == 3 || stage->opcode == 5 || (stage->opcode >= 10 && stage->opcode <= 11)) {
        int8_t signed_imm = sign_extend_immediate(stage->r2 & 0x3F);
        if (signed_imm != stage->r2) {
            printf(" [signed=%d]", signed_imm);
        }
    }
    printf(")\n");
}

void execute_instruction(PipelineStage *stage) {
    int opcode = stage->opcode;
    int r1 = stage->r1;
    int r2 = stage->r2;
    int immediate = r2;
    
    printf("  Execute: %s (opcode=%d, r1=%d, r2/imm=%d)\n", 
           stage->name, opcode, r1, r2);
    
    int old_value = reg[r1];
    int8_t signed_immediate;
    
    // For shift, branch, and jump, keep immediate as unsigned
    if (opcode == 4 || opcode == 8 || opcode == 9) {
        signed_immediate = (int8_t)(immediate & 0x3F);
    } else {
        signed_immediate = sign_extend_immediate(immediate & 0x3F);
    }
    
    switch (opcode) {
        case 0: // ADD
            printf("    Inputs: R%d=%d, R%d=%d\n", r1, reg[r1], r2, reg[r2]);
            int8_t val1 = reg[r1];
            int8_t val2 = reg[r2];
            reg[r1] = (int8_t)((int8_t)reg[r1] + (int8_t)reg[r2]);
            update_flags(reg[r1], val1, val2, '+');
            break;
        case 1: // SUB
            printf("    Inputs: R%d=%d, R%d=%d\n", r1, reg[r1], r2, reg[r2]);
            int8_t val1_sub = reg[r1];
            int8_t val2_sub = reg[r2];
            reg[r1] = (int8_t)((int8_t)reg[r1] - (int8_t)reg[r2]);
            update_flags(reg[r1], val1_sub, val2_sub, '-');
            break;
        case 2: // MUL
            printf("    Inputs: R%d=%d, R%d=%d\n", r1, reg[r1], r2, reg[r2]);
            reg[r1] = (int8_t)((int8_t)reg[r1] * (int8_t)reg[r2]);
            SREG = SREG & 0b11100000;
            if (reg[r1] == 0)
                SREG = SREG | (1 << 0);
            if (reg[r1] & 0x80)
                SREG = SREG | (1 << 2);
            
            break;
        case 3: // MOVI
            printf("    Inputs: R%d=%d, imm=%d\n", r1, reg[r1], signed_immediate);
            reg[r1] = signed_immediate;
            break;
        case 4: // BEQZ
            printf("    Inputs: R%d=%d, imm=%d\n", r1, reg[r1], immediate);
            if (reg[r1] == 0) {
                int new_pc = stage->pc + 1 + immediate;
                printf("    Branch taken: PC updated from %d to %d\n", pc, new_pc);
                tmp_PC_branch = new_pc;
                branch_taken = 1;  // Set flag to flush pipeline
            }
            break;
        case 5: // ANDI
            printf("    Inputs: R%d=%d, imm=%d\n", r1, reg[r1], signed_immediate);
            reg[r1] = (int8_t)((int8_t)reg[r1] & signed_immediate);
            update_flags(reg[r1], 0, 0, ' ');
            break;
        case 6: // EOR
            printf("    Inputs: R%d=%d, R%d=%d\n", r1, reg[r1], r2, reg[r2]);
            reg[r1] = (int8_t)((int8_t)reg[r1] ^ (int8_t)reg[r2]);
            update_flags(reg[r1], 0, 0, ' ');
            break;
        case 7: // BR
            printf("    Inputs: R%d=%d, R%d=%d\n", r1, reg[r1], r2, reg[r2]);
            int new_pc = ((uint16_t)reg[r1] << 6) | reg[r2];
            printf("    Jump taken: PC updated from %d to %d\n", pc, new_pc);
            tmp_PC_branch = new_pc;
            branch_taken = 1;  // Set flag to flush pipeline
            break;
        case 8: // SAL
            printf("    Inputs: R%d=%d, imm=%d\n", r1, reg[r1], immediate);
            reg[r1] = (int8_t)((int8_t)reg[r1] << immediate);
            update_flags(reg[r1], 0, 0, ' ');
            break;
        case 9: // SAR
            printf("    Inputs: R%d=%d, imm=%d\n", r1, reg[r1], immediate);
            if (reg[r1] & 0x80) {
                reg[r1] = (int8_t)((reg[r1] >> immediate) | (~(0xFF >> immediate)));
            } else {
                reg[r1] = (int8_t)(reg[r1] >> immediate);
            }
            update_flags(reg[r1], 0, 0, ' ');
            break;
        case 10: // LDR
            printf("    Inputs: R%d=%d, addr=%d\n", r1, reg[r1], signed_immediate);
            uint16_t addr = (signed_immediate < 0) ? 
                          (2048 + signed_immediate) % 2048 : 
                          signed_immediate % 2048;
            reg[r1] = data_memory[addr];
            break;
        case 11: // STR
            printf("    Inputs: R%d=%d, addr=%d\n", r1, reg[r1], signed_immediate);
            uint16_t store_addr = (signed_immediate < 0) ? 
                               (2048 + signed_immediate) % 2048 : 
                               signed_immediate % 2048;
            data_memory[store_addr] = reg[r1];
            printf("    Memory updated: data_memory[%d] = %d\n", store_addr, reg[r1]);
            break;
        default:
            printf("    Unknown opcode: %d\n", opcode);
    }
    
    if (old_value != reg[r1]) {
        printf("    Register updated: R%d changed from %d to %d\n", r1, old_value, reg[r1]);
    }
    
    if (opcode == 0 || opcode == 1 || opcode == 2 || opcode == 5 || opcode == 6 || opcode == 8 || opcode == 9) {
        printf("    ");
        print_sreg();
    }
}

void fetch_instruction() {
    if (pc >= 1024 || instruction_memory[pc] == 0) {
        fetch_stage.active = 0;
        printf("  Fetch: No instruction\n");

        end_of_program = 1;
        return;
    }
    
    fetch_stage.instruction = instruction_memory[pc];
    fetch_stage.pc = pc;
    fetch_stage.active = 1;
    
    printf("  Fetch: Instruction at PC=%d is 0x%04X\n", pc, fetch_stage.instruction);
    
    pc++;
}

void execute_pipeline_cycle() {
    printf("Clock Cycle %d:\n", ++clock_cycle);
    

    if(branch_taken){
        decode_stage.active = 0;
        execute_stage.active = 0;
        branch_taken = 0;
        pc = tmp_PC_branch;
    }
    
    if (execute_stage.active) {
        execute_instruction(&execute_stage);
        execute_stage.active = 0;
        
    } else {
        printf("  Execute: No instruction\n");
    }
    
    if (decode_stage.active) {
        decode_instruction(&decode_stage);

        execute_stage.active = decode_stage.active;
        execute_stage.instruction = decode_stage.instruction;
        execute_stage.pc = decode_stage.pc;
        strcpy(execute_stage.name, decode_stage.name);
        execute_stage.opcode = decode_stage.opcode;
        execute_stage.r1 = decode_stage.r1;
        execute_stage.r2 = decode_stage.r2;
        
        decode_stage.active = 0;
    }else {
        printf("  Decode: No instruction\n");
    }
    
    if (fetch_stage.active) {
        fetch_instruction();
        decode_stage.active = fetch_stage.active;
        decode_stage.instruction = fetch_stage.instruction;
        decode_stage.pc = fetch_stage.pc;
    }
    else{
        printf("  Fetch: No instruction\n");
    }
    
    if (!end_of_program && !fetch_stage.active && !decode_stage.active && !execute_stage.active) {
        end_of_program = 1;
        
    } 
    printf("\n");
}

uint16_t parse_instruction(char* line) {
    char instr[10];
    int r1, r2, imm;
    uint16_t binary = 0;
    
    char* newline = strchr(line, '\n');
    if (newline) *newline = '\0';
    
    if (strlen(line) == 0 || line[0] == '#' || line[0] == '/') {
        return 0xFFFF;
    }
    
    if (sscanf(line, "%s", instr) != 1) {
        return 0xFFFF;
    }
    
    for (int i = 0; instr[i]; i++) {
        if (instr[i] >= 'a' && instr[i] <= 'z') {
            instr[i] = instr[i] - 'a' + 'A';
        }
    }
    
    if (strcmp(instr, "ADD") == 0) {
        if (sscanf(line, "%*s R%d R%d", &r1, &r2) == 2) {
            binary = (0 << 12) | ((r1 & 0x3F) << 6) | (r2 & 0x3F);
        }
    } else if (strcmp(instr, "SUB") == 0) {
        if (sscanf(line, "%*s R%d R%d", &r1, &r2) == 2) {
            binary = (1 << 12) | ((r1 & 0x3F) << 6) | (r2 & 0x3F);
        }
    } else if (strcmp(instr, "MUL") == 0) {
        if (sscanf(line, "%*s R%d R%d", &r1, &r2) == 2) {
            binary = (2 << 12) | ((r1 & 0x3F) << 6) | (r2 & 0x3F);
        }
    } else if (strcmp(instr, "MOVI") == 0) {
        if (sscanf(line, "%*s R%d %d", &r1, &imm) == 2) {
            uint8_t imm_bits;
            if (imm < 0) {
                imm_bits = (uint8_t)((imm & 0x3F) | 0x20);
            } else {
                imm_bits = imm & 0x3F;
            }
            binary = (3 << 12) | ((r1 & 0x3F) << 6) | imm_bits;
        }
    } else if (strcmp(instr, "BEQZ") == 0) {
        if (sscanf(line, "%*s R%d %d", &r1, &imm) == 2) {
            binary = (4 << 12) | ((r1 & 0x3F) << 6) | (imm & 0x3F);
        }
    } else if (strcmp(instr, "ANDI") == 0) {
        if (sscanf(line, "%*s R%d %d", &r1, &imm) == 2) {
            uint8_t imm_bits;
            if (imm < 0) {
                imm_bits = (uint8_t)((imm & 0x3F) | 0x20);
            } else {
                imm_bits = imm & 0x3F;
            }
            binary = (5 << 12) | ((r1 & 0x3F) << 6) | imm_bits;
        }
    } else if (strcmp(instr, "EOR") == 0) {
        if (sscanf(line, "%*s R%d R%d", &r1, &r2) == 2) {
            binary = (6 << 12) | ((r1 & 0x3F) << 6) | (r2 & 0x3F);
        }
    } else if (strcmp(instr, "BR") == 0) {
        if (sscanf(line, "%*s R%d R%d", &r1, &r2) == 2) {
            binary = (7 << 12) | ((r1 & 0x3F) << 6) | (r2 & 0x3F);
        }
    } else if (strcmp(instr, "SAL") == 0) {
        if (sscanf(line, "%*s R%d %d", &r1, &imm) == 2) {
            binary = (8 << 12) | ((r1 & 0x3F) << 6) | (imm & 0x3F);
        }
    } else if (strcmp(instr, "SAR") == 0) {
        if (sscanf(line, "%*s R%d %d", &r1, &imm) == 2) {
            binary = (9 << 12) | ((r1 & 0x3F) << 6) | (imm & 0x3F);
        }
    } else if (strcmp(instr, "LDR") == 0) {
        if (sscanf(line, "%*s R%d %d", &r1, &imm) == 2) {
            uint8_t imm_bits;
            if (imm < 0) {
                imm_bits = (uint8_t)((imm & 0x3F) | 0x20);
            } else {
                imm_bits = imm & 0x3F;
            }
            binary = (10 << 12) | ((r1 & 0x3F) << 6) | imm_bits;
        }
    } else if (strcmp(instr, "STR") == 0) {
        if (sscanf(line, "%*s R%d %d", &r1, &imm) == 2) {
            uint8_t imm_bits;
            if (imm < 0) {
                imm_bits = (uint8_t)((imm & 0x3F) | 0x20);
            } else {
                imm_bits = imm & 0x3F;
            }
            binary = (11 << 12) | ((r1 & 0x3F) << 6) | imm_bits;
        }
    } else {
        return 0xFFFF;
    }
    
    return binary;
}

int load_program(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open file %s\n", filename);
        return 0;
    }
    
    char line[256];
    int addr = 0;
    
    while (fgets(line, sizeof(line), file) && addr < 1024) {
        uint16_t binary = parse_instruction(line);
        
        if (binary != 0xFFFF) {
            instruction_memory[addr] = binary;
            printf("Loaded instruction at %d: 0x%04X\n", addr, binary);
            addr++;
        }
    }
    
    fclose(file);
    return addr;
}

void print_system_state() {
    printf("\n--- System State ---\n");
    
    printf("PC = %d (0x%04X)\n", pc, pc);
    print_sreg();
    
    printf("\nRegisters:\n");
    for (int i = 0; i < 64; i += 8) {
        printf("R%d-R%d:", i, i+7);
        for (int j = 0; j < 8; j++) {
            printf(" %3d", reg[i+j]);
        }
        printf("\n");
    }
    
    printf("\nData Memory (non-zero locations):\n");
    int empty = 1;
    for (int i = 0; i < 2048; i++) {
        if (data_memory[i] != 0) {
            printf("  [%d] = %d\n", i, data_memory[i]);
            empty = 0;
        }
    }
    if (empty) {
        printf("  All memory locations are zero\n");
    }
    
    printf("\nInstruction Memory:\n");
    for (int i = 0; i < 1024; i++) {
        if (instruction_memory[i] != 0) {
            printf("  [%d] = 0x%04X\n", i, instruction_memory[i]);
        }
    }
    
    printf("\n");
}

void run_program_pipelined() {
    printf("Starting program execution (pipelined mode)...\n\n");
    
    fetch_stage.active = 1;
    decode_stage.active = 0;
    execute_stage.active = 0;
    branch_taken = 0;
    end_of_program = 0;
    clock_cycle = 0;
    
    do {
        execute_pipeline_cycle();
    } while (fetch_stage.active || decode_stage.active || execute_stage.active || !end_of_program);
    
    printf("Program execution completed after %d clock cycles.\n", clock_cycle);
    print_system_state();
}


int main() {
    pc = 0;
    SREG = 0;
    memset(reg, 0, sizeof(reg));
    memset(data_memory, 0, sizeof(data_memory));
    memset(instruction_memory, 0, sizeof(instruction_memory));
    
    reg[1] = 5;
    reg[2] = 20;
    reg[3] = 100;
    

    printf("Harvard Architecture CPU Simulator - Package 3 (Pipelined)\n");
    printf("----------------------------------------------\n\n");
    
    int count = load_program("Test_Instructions.txt");
    if (count > 0) {
        printf("Loaded %d instructions from %s\n\n", count, "Test_Instructions.txt");
    } else {
        printf("Failed to load program or no valid instructions found.\n");
        return 1;
    }
    
    run_program_pipelined();
    
    return 0;
}