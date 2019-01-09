/*
 * NAF decompressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

#define VERSION "1.0.0"
#define DATE "2019-01-08"
#define COPYRIGHT_YEARS "2018-2019"

#define NDEBUG

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <zstd.h>

#include "utils.c"

typedef enum { UNDECIDED, FORMAT_NAME, PART_LIST, PART_SIZES, NUMBER_OF_SEQUENCES,
               TITLE, IDS, NAMES, LENGTHS, TOTAL_LENGTH, MASK, TOTAL_MASK_LENGTH,
               FOUR_BIT, DNA, MASKED_DNA, FASTA, MASKED_FASTA, FASTQ
             } OUTPUT_TYPE;

static OUTPUT_TYPE out_type = UNDECIDED;

static char *in_file_path = NULL;
static FILE *IN = NULL;

static unsigned char header[6];
static int has_title = 0;
static int has_ids = 0;
static int has_names = 0;
static int has_lengths = 0;
static int has_mask = 0;
static int has_data = 0;
static int has_quality = 0;
static unsigned long long max_line_length;
static unsigned long long N;


static char *ids_buffer = NULL;
static unsigned char *compressed_ids_buffer = NULL;
static char **ids = NULL;

static char *names_buffer = NULL;
static unsigned char *compressed_names_buffer = NULL;
static char **names = NULL;

static unsigned int *lengths_buffer = NULL;
static unsigned char *compressed_lengths_buffer = NULL;
static unsigned long long n_lengths = 0;

static unsigned long long mask_size = 0;
static unsigned char *mask_buffer = NULL;
static unsigned char *compressed_mask_buffer = NULL;

static unsigned long long total_seq_length = 0;
static unsigned long long compressed_seq_size = 0;
static unsigned char *compressed_seq_buffer = NULL;
static unsigned long long compressed_seq_pos = 0;

static unsigned long long total_quality_length = 0;
static unsigned long long compressed_quality_size = 0;


static size_t in_buffer_size = 0;
static char *in_buffer = NULL;

static size_t out_buffer_size = 0;
static char *out_buffer = NULL;

static size_t mem_out_buffer_size = 0;
static unsigned char *mem_out_buffer = NULL;

static size_t out_print_buffer_size = 0;
static unsigned char *out_print_buffer = NULL;

static ZSTD_DStream *input_decompression_stream = NULL;
static size_t file_bytes_to_read;
static ZSTD_inBuffer zstd_file_in_buffer;

static ZSTD_DStream *memory_decompression_stream = NULL;
static size_t memory_bytes_to_read;
static ZSTD_inBuffer zstd_mem_in_buffer;


static unsigned long long cur_seq_index = 0;

static unsigned char *dna_buffer = NULL;
static size_t dna_buffer_size = 0;
static size_t dna_buffer_flush_size = 0;
static unsigned dna_buffer_pos = 0;
static unsigned dna_buffer_filling_pos = 0;
static unsigned dna_buffer_printing_pos = 0;
static unsigned dna_buffer_remaining = 0;

static char *quality_buffer = NULL;
static size_t quality_buffer_size = 0;
static size_t quality_buffer_flush_size = 0;
static unsigned quality_buffer_filling_pos = 0;
static unsigned quality_buffer_printing_pos = 0;
static unsigned quality_buffer_remaining = 0;

static unsigned long long total_seq_n_bp_remaining = 0;

static unsigned long long cur_seq_len_index = 0;
static unsigned long long cur_seq_len_n_bp_remaining = 0;

static unsigned long long cur_qual_len_index = 0;

static unsigned long long cur_mask = 0;
static unsigned int cur_mask_remaining = 0;
static int mask_on = 0;

static unsigned long long cur_line_n_bp_remaining = 0;

#include "input.c"
#include "output.c"
#include "output-fastq.c"


#define FREE(p) \
do { if ((p) != NULL) { free(p); (p) = NULL; } } while (0)


static void done(void)
{
    if (IN != NULL && IN != stdin) { fclose(IN); IN = NULL; }

    FREE(ids);
    FREE(ids_buffer);
    FREE(compressed_ids_buffer);

    FREE(names);
    FREE(names_buffer);
    FREE(compressed_names_buffer);

    FREE(lengths_buffer);
    FREE(compressed_lengths_buffer);

    FREE(mask_buffer);
    FREE(compressed_mask_buffer);

    FREE(compressed_seq_buffer);

    FREE(in_buffer);
    FREE(out_buffer);
    FREE(mem_out_buffer);
    FREE(out_print_buffer);
    FREE(input_decompression_stream);
    FREE(memory_decompression_stream);

    FREE(dna_buffer);
    FREE(quality_buffer);
}


static void show_version(void)
{
    fprintf(stderr, "unnaf - NAF decompressor, version " VERSION ", " DATE "\nCopyright (c) " COPYRIGHT_YEARS " Kirill Kryukov\n");
}


static void show_help(void)
{
    fprintf(stderr,
        "Usage: unnaf [OUTPUT-TYPE] [file.naf]\n"
        "Output type choices:\n"
        "  --format    - File format version\n"
        "  --part-list - List of parts\n"
        "  --sizes     - Part sizes\n"
        "  --number    - Number of sequences\n"
        "  --title     - Dataset title\n"
        "  --ids       - Sequence ids (accession numbers)\n"
        "  --names     - Full sequence names (including ids)\n"
        "  --lengths   - Sequence lengths\n"
        "  --total-length - Sum of sequence lengths\n"
        "  --mask      - Masked region lengths\n"
        "  --4bit      - 4bit-encoded DNA (binary data)\n"
        "  --dna       - Continuous DNA sequence without mask\n"
        "  --masked-dna - Continuous masked DNA sequence\n"
        "  --fasta     - FASTA-formatted sequences\n"
        "  --masked-fasta - Masked FASTA-formatted sequences\n"
        "  --fastq     - FASTQ-formatted sequences\n"
    );
}


static void set_out_type(OUTPUT_TYPE new_type)
{
    if (out_type != UNDECIDED) { fprintf(stderr, "Error: Only one output type should be specified\n"); exit(1); }
    out_type = new_type;
}


static void set_input_file_path(char *new_path)
{
    assert(new_path != NULL);

    if (in_file_path != NULL) { fprintf(stderr, "Error: Can process only one file at a time\n"); exit(1); }
    if (*new_path == '\0') { fprintf(stderr, "Error: empty input path specified\n"); exit(1); }
    in_file_path = new_path;
}


int main(int argc, char **argv)
{
    atexit(done);
    init_tables();

    bool print_version = false;

    for (int i = 1; i < argc; i++)
    {
        if (!strncmp(argv[i], "--", 2))
        {
            if (!strcmp(argv[i], "--help")) { show_help(); exit(0); }
            else if (!strcmp(argv[i], "--format"           )) { set_out_type(FORMAT_NAME); }
            else if (!strcmp(argv[i], "--part-list"        )) { set_out_type(PART_LIST); }
            else if (!strcmp(argv[i], "--sizes"            )) { set_out_type(PART_SIZES); }
            else if (!strcmp(argv[i], "--number"           )) { set_out_type(NUMBER_OF_SEQUENCES); }
            else if (!strcmp(argv[i], "--title"            )) { set_out_type(TITLE); }
            else if (!strcmp(argv[i], "--ids"              )) { set_out_type(IDS); }
            else if (!strcmp(argv[i], "--names"            )) { set_out_type(NAMES); }
            else if (!strcmp(argv[i], "--lengths"          )) { set_out_type(LENGTHS); }
            else if (!strcmp(argv[i], "--total-length"     )) { set_out_type(TOTAL_LENGTH); }
            else if (!strcmp(argv[i], "--mask"             )) { set_out_type(MASK); }
            else if (!strcmp(argv[i], "--total-mask-length")) { set_out_type(TOTAL_MASK_LENGTH); }
            else if (!strcmp(argv[i], "--4bit"             )) { set_out_type(FOUR_BIT); }
            else if (!strcmp(argv[i], "--dna"              )) { set_out_type(DNA); }
            else if (!strcmp(argv[i], "--masked-dna"       )) { set_out_type(MASKED_DNA); }
            else if (!strcmp(argv[i], "--fasta"            )) { set_out_type(FASTA); }
            else if (!strcmp(argv[i], "--masked-fasta"     )) { set_out_type(MASKED_FASTA); }
            else if (!strcmp(argv[i], "--fastq"            )) { set_out_type(FASTQ); }
            else if (!strcmp(argv[i], "--version")) { print_version = true; }
            else { fprintf(stderr, "Unknown option \"%s\"\n", argv[i]); exit(1); }
        }
        else { set_input_file_path(argv[i]); }
    }

    if (print_version)
    {
        show_version();
        exit(0);
    }

    if (in_file_path != NULL)
    {
        IN = fopen(in_file_path, "rb");
        if (IN == NULL) { fprintf(stderr, "Can't open input file\n"); exit(1); }
    }
    else
    {
        if (isatty(fileno(stdin))) { fprintf(stderr, "Error: Input file not specified and no input pipe\n"); exit(1); }

        if ( !freopen(NULL, "rb", stdin)
#if _WIN32                	
             && _setmode(_fileno(stdin), _O_BINARY) < 0
#endif
           ) { fprintf(stderr, "Can't read input in binary mode\n"); exit(1); }
        IN = stdin;
    }

    read_header();

    if (out_type == UNDECIDED)
    {
        out_type = has_quality ? FASTQ : MASKED_FASTA;
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
    dna_buffer = (unsigned char *)malloc(dna_buffer_size);
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
