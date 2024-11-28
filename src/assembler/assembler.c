#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "vec.h"
#include "index.h"
#include "translator.h"
#include "../frontend/frontend.h"
#include "../backend/backend.h"

int read_greedy(FILE** in_fp, char* buffer, size_t n) {
	FILE* fp = *in_fp;
	int i = 0;
	char c;

	while((c = fgetc(fp)) != '.' && c != '\n' && c != ',' && c != EOF) {
		if (c == ' ' || c == '\t') continue;
		if (i==n-1) return 1;
		buffer[i] = c;
		i++;
	}

	buffer[i] = '\0';
	fseek(fp, -1, SEEK_CUR);
	return 0;
}

long int parse_imm(char* arg, char** endptr) {
	long int ret;

	if (arg[0] == '0') {
		if (arg[1] == 'x') {
			ret = strtol(arg+2, endptr, 16);
		} else if (arg[1] == 'b') {
			ret = strtol(arg+2, endptr, 2);
		} else {
			ret = strtol(arg+1, endptr, 8);
		}
	} else {
		ret = strtol(arg, endptr, 10);
	}

	return ret;
}

int pre_pass(FILE **in_fp, uint8_t* memory) {
	FILE* fp = *in_fp;
	char c;
	char buffer[80];
	char *endptr;
	int i=0;
	int line_offset = 1;
	int mem_pointer = DATA_BASE;
	long imm = 0;
	bool data_flag = false;
	bool comment_flag = false; // Are we reading a comment
	bool command_flag = false;

	while(1) {
		c = fgetc(fp);
		switch (c) {
			case '#':
			case ';':
				comment_flag = true;
				if (command_flag) {
					show_error("Unexpected comment on line %d", line_offset);
					return -1;
				}
				break;

			case EOF:
				show_error("Unexpected End of file!");
				return -1;

			case '\n':
				line_offset++;
				comment_flag = false;
				
			case ' ':
			case '\t':
				if (command_flag) {
					buffer[i] = '\0';
					if (!strcmp(buffer, "data")) {
						data_flag = true;

					} else if (!strcmp(buffer, "text")) {
						fseek(fp, -1, SEEK_CUR);
						return line_offset-1;

					} else if (!strcmp(buffer, "dword")) {
						if (!data_flag) {
							show_error("Invalid command outside data segment on line %d", line_offset);
						}

						do {
							if (read_greedy(&fp, buffer, 80)) {
								show_error("Value too large on line %d", line_offset);
								return -1;
							}

							imm = parse_imm(buffer, &endptr);
							if (*endptr != '\0') {
								show_error("Failed to parse value on line %d", line_offset);
								return -1;
							}

							memcpy(&memory[mem_pointer], &imm, 8);
							mem_pointer += 8;
						} while((c = fgetc(fp)) == ',');
		
						fseek(fp, -1, SEEK_CUR);

					} else if (!strcmp(buffer, "word")) {
						if (!data_flag) {
							show_error("Invalid command outside data segment on line %d", line_offset);
						}

						do {
							if (read_greedy(&fp, buffer, 80)) {
								show_error("Value too large on line %d", line_offset);
								return -1;
							}

							imm = parse_imm(buffer, &endptr);
							if (*endptr != '\0') {
								show_error("Failed to parse value on line %d", line_offset);
								return -1;
							}

							memcpy(&memory[mem_pointer], &imm, 4);
							mem_pointer += 4;
						} while((c = fgetc(fp)) == ',');
		
						fseek(fp, -1, SEEK_CUR);

					} else if (!strcmp(buffer, "half")) {
						if (!data_flag) {
							show_error("Invalid command outside data segment on line %d", line_offset);
						}

						do {
							if (read_greedy(&fp, buffer, 80)) {
								show_error("Value too large on line %d", line_offset);
								return -1;
							}

							imm = parse_imm(buffer, &endptr);
							if (*endptr != '\0') {
								show_error("Failed to parse value on line %d", line_offset);
								return -1;
							}

							memcpy(&memory[mem_pointer], &imm, 2);
							mem_pointer += 2;
						} while((c = fgetc(fp)) == ',');
		
						fseek(fp, -1, SEEK_CUR);
					
					} else if (!strcmp(buffer, "byte")) {
						if (!data_flag) {
							show_error("Invalid command outside data segment on line %d", line_offset);
						}

						do {
							if (read_greedy(&fp, buffer, 80)) {
								show_error("Value too large on line %d", line_offset);
								return -1;
							}

							imm = parse_imm(buffer, &endptr);
							if (*endptr != '\0') {
								show_error("Failed to parse value on line %d", line_offset);
								return -1;
							}

							memcpy(&memory[mem_pointer], &imm, 1);
							mem_pointer += 1;
						} while((c = fgetc(fp)) == ',');
		
						fseek(fp, -1, SEEK_CUR);

					} else {
						show_error("Unknown statement on line %d", line_offset);
						return -1;
					}
					command_flag = false;
				}

				break;

			case '.':
				if (comment_flag) break;

				if (command_flag) {
					show_error("Unexpected . on line %d", line_offset);
					return -1;
				}
				command_flag = true;
				i=0;
				break;

			default:
				if (comment_flag) break;
				if (!command_flag) {
					fseek(fp, -1, SEEK_CUR);
					return line_offset-1;
				}

				buffer[i] = c;
				i++;
				if (i==79) {
					show_error("Unknown statement on line %d", line_offset);
					return -1;
				}
				break;
		}
	}
}

// Reads in_fp and writes the same to out_fp while ignoring all whitespace, comments and labels (but makes a note of label positions)
int first_pass(FILE *in_fp, char *out_fp, label_index* index, vec* line_mapping, int line_offset) {
	char c;
	char label_buffer[128];
	int linecount = line_offset;
	int instruction_count = 0;
	int line_len = 0;
	bool comment_flag = false; // Are we reading a comment
	bool lw_flag = true; // leading whitespace flag
	bool cw_flag = false; // consecutive whitespace flag
	bool whitespace_flag = false; // is the current charachter whitespace (different rules are followed to decide if it should be written to output)
	bool instr_flag = false; // Was there an instruction on this line
	bool keep_reading = true;

	while (keep_reading) {
		c = fgetc(in_fp);
		switch (c) {
			case '#':
			case ';':
				comment_flag = true;
				break;

			case EOF:
				keep_reading = false;
			case '\n':
				if (instr_flag) {
					*(out_fp++) = '\n';
					append(line_mapping, linecount);
					instruction_count += 1;
				} 
				linecount += 1;
				line_len = 0;
				comment_flag = false;
				lw_flag = true;
				cw_flag = false;
				instr_flag = false;
				break;

			case ':':
				if (comment_flag) break;
				if (lw_flag) { 
					show_error("WARN: Empty Label on line %d, Stopping...\n", linecount);
					return -1;
				}
				if (line_len >= 128) {
					show_error("ERROR: Label too long on line %d, Stopping...\n", linecount);
					return -2;
				}

				label_buffer[line_len] = '\0';
				int prev_line = label_to_position(index, label_buffer);
				if (prev_line != -1) {
					show_error("ERROR: Pre-existing label defined on line %d repeated on %d, Stopping...\n", line_mapping->values[prev_line], linecount);
					return -3;
				}
				
				add_label(index, label_buffer, instruction_count);
				out_fp -= line_len;
				instr_flag = false;
				lw_flag = true;
				break;

			case ' ':
			case '\t':
				if (comment_flag) break;
				if (lw_flag || cw_flag) break;
				cw_flag = true;
				whitespace_flag = true;

			default:
				if (comment_flag) break;
				lw_flag = false;
				if (!whitespace_flag) cw_flag = false;
				instr_flag = true;
				if (line_len<128) label_buffer[line_len] = c;
				*(out_fp++) = c;
				line_len += 1;
		}
		whitespace_flag = false;
	}
	*(out_fp++) = '\0';
	return 0;
}


// Actually Encode all the instructions and write it to the int array.
int second_pass(char* clean_fp, int* hexcode, label_index* index, vec* line_mapping, bool debug) {

	char name[8]; // Sufficient for any valid instruction/pseudo instruction in Base class
	int instruction_count = 0;
	int i = 0;
	char c;

	while ((c = *(clean_fp++)) != '\0') {
		if (i<8) {
			if (c == ' ' || c == '\t' || c == '\n') {
				name[i] = '\0';

				// Attempt instruction translation
				
				// Checking name of instruction against known instructions
				const instruction_info* instruction = parse_instruction(name);

				if (!instruction) {
					show_error("Error on line %d: Unknown Instruction: (%s)", line_mapping->values[instruction_count], name);
					return 1;
				}

				int addend = 0;
				bool fail_flag = false;

				// Parsing arguments of instruction
				switch (instruction->handler_type) {
					case R_TYPE:
						addend = R_type_parser(&clean_fp, index, &line_mapping->values[instruction_count], instruction_count, &fail_flag);
						break;

					case I1_TYPE:
						addend = I1_type_parser(&clean_fp, index, &line_mapping->values[instruction_count], instruction_count, &fail_flag);
						break; 

					case I1B_TYPE:
						addend = I1B_type_parser(&clean_fp, index, &line_mapping->values[instruction_count], instruction_count, &fail_flag);
						break; 

					case I2_TYPE:
						addend = I2_type_parser(&clean_fp, index, &line_mapping->values[instruction_count], instruction_count, &fail_flag);
						break; 

					case S_TYPE:
						addend = S_type_parser(&clean_fp, index, &line_mapping->values[instruction_count], instruction_count, &fail_flag);
						break; 

					case B_TYPE:
						addend = B_type_parser(&clean_fp, index, &line_mapping->values[instruction_count], instruction_count, &fail_flag);
						break; 

					case U_TYPE:
						addend = U_type_parser(&clean_fp, index, &line_mapping->values[instruction_count], instruction_count, &fail_flag);
						break; 

					case J_TYPE:
						addend = J_type_parser(&clean_fp, index, &line_mapping->values[instruction_count], instruction_count, &fail_flag);
						break; 

					case I3_TYPE:
						addend = I3_type_parser(&clean_fp, index, &line_mapping->values[instruction_count], instruction_count, &fail_flag);
						break; 

					case I4_TYPE:
						break;

					// case P_TYPE:
					// 	addend = P_type_parser(&clean_fp, index, &line_mapping->values[instruction_count], instruction_count, &fail_flag, constants);
					// 	break; 
						
					default:
						show_error("Error on line %d: Unclassified type, This should not have happened!", line_mapping->values[instruction_count]);
						return 1;
				}
				
				if (fail_flag) return 1;

				// Write the instruction to the output buffer
				hexcode[instruction_count] = instruction->constant + addend;
				if (debug) show_error("Instruction %d: %08X", instruction_count, hexcode[instruction_count]);
				
				instruction_count++;
				i = 0;

			} else name[i++] = c;
		} else {
			// No valid instruction would be this long
			name[7] = '\0';
			show_error("Error on line %d: Invalid Instruction: %s", line_mapping->values[instruction_count], name);
			return 1;
		}

	}
		
	return 0;
}

int* assembler_main(FILE* in_fp, char* cleaned, label_index* index, uint8_t* memory) {

	// Initializing and Parsing command line switches
	bool debug = false;
	bool binary = true;

	vec *line_mapping;
	int result;
	line_mapping = new_managed_array();

	// Perform the pre-processing
	if ((result = pre_pass(&in_fp, memory)) == -1) {
		return NULL;
	}

	// Perform the first pass
	if ((result = first_pass(in_fp, cleaned, index, line_mapping, result)) != 0) {
		return NULL;
	}

	int* hexcode = malloc(sizeof(int)*(line_mapping->len+1));
	hexcode[0] = line_mapping->len;

	// Perform the second pass
	if ((result = second_pass(cleaned, &hexcode[1], index, line_mapping, debug)) != 0) {
		return NULL;
	}

	free_managed_array(line_mapping);
	return hexcode;
}

int new_pre_pass(FILE **in_fp, uint8_t* memory) {
	FILE* fp = *in_fp;
	char c;
	char buffer[80];
	char *endptr;
	int i=0;
	int line_offset = 1;
	int mem_pointer = DATA_BASE;
	long imm = 0;
	bool data_flag = true;
	bool comment_flag = false; // Are we reading a comment
	bool command_flag = false;
	bool got_command = false;

	while(1) {
		c = fgetc(fp);
		switch (c) {
			case '#':
			case ';':
				comment_flag = true;
				if (command_flag) {
					show_error("Unexpected comment on line %d", line_offset);
					return -1;
				}
				break;

			case EOF:
				return 0;

			case '\n':
				line_offset++;
				comment_flag = false;
				if (got_command) return 0;
				
			case ' ':
			case '\t':
				if (command_flag) {
					buffer[i] = '\0';
					if (!strcmp(buffer, "data")) {
						data_flag = true;

					} else if (!strcmp(buffer, "text")) {
						fseek(fp, -1, SEEK_CUR);
						return line_offset-1;

					} else if (!strcmp(buffer, "dword")) {
						if (!data_flag) {
							show_error("Invalid command outside data segment on line %d", line_offset);
						}

						do {
							if (read_greedy(&fp, buffer, 80)) {
								show_error("Value too large on line %d", line_offset);
								return -1;
							}

							imm = parse_imm(buffer, &endptr);
							if (*endptr != '\0') {
								show_error("Failed to parse value on line %d", line_offset);
								return -1;
							}

							memcpy(&memory[mem_pointer], &imm, 8);
							mem_pointer += 8;
						} while((c = fgetc(fp)) == ',');
		
						fseek(fp, -1, SEEK_CUR);

					} else if (!strcmp(buffer, "word")) {
						if (!data_flag) {
							show_error("Invalid command outside data segment on line %d", line_offset);
						}

						do {
							if (read_greedy(&fp, buffer, 80)) {
								show_error("Value too large on line %d", line_offset);
								return -1;
							}

							imm = parse_imm(buffer, &endptr);
							if (*endptr != '\0') {
								show_error("Failed to parse value on line %d", line_offset);
								return -1;
							}

							memcpy(&memory[mem_pointer], &imm, 4);
							mem_pointer += 4;
						} while((c = fgetc(fp)) == ',');
		
						fseek(fp, -1, SEEK_CUR);

					} else if (!strcmp(buffer, "half")) {
						if (!data_flag) {
							show_error("Invalid command outside data segment on line %d", line_offset);
						}

						do {
							if (read_greedy(&fp, buffer, 80)) {
								show_error("Value too large on line %d", line_offset);
								return -1;
							}

							imm = parse_imm(buffer, &endptr);
							if (*endptr != '\0') {
								show_error("Failed to parse value on line %d", line_offset);
								return -1;
							}

							memcpy(&memory[mem_pointer], &imm, 2);
							mem_pointer += 2;
						} while((c = fgetc(fp)) == ',');
		
						fseek(fp, -1, SEEK_CUR);
					
					} else if (!strcmp(buffer, "byte")) {
						if (!data_flag) {
							show_error("Invalid command outside data segment on line %d", line_offset);
						}

						do {
							if (read_greedy(&fp, buffer, 80)) {
								show_error("Value too large on line %d", line_offset);
								return -1;
							}

							imm = parse_imm(buffer, &endptr);
							if (*endptr != '\0') {
								show_error("Failed to parse value on line %d", line_offset);
								return -1;
							}

							memcpy(&memory[mem_pointer], &imm, 1);
							mem_pointer += 1;
						} while((c = fgetc(fp)) == ',');
		
						fseek(fp, -1, SEEK_CUR);

					} else {
						show_error("Unknown statement on line %d", line_offset);
						return -1;
					}
					command_flag = false;
				}

				break;

			case '.':
				if (comment_flag) break;

				if (command_flag) {
					show_error("Unexpected . on line %d", line_offset);
					return -1;
				}
				command_flag = true;
				got_command = true;
				i=0;
				break;

			default:
				if (comment_flag) break;
				if (!command_flag) {
					fseek(fp, -1, SEEK_CUR);
					return line_offset-1;
				}

				buffer[i] = c;
				i++;
				if (i==79) {
					show_error("Unknown statement on line %d", line_offset);
					return -1;
				}
				break;
		}
	}
}

int data_only_run(FILE* in_fp, uint8_t* memory) {
	if (new_pre_pass(&in_fp, memory) == -1) return 1;
	return 0;
}
