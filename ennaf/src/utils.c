/*
 * NAF compressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

static_assert(sizeof(int) == 4, "Unsupported platform: int is not 4 bytes");
static_assert(sizeof(long) == 8, "Unsupported platform: long is not 8 bytes");
static_assert(sizeof(void*) == 8, "Unsupported platform: void* is not 8 bytes");
static_assert(sizeof(size_t) == 8, "Unsupported platform: size_t is not 8 bytes");
static_assert(sizeof(ptrdiff_t) == 8, "Unsupported platform: ptrdiff_t is not 8 bytes");
static_assert(sizeof(off_t) == 8, "Unsupported platform: off_t is not 8 bytes");
static_assert(sizeof(long long) == 8, "Unsupported platform: long long is not 8 bytes");


static bool is_eol_arr[256];
static bool is_space_arr[256];
static bool is_space_or_gt_arr[256];
static bool is_space_or_plus_arr[256];

static unsigned char nuc_code[256];

static size_t out_buffer_size = 0;
static void *out_buffer = NULL;


static void init_utils(void)
{
    assert(out_buffer == NULL);

    memset(is_eol_arr, 0, sizeof(is_eol_arr));
    memset(is_space_arr, 0, sizeof(is_space_arr));
    memset(is_space_or_gt_arr, 0, sizeof(is_space_or_gt_arr));
    memset(is_space_or_plus_arr, 0, sizeof(is_space_or_plus_arr));

    is_eol_arr['\n'] = true;
    is_eol_arr['\f'] = true;
    is_eol_arr['\r'] = true;

    is_space_arr['\t'] = true;
    is_space_arr['\n'] = true;
    is_space_arr['\v'] = true;
    is_space_arr['\f'] = true;
    is_space_arr['\r'] = true;
    is_space_arr[' '] = true;

    is_space_or_gt_arr['\t'] = true;
    is_space_or_gt_arr['\n'] = true;
    is_space_or_gt_arr['\v'] = true;
    is_space_or_gt_arr['\f'] = true;
    is_space_or_gt_arr['\r'] = true;
    is_space_or_gt_arr[' '] = true;
    is_space_or_gt_arr['>'] = true;

    is_space_or_plus_arr['\t'] = true;
    is_space_or_plus_arr['\n'] = true;
    is_space_or_plus_arr['\v'] = true;
    is_space_or_plus_arr['\f'] = true;
    is_space_or_plus_arr['\r'] = true;
    is_space_or_plus_arr[' '] = true;
    is_space_or_plus_arr['+'] = true;

    memset(nuc_code, 15, 256);

    nuc_code['A'] = 8;  nuc_code['a'] = 8;
    nuc_code['C'] = 4;  nuc_code['c'] = 4;
    nuc_code['G'] = 2;  nuc_code['g'] = 2;
    nuc_code['T'] = 1;  nuc_code['t'] = 1;
    nuc_code['U'] = 1;  nuc_code['u'] = 1;
    nuc_code['R'] = 10; nuc_code['r'] = 10;
    nuc_code['Y'] = 5;  nuc_code['y'] = 5;
    nuc_code['S'] = 6;  nuc_code['s'] = 6;
    nuc_code['W'] = 9;  nuc_code['w'] = 9;
    nuc_code['K'] = 3;  nuc_code['k'] = 3;
    nuc_code['M'] = 12; nuc_code['m'] = 12;
    nuc_code['B'] = 7;  nuc_code['b'] = 7;
    nuc_code['D'] = 11; nuc_code['d'] = 11;
    nuc_code['H'] = 13; nuc_code['h'] = 13;
    nuc_code['V'] = 14; nuc_code['v'] = 14;
    nuc_code['-'] = 0;

    out_buffer_size = ZSTD_CStreamOutSize();
    out_buffer = malloc(out_buffer_size);
}


static bool string_has_characters_unsafe_in_file_names(char *str)
{
    assert(str != NULL);

    for (char *c = str; *c; c++)
    {
        if (*c < 32 || *c == '\\' || *c == '/' || *c == ':' || *c == '*' || *c == '?' || *c == '"' || *c == '<' || *c == '>' || *c == '|')
        {
            return true;
        }
    }
    return false;
}


static void fread_or_die(void *ptr, size_t element_size, size_t n_elements, FILE *F)
{
    assert(ptr != NULL);
    assert(F != NULL);

    size_t elements_read = fread(ptr, element_size, n_elements, F);
    if (elements_read != n_elements) { fprintf(stderr, "Error reading from file\n"); exit(1); }
}
#define fread dont_use_fread


static void fwrite_or_die(const void *ptr, size_t element_size, size_t n_elements, FILE *F)
{
    assert(ptr != NULL);
    assert(F != NULL);

    size_t elements_written = fwrite(ptr, element_size, n_elements, F);
    if (elements_written != n_elements) { fprintf(stderr, "Error writing to file\n"); exit(1); }
}
#define fwrite dont_use_fwrite


static void fclose_or_die(FILE *F)
{
    assert(F != NULL);

    int error = fclose(F);
    if (error != 0) { fprintf(stderr, "Error: Can't write to file. Disk full?\n"); exit(1); }
}


static FILE* create_temp_file(char *path, char *purpose)
{
    assert(path != NULL);
    assert(purpose != NULL);

    FILE *F = fopen(path, "wb");
    if (!F) { fprintf(stderr, "Can't create temporary %s file \"%s\"\n", purpose, path); exit(1); }
    return F;
}


static ZSTD_CStream* create_zstd_cstream(int level)
{
    ZSTD_CStream *s = ZSTD_createCStream();
    if (s == NULL) { fprintf(stderr, "ZSTD_createCStream() error\n"); exit(1); }
    size_t const initResult = ZSTD_initCStream(s, level);
    if (ZSTD_isError(initResult)) { fprintf(stderr, "ZSTD_initCStream() error: %s\n", ZSTD_getErrorName(initResult)); exit(1); }
    return s;
}


static void write_to_cstream(ZSTD_CStream *s, FILE *F, void *data, size_t size)
{
    assert(s != NULL);
    assert(F != NULL);
    assert(data != NULL);
    assert(out_buffer != NULL);
    assert(out_buffer_size != 0);

    ZSTD_inBuffer input = { data, size, 0 };
    while (input.pos < input.size)
    {
        ZSTD_outBuffer output = { out_buffer, out_buffer_size, 0 };
        size_t toRead = ZSTD_compressStream(s, &output, &input);
        if (ZSTD_isError(toRead)) { fprintf(stderr, "ZSTD_compressStream() error: %s\n", ZSTD_getErrorName(toRead)); exit(1); }
        fwrite_or_die(out_buffer, 1, output.pos, F);
    }
}


static void flush_cstream(ZSTD_CStream *s, FILE *F)
{
    assert(s != NULL);
    assert(F != NULL);
    assert(out_buffer != NULL);
    assert(out_buffer_size != 0);

    ZSTD_outBuffer output = { out_buffer, out_buffer_size, 0 };
    size_t const remainingToFlush = ZSTD_endStream(s, &output);
    if (remainingToFlush) { fprintf(stderr, "not fully flushed"); exit(1); }
    fwrite_or_die(out_buffer, 1, output.pos, F);
}


static void write_variable_length_encoded_number(FILE *F, unsigned long long a)
{
    assert(F != NULL);

    unsigned char vle_buffer[10];
    unsigned char *b = vle_buffer + 10;
    *--b = (unsigned char)(a & 127ull);
    a >>= 7;
    while (a > 0)
    {
        *--b = (unsigned char)(128ull | (a & 127ull));
        a >>= 7;
    }
    size_t len = (size_t)(vle_buffer + 10 - b);
    fwrite_or_die(b, 1, len, F);
}