/*
 * NAF compressor
 * Copyright (c) 2018-2020 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

#define VERSION "1.2.0"
#define DATE "2020-09-01"
#define COPYRIGHT_YEARS "2018-2020"

#include "platform.h"
#include "encoders.h"
#include "tables.c"

#define UNCOMPRESSED_BUFFER_SIZE (1ull * 1000 * 1000)
#define COMPRESSED_BUFFER_SIZE (2ull * 1000 * 1000)

static const unsigned char naf_magic_number[3] = { 0x01u, 0xF9u, 0xECu };

static bool verbose = false;
static bool binary_stderr = false;
static bool keep_temp_files = false;
static bool no_mask = false;

static char *in_file_path = NULL;
static FILE *IN = NULL;
static struct stat input_stat;
static bool have_input_stat = false;

static char *out_file_path = NULL;
static char *out_file_path_auto = NULL;
static FILE *OUT = NULL;
static bool force_stdout = false;
static bool created_output_file = false;

static int compression_level = 1;

static char *temp_dir = NULL;
static char *dataset_name = NULL;
static char *dataset_title = NULL;

static size_t temp_prefix_length = 0;
static size_t temp_path_length = 0;
static char *temp_prefix = NULL;

enum { in_format_unknown, in_format_fasta, in_format_fastq };
static int in_format_from_command_line = in_format_unknown;
static int in_format_from_input = in_format_unknown;
static int in_format_from_extension = in_format_unknown;

enum { seq_type_dna, seq_type_rna, seq_type_protein, seq_type_text };
static int in_seq_type = seq_type_dna;
static const char *in_seq_type_name = "DNA";

static bool store_title = false;
static bool store_mask  = true;
static bool store_qual  = false;

static bool parity = false;
static unsigned char* out_4bit_buffer = NULL;
static unsigned char* out_4bit_pos = NULL;

static unsigned long long seq_size_original  = 0ull;
static unsigned long long longest_line_length = 0ull;

static bool line_length_is_specified = false;
static unsigned long long requested_line_length = 0ull;

#define file_copy_buffer_size 131072
static unsigned char* file_copy_buffer = NULL;

#define length_units_buffer_n_units 4096
static unsigned int *length_units = NULL;
static unsigned int length_unit_index = 0;

#define mask_units_buffer_size 16384
static unsigned char *mask_units = NULL;
static unsigned char *mask_units_end = NULL;
static unsigned char *mask_units_pos = NULL;
static unsigned long long mask_len = 0;
static bool mask_on = false;

#define in_buffer_size 16384
static unsigned char *in_buffer = NULL;
static size_t in_begin = 0;
static size_t in_end = 0;

static unsigned long long n_sequences = 0ull;

static bool *is_unexpected_arr = is_unexpected_dna_arr;
static bool abort_on_unexpected_code = false;
static bool assume_well_formed_input = false;

static size_t out_4bit_buffer_size = 0;
static size_t zstd_stream_recommended_out_buffer_size = 0;

typedef struct {
    size_t allocated;
    size_t fill;
    unsigned long long uncompressed_size;
    unsigned long long compressed_size;
    unsigned long long written;
    ZSTD_CStream *cstream;
    FILE *file;
    char *path;
    unsigned char *buf;
} compressor_t;

compressor_t IDS  = { 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL };
compressor_t COMM = { 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL };
compressor_t LEN  = { 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL };
compressor_t MASK = { 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL };
compressor_t SEQ  = { 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL };
compressor_t QUAL = { 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL };

static bool success = false;


#include "utils.c"
#include "files.c"
#include "compressor.c"
#include "encoders.c"
#include "process.c"


#define FREE(p) \
do { if ((p) != NULL) { free(p); (p) = NULL; } } while (0)


static void done(void)
{
    compressor_done(&IDS);
    compressor_done(&COMM);
    compressor_done(&LEN);
    compressor_done(&MASK);
    compressor_done(&SEQ);
    compressor_done(&QUAL);

    FREE(name.data);
    FREE(comment.data);
    FREE(seq.data);
    FREE(qual.data);

    FREE(in_buffer);
    FREE(out_4bit_buffer);
    FREE(file_copy_buffer);
    FREE(length_units);
    FREE(mask_units);

    close_output_file();
    close_input_file();

    if (!success && created_output_file)
    {
        if (remove(out_file_path) != 0) { err("can't remove incomplete output file \"%s\"\n", out_file_path); }
    }

    FREE(out_file_path_auto);
    FREE(temp_prefix);
}


static void set_input_file_path(char *new_path)
{
    assert(new_path != NULL);

    if (in_file_path != NULL) { die("can compress only one file at a time\n"); }
    if (*new_path == '\0') { die("empty input file name\n"); }
    in_file_path = new_path;
}


static void set_output_file_path(char *new_path)
{
    assert(new_path != NULL);

    if (out_file_path != NULL) { die("double --out parameter\n"); }
    if (*new_path == '\0') { die("empty --out parameter\n"); }
    out_file_path = new_path;
}


static void set_temp_dir(char *new_temp_dir)
{
    assert(new_temp_dir != NULL);

    if (temp_dir != NULL) { die("double --temp-dir parameter\n"); }
    if (*new_temp_dir == '\0') { die("empty --temp-dir parameter\n"); }
    temp_dir = new_temp_dir;
}


static void set_dataset_name(char *new_name)
{
    assert(new_name != NULL);

    if (dataset_name != NULL) { die("double --name parameter\n"); }
    if (*new_name == '\0') { die("empty --name parameter\n"); }
    if (string_has_characters_unsafe_in_file_names(new_name)) { die("--name \"%s\" - contains characters unsafe in file names\n", new_name); }
    dataset_name = new_name;
}


static void set_dataset_title(char *new_title)
{
    assert(new_title != NULL);

    if (dataset_title != NULL) { die("double --title parameter\n"); }
    if (*new_title == '\0') { die("empty --title parameter\n"); }
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
    if (a < min_level || a > max_level || *end != '\0') { die("invalid value of --level, should be from %ld to %ld\n", min_level, max_level); }
    compression_level = (int)a;
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

    if (in_format_from_command_line != in_format_unknown) { die("input format specified more than once\n"); }
    in_format_from_command_line = parse_input_format(new_format);
    if (in_format_from_command_line == in_format_unknown) { die("unknown input format specified: \"%s\"\n", new_format); }
}


static void detect_input_format_from_input_file_extension(void)
{
    assert(in_format_from_extension == in_format_unknown);

    if (in_file_path != NULL)
    {
        char *ext = in_file_path + strlen(in_file_path);
        while (ext > in_file_path && *(ext-1) != '/' && *(ext-1) != '\\' && *(ext-1) != '.') { ext--; }
        if (ext > in_file_path && *(ext-1) == '.') { in_format_from_extension = parse_input_format(ext); }
    }
}


static void detect_temp_directory(void)
{
    if (temp_dir == NULL) { temp_dir = getenv("TMPDIR"); }
    if (temp_dir == NULL) { temp_dir = getenv("TMP"); }
    if (temp_dir == NULL)
    {
        die("temporary directory is not specified.\n"
            "Please either set TMPDIR or TMP environment variable, or add '--temp-dir DIR' to command line.\n");
    }
    if (verbose) { msg("Using temporary directory \"%s\"\n", temp_dir); }
}


static void show_version(void)
{
    msg("ennaf - NAF compressor, version " VERSION ", " DATE "\nCopyright (c) " COPYRIGHT_YEARS " Kirill Kryukov\n");
    if (verbose) { msg("Built with zstd " ZSTD_VERSION_STRING ", using runtime zstd %s\n", ZSTD_versionString()); }
}


static void show_help(void)
{
    int min_level = ZSTD_minCLevel();
    int max_level = ZSTD_maxCLevel();

    msg("Usage: ennaf [OPTIONS] [infile]\n"
        "Options:\n"
        "  -o FILE            - Write compressed output to FILE\n"
        "  -c                 - Write to standard output\n"
        "  -#, --level #      - Use compression level # (from %d to %d, default: 1)\n"
        "  --temp-dir DIR     - Use DIR as temporary directory\n"
        "  --name NAME        - Use NAME as prefix for temporary files\n"
        "  --title TITLE      - Store TITLE as dataset title\n"
        "  --fasta            - Input is in FASTA format\n"
        "  --fastq            - Input is in FASTQ format\n"
        "  --dna              - Input sequence is DNA (default)\n"
        "  --rna              - Input sequence is RNA\n"
        "  --protein          - Input sequence is protein\n"
        "  --text             - Input sequence is text\n"
        "  --strict           - Fail on unexpected input characters\n"
        "  --line-length N    - Override line length to N\n"
        "  --verbose          - Verbose mode\n"
        "  --keep-temp-files  - Keep temporary files\n"
        "  --no-mask          - Don't store mask\n"
        "  -h, --help         - Show help\n"
        "  -V, --version      - Show version\n",
        min_level, max_level);
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
                    if (!strcmp(argv[i], "--temp-dir")) { i++; set_temp_dir(argv[i]); continue; }
                    if (!strcmp(argv[i], "--name")) { i++; set_dataset_name(argv[i]); continue; }
                    if (!strcmp(argv[i], "--title")) { i++; set_dataset_title(argv[i]); continue; }
                    if (!strcmp(argv[i], "--level")) { i++; set_compression_level(argv[i]); continue; }
                    if (!strcmp(argv[i], "--line-length")) { i++; set_line_length(argv[i]); continue; }

                    // Deprecated, undocumented.
                    if (!strcmp(argv[i], "--out")) { i++; set_output_file_path(argv[i]); continue; }
                    if (!strcmp(argv[i], "--in")) { i++; set_input_file_path(argv[i]); continue; }
                    if (!strcmp(argv[i], "--in-format")) { i++; set_input_format_from_command_line(argv[i]); continue; }
                }
                if (!strcmp(argv[i], "--help")) { show_help(); exit(0); }
                if (!strcmp(argv[i], "--version")) { print_version = true; continue; }
                if (!strcmp(argv[i], "--verbose")) { verbose = true; continue; }
                if (!strcmp(argv[i], "--binary-stderr")) { if (!binary_stderr) { binary_stderr = true; change_stderr_to_binary(); } continue; }
                if (!strcmp(argv[i], "--keep-temp-files")) { keep_temp_files = true; continue; }
                if (!strcmp(argv[i], "--no-mask")) { no_mask = true; continue; }
                if (!strcmp(argv[i], "--fasta")) { set_input_format_from_command_line("fasta"); continue; }
                if (!strcmp(argv[i], "--fastq")) { set_input_format_from_command_line("fastq"); continue; }
                if (!strcmp(argv[i], "--dna")) { in_seq_type = seq_type_dna; continue; }
                if (!strcmp(argv[i], "--rna")) { in_seq_type = seq_type_rna; continue; }
                if (!strcmp(argv[i], "--protein")) { in_seq_type = seq_type_protein; continue; }
                if (!strcmp(argv[i], "--text")) { in_seq_type = seq_type_text; continue; }
                if (!strcmp(argv[i], "--well-formed")) { assume_well_formed_input = true; continue; }
                if (!strcmp(argv[i], "--strict")) { abort_on_unexpected_code = true; continue; }
            }

            if (i < argc - 1)
            {
                if (!strcmp(argv[i], "-o")) { i++; set_output_file_path(argv[i]); continue; }
            }

            if (!strcmp(argv[i], "-c")) { force_stdout = true; continue; }
            if (argv[i][1] >= '0' && argv[i][1] <= '9') { set_compression_level(argv[i]+1); continue; }
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
        die("'-c' and '-o' can't be used together\n");
    }

    if (assume_well_formed_input && abort_on_unexpected_code)
    {
        die("'--well-formed' and '--strict' can't be used together\n");
    }
}


int main(int argc, char **argv)
{
    atexit(done);
    init_encoders();

    parse_command_line(argc, argv);
    if (in_file_path == NULL && isatty(fileno(stdin)))
    {
        err("no input specified, use \"ennaf -h\" for help\n");
        exit(0);
    }

    if (no_mask || in_seq_type >= seq_type_protein) { store_mask = false; }

    if (in_seq_type == seq_type_dna)
    {
        is_unexpected_arr = is_unexpected_dna_arr;
        in_seq_type_name = "DNA";
        unexpected_seq_char_replacement = 'N';
    }
    if (in_seq_type == seq_type_rna)
    {
        is_unexpected_arr = is_unexpected_rna_arr;
        in_seq_type_name = "RNA";
        unexpected_seq_char_replacement = 'N';
    }
    else if (in_seq_type == seq_type_protein)
    {
        is_unexpected_arr = is_unexpected_protein_arr;
        in_seq_type_name = "protein";
        unexpected_seq_char_replacement = 'X';
    }
    else if (in_seq_type == seq_type_text)
    {
        is_unexpected_arr = is_unexpected_text_arr;
        in_seq_type_name = "text";
        unexpected_seq_char_replacement = '?';
    }

    detect_temp_directory();
    detect_input_format_from_input_file_extension();

    open_input_file();
    confirm_input_format();
    store_qual = (in_format_from_input == in_format_fastq);
    if (in_seq_type == seq_type_text && in_format_from_input == in_format_fasta) { is_unexpected_arr['>'] = true; }

    if (!force_stdout && out_file_path == NULL && isatty(fileno(stdout)))
    {
        if (in_file_path == NULL) { die("output file is not specified\n"); }
        else
        {
            size_t len = strlen(in_file_path) + 5;
            out_file_path_auto = (char *) malloc_or_die(len);
            snprintf(out_file_path_auto, len, "%s.naf", in_file_path);
            out_file_path = out_file_path_auto;
        }
    }

    open_output_file();
    if (in_file_path != NULL && out_file_path != NULL)
    {
        if (fstat(fileno(IN), &input_stat) == 0) { have_input_stat = true; }
        else { err("can't obtain status of input file\n"); }
    }

    make_temp_prefix();
    compressor_init(&IDS, "ids");
    compressor_init(&COMM, "comments");
    compressor_init(&LEN, "lengths");
    if (store_mask) { compressor_init(&MASK, "mask"); }
    compressor_init(&SEQ, "sequence");
    if (store_qual) { compressor_init(&QUAL, "quality"); }

    process();
    close_input_file();

    if (mask_len > 0) { add_mask(mask_len); }

    if (length_unit_index > 0)
    {
        compress(&LEN, length_units, sizeof(unsigned int) * length_unit_index);
        length_unit_index = 0;
    }

    if (mask_units_pos > mask_units)
    {
        compress(&MASK, mask_units, (size_t)(mask_units_pos - mask_units));
        mask_units_pos = mask_units;
    }

    if (parity) { out_4bit_pos++; }
    if (out_4bit_pos > out_4bit_buffer)
    {
        compress(&SEQ, out_4bit_buffer, (size_t)(out_4bit_pos - out_4bit_buffer));
    }

    compressor_end_stream(&IDS);
    compressor_end_stream(&COMM);
    compressor_end_stream(&LEN);
    compressor_end_stream(&MASK);
    compressor_end_stream(&SEQ);
    compressor_end_stream(&QUAL);

    fwrite_or_die(naf_magic_number, 1, 3, OUT);

    // In case of DNA input, write NAFv1 format.
    // Otherwise write NAFv2 where we can store sequence type.
    if (in_seq_type == seq_type_dna) { fputc_or_die(1, OUT); }
    else { fputc_or_die(2, OUT); fputc_or_die(in_seq_type, OUT); }

    fputc_or_die( (0           << 7) |   // extended format
                  (store_title << 6) |   // title
                  (1           << 5) |   // ids
                  (1           << 4) |   // comments
                  (1           << 3) |   // lengths
                  (store_mask  << 2) |   // mask
                  (1           << 1) |   // sequence
                   store_qual          , OUT);
    fputc_or_die(' ', OUT);

    unsigned long long out_line_length = line_length_is_specified ? requested_line_length : longest_line_length;
    if (verbose) { msg("Output line length: %llu\n", out_line_length); }
    write_variable_length_encoded_number(OUT, out_line_length);
    write_variable_length_encoded_number(OUT, n_sequences);

    if (store_title)
    {
        size_t title_length = strlen(dataset_title);
        write_variable_length_encoded_number(OUT, title_length);
        fwrite_or_die(dataset_title, 1, title_length, OUT);
    }

    write_variable_length_encoded_number(OUT, IDS.uncompressed_size);
    write_compressed_data(OUT, &IDS);

    write_variable_length_encoded_number(OUT, COMM.uncompressed_size);
    write_compressed_data(OUT, &COMM);

    write_variable_length_encoded_number(OUT, LEN.uncompressed_size);
    write_compressed_data(OUT, &LEN);

    if (store_mask)
    {
        write_variable_length_encoded_number(OUT, MASK.uncompressed_size);
        write_compressed_data(OUT, &MASK);
    }

    write_variable_length_encoded_number(OUT, seq_size_original);
    write_compressed_data(OUT, &SEQ);

    if (store_qual)
    {
        write_variable_length_encoded_number(OUT, QUAL.uncompressed_size);
        write_compressed_data(OUT, &QUAL);
    }

    if (out_file_path != NULL && have_input_stat) { close_output_file_and_set_stat(); }
    else { close_output_file(); }

    if (!assume_well_formed_input) { report_unexpected_input_char_stats(); }

    if (verbose) { msg("Processed %llu sequences\n", n_sequences); }
    success = true;

    return 0;
}
