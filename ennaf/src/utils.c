/*
 * NAF compressor
 * Copyright (c) 2018-2021 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */


//__attribute__ ((format (printf, 1, 2)))
static void msg(const char *format, ...) 
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
}


__attribute__ ((cold))
//__attribute__ ((format (printf, 1, 2)))
static void warn(const char *format, ...) 
{
    fputs("ennaf warning: ", stderr);
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
}


__attribute__ ((cold))
//__attribute__ ((format (printf, 1, 2)))
static void err(const char *format, ...) 
{
    fputs("ennaf error: ", stderr);
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
}


__attribute__ ((cold))
//__attribute__ ((format (printf, 1, 2)))
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


#define ZSTD_TRY(f)  \
do {                 \
    size_t e = f;    \
    if (ZSTD_isError(e)) { die("zstd error: %s", ZSTD_getErrorName(e)); }  \
} while (0)


__attribute__ ((cold))
__attribute__ ((noreturn))
static void out_of_memory(const size_t size)
{
    die("can't allocate %zu bytes\n", size);
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
