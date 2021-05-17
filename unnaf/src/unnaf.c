/*
 * NAF decompressor
 * Copyright (c) 2018-2021 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

#define VERSION "1.3.0"
#define DATE "2021-05-17"
#define COPYRIGHT_YEARS "2018-2021"

#include "platform.h"

static unsigned char code_to_nuc[16] = {'-','T','G','K','C','Y','S','B','A','W','R','D','M','H','V','N'};
static unsigned short codes_to_nucs[256];

typedef enum { UNDECIDED, FORMAT_NAME, PART_LIST, PART_SIZES, NUMBER_OF_SEQUENCES,
               TITLE, IDS, NAMES, LENGTHS, TOTAL_LENGTH, MASK, TOTAL_MASK_LENGTH,
               FOUR_BIT,
               DNA, MASKED_DNA, UNMASKED_DNA,
               SEQ, SEQUENCES, CHARCOUNT,
               FASTA, MASKED_FASTA, UNMASKED_FASTA,
               FASTQ
             } OUTPUT_TYPE;

static OUTPUT_TYPE out_type = UNDECIDED;

enum { seq_type_dna, seq_type_rna, seq_type_protein, seq_type_text };
static int in_seq_type = seq_type_dna;
static const char *in_seq_type_name = "DNA";

static bool verbose = false;
static bool binary_stderr = false;
static bool use_mask = true;

static char *in_file_path = NULL;
static FILE *IN = NULL;
static struct stat input_stat;
static bool have_input_stat = false;

static char *out_file_path = NULL;
static char *out_file_path_auto = NULL;
static FILE *OUT = NULL;
static bool force_stdout = false;
static bool binary_stdout = false;
static bool created_output_file = false;

static unsigned char format_version = 1;
static unsigned char name_separator = ' ';

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

static bool line_length_is_specified = false;
static unsigned long long requested_line_length = 0ull;

static bool success = false;


#include "utils.c"
#include "files.c"
#include "input.c"
#include "output.c"
#include "output-sequences.c"
#include "output-fastq.c"


#define FREE(p) \
do { if ((p) != NULL) { free(p); (p) = NULL; } } while (0)


static void done(void)
{
    if (IN != NULL && IN != stdin) { fclose(IN); IN = NULL; }

    close_input_file();
    close_output_file();

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

    if (!success && created_output_file)
    {
        if (remove(out_file_path) != 0) { err("can't remove incomplete output file \"%s\"\n", out_file_path); }
    }

    FREE(out_file_path_auto);
}


static void set_out_type(OUTPUT_TYPE new_type)
{
    if (out_type != UNDECIDED) { die("only one output type should be specified\n"); }
    out_type = new_type;
}


static void set_input_file_path(char *new_path)
{
    assert(new_path != NULL);

    if (in_file_path != NULL) { die("can process only one file at a time\n"); }
    if (*new_path == '\0') { die("empty input path specified\n"); }
    in_file_path = new_path;
}


static void set_output_file_path(char *new_path)
{
    assert(new_path != NULL);

    if (out_file_path != NULL) { die("double --out parameter\n"); }
    if (*new_path == '\0') { die("empty --out parameter\n"); }
    out_file_path = new_path;
}


static void set_line_length(char *str)
{
    assert(str != NULL);

    char *end;
    long long a = strtoll(str, &end, 10);
    if (*end != '\0') { die("can't parse the value of --line-length parameter\n"); }
    if (a < 0ll) { die("negative line length specified\n"); }

    char test_str[21];
    int nc = snprintf(test_str, 21, "%lld", a);
    if (nc < 1 || nc > 20 || strcmp(test_str, str) != 0) { die("can't parse the value of --line-length parameter\n"); }

    requested_line_length = (unsigned long long) a;
    line_length_is_specified = true;
}


static void show_version(void)
{
    msg("unnaf - NAF decompressor, version " VERSION ", " DATE "\nCopyright (c) " COPYRIGHT_YEARS " Kirill Kryukov\n");
    if (verbose) { msg("Built with zstd " ZSTD_VERSION_STRING ", using runtime zstd %s\n", ZSTD_versionString()); }
}


static void show_help(void)
{
    msg("Usage: unnaf [OUTPUT-TYPE] [file.naf]\n"
        "Options for selecting output type:\n"
        "  --format        - File format version\n"
        "  --part-list     - List of parts\n"
        "  --sizes         - Part sizes\n"
        "  --number        - Number of sequences\n"
        "  --title         - Dataset title\n"
        "  --ids           - Sequence ids (accession numbers)\n"
        "  --names         - Full sequence names (including ids)\n"
        "  --lengths       - Sequence lengths\n"
        "  --total-length  - Sum of sequence lengths\n"
        "  --mask          - Masked region lengths\n"
        "  --4bit          - 4bit-encoded nucleotide sequence (binary data)\n"
        "  --seq           - Continuous concatenated sequence\n"
        "  --sequences     - One sequence per line, no names\n"
        "  --fasta         - FASTA-formatted sequences\n"
        "  --fastq         - FASTQ-formatted sequences\n"
        "Other options:\n"
        "  -o FILE         - Decompress into FILE\n"
        "  -c              - Write to standard output\n"
        "  --line-length N - Use lines of width N for FASTA output\n"
        "  --no-mask       - Ignore mask\n"
        "  --binary-stdout - Set stdout stream to binary mode.\n"
        "  --binary-stderr - Set stderr stream to binary mode.\n"
        "  --binary        - Shortcut for \"--binary-stdout --binary-stderr\"\n"
        "  -h, --help      - Show help\n"
        "  -V, --version   - Show version\n"
    );
}


static void parse_command_line(int argc, char **argv)
{
    bool print_version = false;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (argv[i][1] == '-')
            {
                if (i < argc - 1)
                {
                    if (!strcmp(argv[i], "--line-length")) { i++; set_line_length(argv[i]); continue; }
                }
                if (!strcmp(argv[i], "--format"           )) { set_out_type(FORMAT_NAME        ); continue; }
                if (!strcmp(argv[i], "--part-list"        )) { set_out_type(PART_LIST          ); continue; }
                if (!strcmp(argv[i], "--sizes"            )) { set_out_type(PART_SIZES         ); continue; }
                if (!strcmp(argv[i], "--number"           )) { set_out_type(NUMBER_OF_SEQUENCES); continue; }
                if (!strcmp(argv[i], "--title"            )) { set_out_type(TITLE              ); continue; }
                if (!strcmp(argv[i], "--ids"              )) { set_out_type(IDS                ); continue; }
                if (!strcmp(argv[i], "--names"            )) { set_out_type(NAMES              ); continue; }
                if (!strcmp(argv[i], "--lengths"          )) { set_out_type(LENGTHS            ); continue; }
                if (!strcmp(argv[i], "--total-length"     )) { set_out_type(TOTAL_LENGTH       ); continue; }
                if (!strcmp(argv[i], "--mask"             )) { set_out_type(MASK               ); continue; }
                if (!strcmp(argv[i], "--total-mask-length")) { set_out_type(TOTAL_MASK_LENGTH  ); continue; }
                if (!strcmp(argv[i], "--4bit"             )) { set_out_type(FOUR_BIT           ); continue; }
                if (!strcmp(argv[i], "--seq"              )) { set_out_type(SEQ                ); continue; }
                if (!strcmp(argv[i], "--sequences"        )) { set_out_type(SEQUENCES          ); continue; }
                if (!strcmp(argv[i], "--charcount"        )) { set_out_type(CHARCOUNT          ); continue; }
                if (!strcmp(argv[i], "--fasta"            )) { set_out_type(FASTA              ); continue; }
                if (!strcmp(argv[i], "--fastq"            )) { set_out_type(FASTQ              ); continue; }
                if (!strcmp(argv[i], "--no-mask")) { use_mask = false; continue; }
                if (!strcmp(argv[i], "--binary-stdout")) { binary_stdout = true; continue; }
                if (!strcmp(argv[i], "--binary-stderr")) { if (!binary_stderr) { binary_stderr = true; change_stderr_to_binary(); } continue; }
                if (!strcmp(argv[i], "--binary")) { binary_stdout = true; if (!binary_stderr) { binary_stderr = true; change_stderr_to_binary(); } continue; }
                if (!strcmp(argv[i], "--help")) { show_help(); exit(0); }
                if (!strcmp(argv[i], "--verbose")) { verbose = true; continue; }
                if (!strcmp(argv[i], "--version")) { print_version = true; continue; }

                // Deprecated undocumented options.
                if (!strcmp(argv[i], "--dna"              )) { set_out_type(DNA); continue; }            // Instead use "--seq"
                if (!strcmp(argv[i], "--masked-dna"       )) { set_out_type(MASKED_DNA); continue; }     // Instead use "--seq"
                if (!strcmp(argv[i], "--unmasked-dna"     )) { set_out_type(UNMASKED_DNA); continue; }   // Instead use "--seq --no-mask"
                if (!strcmp(argv[i], "--masked-fasta"     )) { set_out_type(MASKED_FASTA); continue; }   // Instead use "--fasta"
                if (!strcmp(argv[i], "--unmasked-fasta"   )) { set_out_type(UNMASKED_FASTA); continue; } // Instead use "--fasta --no-mask"
            }

            if (i < argc - 1)
            {
                if (!strcmp(argv[i], "-o")) { i++; set_output_file_path(argv[i]); continue; }
            }

            if (!strcmp(argv[i], "-c")) { force_stdout = true; continue; }
            if (!strcmp(argv[i], "-h")) { show_help(); exit(0); }
            if (!strcmp(argv[i], "-V")) { print_version = true; continue; }

            die("unknown or incomplete argument \"%s\"\n", argv[i]);
        }
        set_input_file_path(argv[i]);
    }

    if (print_version)
    {
        show_version();
        exit(0);
    }

    if (force_stdout && out_file_path != NULL)
    {
        die("-c and -o arguments can't be used together\n");
    }
}


int main(int argc, char **argv)
{
    atexit(done);

    parse_command_line(argc, argv);
    if (in_file_path == NULL && isatty(fileno(stdin)))
    {
        err("no input specified, use \"unnaf -h\" for help\n");
        exit(0);
    }

    open_input_file();
    read_header();
    if (in_seq_type == seq_type_rna) { code_to_nuc[1] = 'U'; }
    if (in_seq_type <= seq_type_rna) { init_tables(); }

    if (out_type == UNDECIDED)
    {
        out_type = has_quality ? FASTQ : FASTA;
    }

    if ((out_type == DNA || out_type == MASKED_DNA || out_type == UNMASKED_DNA) && (in_seq_type != seq_type_dna))
    {
        die("input has not DNA, but %s data\n", in_seq_type_name);
    }
    if (out_type == FOUR_BIT && in_seq_type >= seq_type_protein)
    {
        die("input has no 4-bit encoded data, but %s sequences\n", in_seq_type_name);
    }

    open_output_file();

    if (in_file_path != NULL && out_file_path != NULL)
    {
        if (fstat(fileno(IN), &input_stat) == 0) { have_input_stat = true; }
        else { err("can't obtain status of input file\n"); }
    }


    if (out_type == FORMAT_NAME)
    {
        fprintf(OUT, "%s sequences%s in NAF format version %d\n", in_seq_type_name, has_quality ? " with qualities" : "", format_version);
    }
    else if (out_type == PART_LIST) { print_list_of_parts(); }
    else
    {
        max_line_length = read_number(IN);
        if (line_length_is_specified) { max_line_length = requested_line_length; }
        N = read_number(IN);

        if (out_type == NUMBER_OF_SEQUENCES) { fprintf(OUT, "%llu\n", N); }
        else if (out_type == PART_SIZES) { print_part_sizes(); }
        else if (out_type == TITLE) { print_title(); }
        else if (N != 0)
        {
            skip_title();

            if (out_type == IDS) { print_ids(); }
            else if (out_type == NAMES) { print_names(); }
            else if (out_type == LENGTHS) { print_lengths(); }
            else if (out_type == TOTAL_LENGTH) { print_total_length(); }
            else if (out_type == MASK) { print_mask(); }
            else if (out_type == TOTAL_MASK_LENGTH) { print_total_mask_length(); }
            else if (out_type == FOUR_BIT) { print_4bit(); }
            else
            {
                dna_buffer_flush_size = ZSTD_DStreamOutSize() * 2;
                dna_buffer_size = dna_buffer_flush_size * 2 + 10;
                dna_buffer = (unsigned char *) malloc_or_die(dna_buffer_size);
                out_print_buffer_size = dna_buffer_size * 2;
                out_print_buffer = (unsigned char *) malloc_or_die(out_print_buffer_size);

                if (out_type == DNA) { print_dna(use_mask && has_mask); }
                else if (out_type == SEQ) { print_dna(use_mask && has_mask); }
                else if (out_type == MASKED_DNA) { print_dna(use_mask && has_mask); }
                else if (out_type == UNMASKED_DNA) { print_dna(0); }
                else if (out_type == CHARCOUNT) { print_charcount(use_mask && has_mask); }
                else if (out_type == SEQUENCES) { print_sequences(use_mask && has_mask); }
                else if (out_type == FASTA) { print_fasta(use_mask && has_mask); }
                else if (out_type == MASKED_FASTA) { print_fasta(use_mask && has_mask); }
                else if (out_type == UNMASKED_FASTA) { print_fasta(0); }
                else if (out_type == FASTQ)
                {
                    if (N > 0)
                    {
                        if (!has_quality) { die("FASTQ output requested, but input has no qualities\n"); }
                        print_fastq(0);
                    }
                }
                else { die("unknown output requested\n"); }
            }
        }
    }

    close_input_file();
    if (out_file_path != NULL && have_input_stat) { close_output_file_and_set_stat(); }
    else { close_output_file(); }

    success = true;
    return 0;
}
