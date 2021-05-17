/*
 * NAF compressor
 * Copyright (c) 2018-2021 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

static void copy_file_to_out(FILE* FROM, char *from_path, long start, unsigned long long data_size);
static void write_variable_length_encoded_number(FILE *F, unsigned long long a);
