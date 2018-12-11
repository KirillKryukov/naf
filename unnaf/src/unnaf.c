/*
 * unnaf - NAF format decoder
 *
 * Version 0.1.1 (October 8, 2018)
 *
 * Copyright (c) 2018 Kirill Kryukov. All rights reserved.
 *
 * This source code is licensed under the zlib/libpng license, found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zstd.h>

#include "unnaf-utils.c"

typedef enum { UNDECIDED, FORMAT_NAME, PART_LIST, PART_SIZES, NUMBER_OF_SEQUENCES,
               TITLE, IDS, NAMES, LENGTHS, TOTAL_LENGTH, MASK, TOTAL_MASK_LENGTH,
               FOUR_BIT, DNA, MASKED_DNA, FASTA, MASKED_FASTA, FASTQ
             } OUTPUT_TYPE;

static OUTPUT_TYPE out_type = UNDECIDED;

static char *in_file_name = 0;
static FILE *IN = 0;

unsigned char header[6];
int has_title = 0;
int has_ids = 0;
int has_names = 0;
int has_lengths = 0;
int has_mask = 0;
int has_data = 0;
int has_quality = 0;
unsigned long long max_line_length;
unsigned long long N;



char *ids_buffer = 0;
unsigned char *compressed_ids_buffer = 0;
char **ids = 0;

char *names_buffer = 0;
unsigned char *compressed_names_buffer = 0;
char **names = 0;

unsigned int *lengths_buffer = 0;
unsigned char *compressed_lengths_buffer = 0;
unsigned long long n_lengths = 0;

unsigned long long mask_size = 0;
unsigned char *mask_buffer = 0;
unsigned char *compressed_mask_buffer = 0;

unsigned long long total_seq_length = 0;
unsigned long long compressed_seq_size = 0;
unsigned char* compressed_seq_buffer = 0;
unsigned long long compressed_seq_pos = 0;

unsigned long long total_quality_length = 0;
unsigned long long compressed_quality_size = 0;



size_t in_buffer_size = 0;
char *in_buffer = 0;

size_t out_buffer_size = 0;
char *out_buffer = 0;

size_t mem_out_buffer_size = 0;
unsigned char *mem_out_buffer = 0;

size_t out_print_buffer_size = 0;
unsigned char *out_print_buffer = 0;

ZSTD_DStream *input_decompression_stream = 0;
size_t file_bytes_to_read;
ZSTD_inBuffer zstd_file_in_buffer;

ZSTD_DStream *memory_decompression_stream = 0;
size_t memory_bytes_to_read;
ZSTD_inBuffer zstd_mem_in_buffer;



unsigned long long cur_seq_index = 0;
unsigned long long cur_seq_pos = 0;

char *dna_buffer = 0;
size_t dna_buffer_size = 0;
size_t dna_buffer_flush_size = 0;
unsigned dna_buffer_pos = 0;
unsigned dna_buffer_filling_pos = 0;
unsigned dna_buffer_printing_pos = 0;
unsigned dna_buffer_remaining = 0;

char *quality_buffer = 0;
size_t quality_buffer_size = 0;
size_t quality_buffer_flush_size = 0;
unsigned quality_buffer_filling_pos = 0;
unsigned quality_buffer_printing_pos = 0;
unsigned quality_buffer_remaining = 0;

unsigned long long total_seq_n_bp_remaining = 0;

unsigned long long cur_seq_len_index = 0;
unsigned long long cur_seq_len_n_bp_remaining = 0;

unsigned long long cur_qual_len_index = 0;

unsigned long long cur_mask = 0;
unsigned int cur_mask_remaining = 0;
int mask_on = 0;

unsigned long long cur_line_n_bp_remaining = 0;

#include "unnaf-input.c"
#include "unnaf-output.c"
#include "unnaf-output-fastq.c"


static void done()
{
    if (IN != stdin) { fclose(IN); IN = 0; }

    if (ids) { free(ids); ids = 0; }
    if (ids_buffer) { free(ids_buffer); ids_buffer = 0; }
    if (compressed_ids_buffer) { free(compressed_ids_buffer); compressed_ids_buffer = 0; }

    if (names) { free(names); names = 0; }
    if (names_buffer) { free(names_buffer); names_buffer = 0; }
    if (compressed_names_buffer) { free(compressed_names_buffer); compressed_names_buffer = 0; }

    if (lengths_buffer) { free(lengths_buffer); lengths_buffer = 0; }
    if (compressed_lengths_buffer) { free(compressed_lengths_buffer); compressed_lengths_buffer = 0; }

    if (mask_buffer) { free(mask_buffer); mask_buffer = 0; }
    if (compressed_mask_buffer) { free(compressed_mask_buffer); compressed_mask_buffer = 0; }

    if (compressed_seq_buffer) { free(compressed_seq_buffer); compressed_seq_buffer = 0; }

    if (in_buffer) { free(in_buffer); in_buffer = 0; } 
    if (out_buffer) { free(out_buffer); out_buffer = 0; } 
    if (mem_out_buffer) { free(mem_out_buffer); mem_out_buffer = 0; }
    if (out_print_buffer) { free(out_print_buffer); out_print_buffer = 0; }
    if (input_decompression_stream) { ZSTD_freeDStream(input_decompression_stream); input_decompression_stream = 0; }
    if (memory_decompression_stream) { ZSTD_freeDStream(memory_decompression_stream); memory_decompression_stream = 0; }

    if (dna_buffer) { free(dna_buffer); dna_buffer = 0; }
    if (quality_buffer) { free(quality_buffer); quality_buffer = 0; }
}



static void init()
{
    atexit(done);
    init_tables();
}



static void usage()
{
    fprintf(stderr,
        "Usage: unnaf [OUTPUT-TYPE] [file.naf]\n"
        "Output type choices:\n"
        "  -format    - File format version\n"
        "  -part-list - List of parts\n"
        "  -sizes     - Part sizes\n"
        "  -number    - Number of sequences\n"
        "  -title     - Dataset title\n"
        "  -ids       - Sequence ids (accession numbers)\n"
        "  -names     - Full sequence names (including ids)\n"
        "  -lengths   - Sequence lengths\n"
        "  -total-length - Sum of sequence lengths\n"
        "  -mask      - Masked region lengths\n"
        "  -4bit      - 4bit-encoded DNA (binary data)\n"
        "  -dna       - Continuous DNA sequence without mask\n"
        "  -masked-dna - Continuous masked DNA sequence\n"
        "  -fasta     - FASTA-formatted sequences\n"
        "  -masked-fasta - Masked FASTA-formatted sequences\n"
        "  -fastq     - FASTQ-formatted sequences\n"
    );
}



static void set_out_type(OUTPUT_TYPE new_type)
{
    if (out_type != UNDECIDED) { fprintf(stderr, "Can specify only one output type\n"); exit(1); }
    out_type = new_type;
}


static void set_input_file_name(char *new_name)
{
    if (in_file_name != 0) { fprintf(stderr, "Can process only one file at a time\n"); exit(1); }
    in_file_name = new_name;
}


int main(int argc, char **argv)
{
    check_platform();
    init();

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (!strcmp(argv[i], "-help")) { usage(); exit(0); }
            else if (!strcmp(argv[i], "-format"           )) { set_out_type(FORMAT_NAME); }
            else if (!strcmp(argv[i], "-part-list"        )) { set_out_type(PART_LIST); }
            else if (!strcmp(argv[i], "-sizes"            )) { set_out_type(PART_SIZES); }
            else if (!strcmp(argv[i], "-number"           )) { set_out_type(NUMBER_OF_SEQUENCES); }
            else if (!strcmp(argv[i], "-title"            )) { set_out_type(TITLE); }
            else if (!strcmp(argv[i], "-ids"              )) { set_out_type(IDS); }
            else if (!strcmp(argv[i], "-names"            )) { set_out_type(NAMES); }
            else if (!strcmp(argv[i], "-lengths"          )) { set_out_type(LENGTHS); }
            else if (!strcmp(argv[i], "-total-length"     )) { set_out_type(TOTAL_LENGTH); }
            else if (!strcmp(argv[i], "-mask"             )) { set_out_type(MASK); }
            else if (!strcmp(argv[i], "-total-mask-length")) { set_out_type(TOTAL_MASK_LENGTH); }
            else if (!strcmp(argv[i], "-4bit"             )) { set_out_type(FOUR_BIT); }
            else if (!strcmp(argv[i], "-dna"              )) { set_out_type(DNA); }
            else if (!strcmp(argv[i], "-masked-dna"       )) { set_out_type(MASKED_DNA); }
            else if (!strcmp(argv[i], "-fasta"            )) { set_out_type(FASTA); }
            else if (!strcmp(argv[i], "-masked-fasta"     )) { set_out_type(MASKED_FASTA); }
            else if (!strcmp(argv[i], "-fastq"            )) { set_out_type(FASTQ); }
            else { fprintf(stderr, "Unknown option \"%s\"\n", argv[i]); exit(1); }
        }
        else { set_input_file_name(argv[i]); }
    }

    if (in_file_name != 0)
    {
        IN = fopen(in_file_name, "rb");
        if (!IN) { fprintf(stderr, "Can't open input file\n"); exit(1); }
    }
    else
    {
        if ( !freopen(0, "rb", stdin)
#if _WIN32
             && _setmode(_fileno(stdin), _O_BINARY) < 0
#endif
           ) { fprintf(stderr, "Can't read input in binary mode\n"); exit(1); }
        IN = stdin;
    }

    read_header();

    if (out_type == UNDECIDED)
    {
        if (has_quality) { out_type = FASTQ; }
        else { out_type = MASKED_FASTA; }
    }

    if (out_type == FORMAT_NAME) { printf("NAF v.%d\n", header[3]); exit(0); }
    if (out_type == PART_LIST) { print_list_of_parts_and_exit(); }

    max_line_length = read_number(IN);
    N = read_number(IN);
    if (out_type == NUMBER_OF_SEQUENCES) { printf("%llu\n", N); exit(0); }
    if (!N) { exit(0); }

    if (out_type == PART_SIZES) { print_part_sizes_and_exit(); }
    if (out_type == TITLE) { print_title_and_exit(); }

    skip_title();

    if (out_type == IDS) { print_ids_and_exit(); }
    if (out_type == NAMES) { print_names_and_exit(); }
    if (out_type == LENGTHS) { print_lengths_and_exit(); }
    if (out_type == TOTAL_LENGTH) { print_total_length_and_exit(); }
    if (out_type == MASK) { print_mask_and_exit(); }
    if (out_type == TOTAL_MASK_LENGTH) { print_total_mask_length_and_exit(); }
    if (out_type == FOUR_BIT) { print_4bit_and_exit(); }

    dna_buffer_flush_size = ZSTD_DStreamOutSize() * 2;
    dna_buffer_size = dna_buffer_flush_size * 2 + 10;
    dna_buffer = (char *)malloc(dna_buffer_size);
    if (!dna_buffer) { fprintf(stderr, "Can't allocate %zu bytes for dna buffer\n", dna_buffer_size); exit(1); }

    out_print_buffer_size = dna_buffer_size * 2;
    out_print_buffer = (unsigned char *)malloc(out_print_buffer_size);
    if (!out_print_buffer) { fprintf(stderr, "Can't allocate %zu bytes for dna buffer\n", out_print_buffer_size); exit(1); }

    if (out_type == DNA) { print_dna_and_exit(0); }
    if (out_type == MASKED_DNA) { print_dna_and_exit(has_mask); }

    if (out_type == FASTA) { print_fasta_and_exit(0); }
    if (out_type == MASKED_FASTA) { print_fasta_and_exit(has_mask); }

    if (out_type == FASTQ) { print_fastq_and_exit(0); }

    return 0;
}
