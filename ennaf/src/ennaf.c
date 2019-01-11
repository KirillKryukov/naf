/*
 * NAF compressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 *
 * Limitations:
 *   - Does not allow specifying name separator, always uses space.
 */

/*
  To do:
  Features:
    * Add support for name separators other than space.
  Quality:
    * Add error-checking wrappers for malloc/calloc/realloc.
*/

#define VERSION "1.0.0"
#define DATE "2019-01-11"
#define COPYRIGHT_YEARS "2018-2019"

#define NDEBUG

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

#include "utils.c"


static unsigned char naf_header_start[7] = "\x01\xF9\xEC\x01\x00\x20";

static bool verbose = false;
static bool keep_temp_files = false;

static char *in_file_path = NULL;
static FILE *IN = NULL;
static int in_fd = -1;

static char *out_file_path = NULL;
static FILE *OUT = NULL;
static int out_fd = -1;

static int compression_level = 1;

static char *temp_dir = NULL;
static char *dataset_name = NULL;
static char *dataset_title = NULL;

static size_t temp_prefix_length = 0;
static size_t temp_path_length = 0;
static char *temp_prefix = NULL;

static char *ids_path  = NULL;
static char *comm_path = NULL;
static char *len_path  = NULL;
static char *mask_path = NULL;
static char *seq_path  = NULL;
static char *qual_path = NULL;

static FILE* IDS  = NULL;
static FILE* COMM = NULL;
static FILE* LEN  = NULL;
static FILE* MASK = NULL;
static FILE* SEQ  = NULL;
static FILE* QUAL = NULL;

enum { in_format_unknown, in_format_fasta, in_format_fastq };
static int in_format_from_command_line = in_format_unknown;
static int in_format_from_input = in_format_unknown;
static int in_format_from_extension = in_format_unknown;

static bool extended_format = false;
static bool store_title = false;
static bool store_ids   = true;
static bool store_comm  = true;
static bool store_len   = true;
static bool store_mask  = true;
static bool store_seq   = true;
static bool store_qual  = false;

static ZSTD_CStream *ids_cstream  = NULL;
static ZSTD_CStream *comm_cstream = NULL;
static ZSTD_CStream *len_cstream  = NULL;
static ZSTD_CStream *mask_cstream = NULL;
static ZSTD_CStream *seq_cstream  = NULL;
static ZSTD_CStream *qual_cstream = NULL;

static bool parity = false;
static unsigned char* out_4bit_buffer = NULL;
static unsigned char* out_4bit_pos = NULL;

static unsigned long long ids_size_original  = 0ull;
static unsigned long long comm_size_original = 0ull;
static unsigned long long seq_size_original  = 0ull;
static unsigned long long qual_size_original = 0ull;
static unsigned long long longest_line_length = 0ull;

static bool line_length_is_specified = false;
static unsigned long long requested_line_length = 0ull;

#define file_copy_buffer_size 131072
static unsigned char* file_copy_buffer = NULL;

#define length_units_buffer_n_units 4096
static unsigned int *length_units = NULL;
static unsigned int length_unit_index = 0;
static size_t n_length_units_stored = 0;

#define mask_units_buffer_size 16384
static unsigned char *mask_units = NULL;
static unsigned char *mask_units_end = NULL;
static unsigned char *mask_units_pos = NULL;
static unsigned long long mask_len = 0;
static bool mask_on = false;
static unsigned long long n_mask_units_stored = 0;

#define in_buffer_size 16384
static unsigned char *in_buffer = NULL;
static size_t in_begin = 0;
static size_t in_end = 0;

static unsigned long long n_sequences = 0ull;


#include "files.c"
#include "encoders.c"
#include "process.c"


#define FREE(p) \
do { if ((p) != NULL) { free(p); (p) = NULL; } } while (0)


static void done(void)
{
    FREE(name.data);
    FREE(comment.data);
    FREE(seq.data);
    FREE(qual.data);

    FREE(in_buffer);
    FREE(out_buffer);
    FREE(out_4bit_buffer);
    FREE(file_copy_buffer);
    FREE(length_units);
    FREE(mask_units);

    if (OUT != NULL && OUT != stdout) { fclose_or_die(OUT); OUT = NULL; }
    close_input_file();
    close_temp_files();

    if (!keep_temp_files)
    {
        if (store_ids  && ids_path  != NULL) { remove_temp_file(ids_path ); }
        if (store_comm && comm_path != NULL) { remove_temp_file(comm_path); }
        if (store_len  && len_path  != NULL) { remove_temp_file(len_path ); }
        if (store_mask && mask_path != NULL) { remove_temp_file(mask_path); }
        if (store_seq  && seq_path  != NULL) { remove_temp_file(seq_path ); }
        if (store_qual && qual_path != NULL) { remove_temp_file(qual_path); }
    }

    FREE(temp_prefix);
    FREE(ids_path);
    FREE(comm_path);
    FREE(len_path);
    FREE(mask_path);
    FREE(seq_path);
    FREE(qual_path);
}


static void set_input_file_path(char *new_path)
{
    assert(new_path != NULL);

    if (in_file_path != NULL) { fprintf(stderr, "Error: double --in parameter\n"); exit(1); }
    if (*new_path == '\0') { fprintf(stderr, "Error: empty --in parameter\n"); exit(1); }
    in_file_path = new_path;
}


static void set_output_file_path(char *new_path)
{
    assert(new_path != NULL);

    if (out_file_path != NULL) { fprintf(stderr, "Error: double --out parameter\n"); exit(1); }
    if (*new_path == '\0') { fprintf(stderr, "Error: empty --out parameter\n"); exit(1); }
    out_file_path = new_path;
}


static void set_temp_dir(char *new_temp_dir)
{
    assert(new_temp_dir != NULL);

    if (temp_dir != NULL) { fprintf(stderr, "Error: double --temp-dir parameter\n"); exit(1); }
    if (*new_temp_dir == '\0') { fprintf(stderr, "Error: empty --temp-dir parameter\n"); exit(1); }
    temp_dir = new_temp_dir;
}


static void set_dataset_name(char *new_name)
{
    assert(new_name != NULL);

    if (dataset_name != NULL) { fprintf(stderr, "Error: double --name parameter\n"); exit(1); }
    if (*new_name == '\0') { fprintf(stderr, "Error: empty --name parameter\n"); exit(1); }
    if (string_has_characters_unsafe_in_file_names(new_name)) { fprintf(stderr, "Error: --name \"%s\" - contains characters unsafe in file names\n", new_name); exit(1); }
    dataset_name = new_name;
}


static void set_dataset_title(char *new_title)
{
    assert(new_title != NULL);

    if (dataset_title != NULL) { fprintf(stderr, "Error: double --title parameter\n"); exit(1); }
    if (*new_title == '\0') { fprintf(stderr, "Error: empty --title parameter\n"); exit(1); }
    dataset_title = new_title;
    store_title = 1;
}


static void set_compression_level(char *str)
{
    assert(str != NULL);

    char *end;
    long a = strtol(str, &end, 10);
    long min_level = ZSTD_minCLevel();
    long max_level = ZSTD_maxCLevel();
    if (a < min_level || a > max_level || *end != '\0')
    {
        fprintf(stderr, "Invalid value of --level, should be from %ld to %ld\n", min_level, max_level);
        exit(1);
    }
    compression_level = (int)a;
}


static void set_line_length(char *str)
{
    assert(str != NULL);

    char *end;
    long long a = strtoll(str, &end, 10);
    if (*end != '\0') { fprintf(stderr, "Can't parse the value of --line-length parameter\n"); exit(1); }
    if (a < 0ll) { fprintf(stderr, "Error: Negative line length specified\n"); exit(1); }

    char test_str[21];
    int nc = snprintf(test_str, 21, "%lld", a);
    if (nc < 1 || nc > 20 || strcmp(test_str, str) != 0) { fprintf(stderr, "Can't parse the value of --line-length parameter\n"); exit(1); }

    requested_line_length = (unsigned long long) a;
    line_length_is_specified = true;
}


static int parse_input_format(const char *str)
{
    assert(str != NULL);

    if (!strcasecmp(str, "fasta") || !strcasecmp(str, "fa") || !strcasecmp(str, "fna")) { return in_format_fasta; }
    if (!strcasecmp(str, "fastq") || !strcasecmp(str, "fq")) { return in_format_fastq; }
    return in_format_unknown;
}


static void set_input_format_from_command_line(const char *new_format)
{
    assert(new_format != NULL);

    if (in_format_from_command_line != in_format_unknown) { fprintf(stderr, "Error: Input format specified more than once\n"); exit(1); }
    in_format_from_command_line = parse_input_format(new_format);
    if (in_format_from_command_line == in_format_unknown) { fprintf(stderr, "Unknown input format specified: \"%s\"\n", new_format); exit(1); }
}


static void detect_input_format_from_input_file_extension(void)
{
    assert(in_format_from_extension == in_format_unknown);

    if (in_file_path != NULL)
    {
        char *ext = in_file_path + strlen(in_file_path);
        while (ext > in_file_path && *(ext-1) != '/' && *(ext-1) != '\\' && *(ext-1) != '.') { ext--; }
        in_format_from_extension = parse_input_format(ext);
    }
}


static void detect_temp_directory(void)
{
    if (temp_dir == NULL) { temp_dir = getenv("TMPDIR"); }
    if (temp_dir == NULL) { temp_dir = getenv("TMP"); }
    if (temp_dir == NULL)
    {
        fprintf(stderr, "Temp directory is not specified\n"
                "Please either set TMPDIR or TMP environment variable, or add '--temp-dir DIR' to command line.\n");
        exit(1);
    }
    if (verbose) { fprintf(stderr, "Using temporary directory \"%s\"\n", temp_dir); }
}


static void show_version(void)
{
    fprintf(stderr, "ennaf - NAF compressor, version " VERSION ", " DATE "\nCopyright (c) " COPYRIGHT_YEARS " Kirill Kryukov\n");
    if (verbose)
    {
        fprintf(stderr, "Built with zstd " ZSTD_VERSION_STRING ", using runtime zstd %s\n", ZSTD_versionString());
    }
}


static void show_help(void)
{
    int min_level = ZSTD_minCLevel();
    int max_level = ZSTD_maxCLevel();

    fprintf(stderr,
        "Usage: ennaf [OPTIONS] [<infile] [>outfile]\n"
        "Options:\n"
        "  --in FILE         - Compress FILE (stdin by default)\n"
        "  --out FILE        - Write compressed output to FILE (stdout by default)\n"
        "  --temp-dir DIR    - Use DIR as temporary directory\n"
        "  --name NAME       - Use NAME as prefix for temporary files\n"
        "  --title TITLE     - Store TITLE as dataset title\n"
        "  --level N         - Use compression level N (from %d to %d, default: 1)\n"
        "  --fasta           - Input is in FASTA format\n"
        "  --fastq           - Input is in FASTQ format\n"
        "  --line-length N   - Override line length to N\n"
        "  --verbose         - Verbose mode\n"
        "  --keep-temp-files - Keep temporary files\n"
        "  --no-mask         - Don't store mask\n"
        "  --help            - Show help\n"
        "  --version         - Show version\n",
        min_level, max_level);
}


static void parse_command_line(int argc, char **argv)
{
    bool print_version = false;

    for (int i = 1; i < argc; i++)
    {
        if (i < argc - 1)
        {
            if (!strcmp(argv[i], "--in")) { i++; set_input_file_path(argv[i]); continue; }
            if (!strcmp(argv[i], "--out")) { i++; set_output_file_path(argv[i]); continue; }
            if (!strcmp(argv[i], "--temp-dir")) { i++; set_temp_dir(argv[i]); continue; }
            if (!strcmp(argv[i], "--name")) { i++; set_dataset_name(argv[i]); continue; }
            if (!strcmp(argv[i], "--title")) { i++; set_dataset_title(argv[i]); continue; }
            if (!strcmp(argv[i], "--level")) { i++; set_compression_level(argv[i]); continue; }
            if (!strcmp(argv[i], "--in-format")) { i++; set_input_format_from_command_line(argv[i]); continue; }  // Undocumented
            if (!strcmp(argv[i], "--line-length")) { i++; set_line_length(argv[i]); continue; }
        }
        if (!strcmp(argv[i], "--help")) { show_help(); exit(0); }
        if (!strcmp(argv[i], "--version")) { print_version = true; continue; }
        if (!strcmp(argv[i], "--verbose")) { verbose = true; continue; }
        if (!strcmp(argv[i], "--keep-temp-files")) { keep_temp_files = true; continue; }
        if (!strcmp(argv[i], "--no-mask")) { store_mask = false; continue; }
        if (!strcmp(argv[i], "--fasta")) { set_input_format_from_command_line("fasta"); continue; }
        if (!strcmp(argv[i], "--fastq")) { set_input_format_from_command_line("fastq"); continue; }
        fprintf(stderr, "Unknown or incomplete parameter \"%s\"\n", argv[i]);
        exit(1);
    }

    if (print_version)
    {
        show_version();
        exit(0);
    }
}


int main(int argc, char **argv)
{
    if (argc <= 1 && isatty(fileno(stdin)))
    {
        fprintf(stderr, "No input specified, use \"ennaf --help\" for help\n");
        exit(0);
    }

    atexit(done);
    init_utils();
    init_encoders();

    parse_command_line(argc, argv);
    detect_temp_directory();
    detect_input_format_from_input_file_extension();

    open_input_file();
    confirm_input_format();
    store_qual = (in_format_from_input == in_format_fastq);

    make_temp_files();

    if (store_ids ) { ids_cstream  = create_zstd_cstream(compression_level); }
    if (store_comm) { comm_cstream = create_zstd_cstream(compression_level); }
    if (store_len ) { len_cstream  = create_zstd_cstream(compression_level); }
    if (store_mask) { mask_cstream = create_zstd_cstream(compression_level); }
    if (store_seq ) { seq_cstream  = create_zstd_cstream(compression_level); }
    if (store_qual) { qual_cstream = create_zstd_cstream(compression_level); }

    process();

    if (mask_len > 0)
    {
        add_mask(mask_len);
    }

    if (length_unit_index > 0)
    {
        write_to_cstream(len_cstream, LEN, length_units, sizeof(unsigned int) * length_unit_index);
        n_length_units_stored += length_unit_index;
        length_unit_index = 0;
    }

    if (mask_units_pos > mask_units)
    {
        write_to_cstream(mask_cstream, MASK, mask_units, (size_t)(mask_units_pos - mask_units));
        n_mask_units_stored += (unsigned long long)(mask_units_pos - mask_units);
        mask_units_pos = mask_units;
    }

    if (store_seq)
    {
        if (parity) { out_4bit_pos++; }
        if (out_4bit_pos > out_4bit_buffer) { write_to_cstream(seq_cstream, SEQ, out_4bit_buffer, (size_t)(out_4bit_pos - out_4bit_buffer) ); }
    }

    if (store_ids ) { flush_cstream(ids_cstream , IDS ); }
    if (store_comm) { flush_cstream(comm_cstream, COMM); }
    if (store_len ) { flush_cstream(len_cstream , LEN ); }
    if (store_mask) { flush_cstream(mask_cstream, MASK); }
    if (store_seq ) { flush_cstream(seq_cstream , SEQ ); }
    if (store_qual) { flush_cstream(qual_cstream, QUAL); }

    close_input_file();
    close_temp_files();


    open_output_file();
    naf_header_start[4] = (unsigned char)( (extended_format << 7) |
                                           (store_title     << 6) |
                                           (store_ids       << 5) |
                                           (store_comm      << 4) |
                                           (store_len       << 3) |
                                           (store_mask      << 2) |
                                           (store_seq       << 1) |
                                            store_qual              );

    fwrite_or_die(naf_header_start, 1, 6, OUT);



    unsigned long long out_line_length = line_length_is_specified ? requested_line_length : longest_line_length;
    if (verbose) { fprintf(stderr, "Output line length: %llu\n", out_line_length); }
    write_variable_length_encoded_number(OUT, out_line_length);
    write_variable_length_encoded_number(OUT, n_sequences);

    if (store_title)
    {
        size_t title_length = strlen(dataset_title);
        write_variable_length_encoded_number(OUT, title_length);
        fwrite_or_die(dataset_title, 1, title_length, OUT);
    }

    if (store_ids)
    {
        write_variable_length_encoded_number(OUT, ids_size_original);
        copy_file_to_out(ids_path);
    }

    if (store_comm)
    {
        write_variable_length_encoded_number(OUT, comm_size_original);
        copy_file_to_out(comm_path);
    }

    if (store_len)
    {
        write_variable_length_encoded_number(OUT, sizeof(unsigned int) * n_length_units_stored);
        copy_file_to_out(len_path);
    }

    if (store_mask)
    {
        write_variable_length_encoded_number(OUT, n_mask_units_stored);
        copy_file_to_out(mask_path);
    }

    if (store_seq)
    {
        write_variable_length_encoded_number(OUT, seq_size_original);
        copy_file_to_out(seq_path);
    }

    if (store_qual)
    {
        write_variable_length_encoded_number(OUT, qual_size_original);
        copy_file_to_out(qual_path);
    }

    if (verbose) { fprintf(stderr, "Processed %llu sequences\n", n_sequences); }

    return 0;
}
