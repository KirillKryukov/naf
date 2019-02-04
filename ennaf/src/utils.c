/*
 * NAF compressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */


__attribute__ ((format (printf, 1, 2)))
static void msg(const char *format, ...) 
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
}


__attribute__ ((cold))
__attribute__ ((format (printf, 1, 2)))
static void warn(const char *format, ...) 
{
    fputs("ennaf warning: ", stderr);
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
}


__attribute__ ((cold))
__attribute__ ((format (printf, 1, 2)))
static void err(const char *format, ...) 
{
    fputs("ennaf error: ", stderr);
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
}


__attribute__ ((cold))
__attribute__ ((format (printf, 1, 2)))
__attribute__ ((noreturn))
static void die(const char *format, ...) 
{
    fputs("ennaf error: ", stderr);
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
    exit(1);
}


__attribute__ ((cold))
__attribute__ ((noreturn))
static void out_of_memory(const size_t size)
{
    die("can't allocate %" PRINT_SIZE_T " bytes\n", size);
}


static void* malloc_or_die(const size_t size)
{
    void *buf = malloc(size);
    if (buf == NULL) { out_of_memory(size); }
    return buf;
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
    if (elements_read != n_elements) { die("can't read from file\n"); }
}


static void fwrite_or_die(const void *ptr, size_t element_size, size_t n_elements, FILE *F)
{
    assert(ptr != NULL);
    assert(F != NULL);
    size_t elements_written = fwrite(ptr, element_size, n_elements, F);
    if (elements_written != n_elements) { die("can't write to file - disk full?\n"); }
}


static void fputc_or_die(int c, FILE *F)
{
    assert(F != NULL);
    if (fputc(c, F) != c) { die("can't write to file - disk full?\n"); }
}


static void fflush_or_die(FILE *F)
{
    assert(F != NULL);
    int error = fflush(F);
    if (error != 0) { die("can't write to file - disk full?\n"); }
}


static void fclose_or_die(FILE *F)
{
    assert(F != NULL);

    int error = fclose(F);
    if (error != 0) { die("can't close file - disk full?\n"); }
}


static FILE* create_temp_file(char *path, const char *purpose)
{
    assert(path != NULL);
    assert(purpose != NULL);

    FILE *F = fopen(path, "wb+");
    if (!F) { die("can't create temporary %s file \"%s\"\n", purpose, path); }
    return F;
}


static ZSTD_CStream* create_zstd_cstream(int level)
{
    ZSTD_CStream *s = ZSTD_createCStream();
    if (s == NULL) { die("ZSTD_createCStream() error\n"); }
    size_t const initResult = ZSTD_initCStream(s, level);
    if (ZSTD_isError(initResult)) { die("ZSTD_initCStream() error: %s\n", ZSTD_getErrorName(initResult)); }
    return s;
}


/*
 * Returns the number of bytes written to file.
 */
static size_t write_to_cstream(ZSTD_CStream *s, FILE *F, void *data, size_t size)
{
    assert(s != NULL);
    assert(F != NULL);
    assert(data != NULL);
    assert(out_buffer != NULL);
    assert(out_buffer_size != 0);

    ZSTD_inBuffer input = { data, size, 0 };
    size_t bytes_written = 0;
    while (input.pos < input.size)
    {
        ZSTD_outBuffer output = { out_buffer, out_buffer_size, 0 };
        size_t toRead = ZSTD_compressStream(s, &output, &input);
        if (ZSTD_isError(toRead)) { die("ZSTD_compressStream() error: %s\n", ZSTD_getErrorName(toRead)); }
        fwrite_or_die(out_buffer, 1, output.pos, F);
        bytes_written += output.pos;
    }
    return bytes_written;
}


/*
 * Returns the number of bytes written to file.
 */
static size_t flush_cstream(ZSTD_CStream *s, FILE *F)
{
    assert(s != NULL);
    assert(F != NULL);
    assert(out_buffer != NULL);
    assert(out_buffer_size != 0);

    ZSTD_outBuffer output = { out_buffer, out_buffer_size, 0 };
    size_t const remainingToFlush = ZSTD_endStream(s, &output);
    if (remainingToFlush) { die("can't end zstd stream\n"); }
    fwrite_or_die(out_buffer, 1, output.pos, F);
    return output.pos;
}
