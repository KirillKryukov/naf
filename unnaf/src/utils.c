/*
 * NAF decompressor
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
static void err(const char *format, ...) 
{
    fputs("unnaf error: ", stderr);
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
    fputs("unnaf error: ", stderr);
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
static inline void incomplete(void)
{
    die("incomplete or truncated input\n");
}


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


static void init_tables(void)
{
    for (unsigned i = 0; i < 16; i++)
    {
        for (unsigned j = 0; j < 16; j++)
        {
            codes_to_nucs[(i << 4) | j] = (unsigned short) ( ((unsigned short)code_to_nuc[i] << 8) | code_to_nuc[j] );
        }
    }
}


static unsigned char fgetc_or_incomplete(FILE *F)
{
    assert(F != NULL);

    int c = fgetc(F);
    if (c == EOF) { incomplete(); }
    return (unsigned char)c;
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


/*
 * Reads a number in variable length encoding.
 */
static unsigned long long read_number(FILE *F)
{
    assert(F != NULL);

    static const char *overflow_msg = "invalid input: overflow reading a variable length encoded number\n";

    unsigned long long a = 0;
    unsigned char c;

    if (!fread(&c, 1, 1, F)) { incomplete(); }

    if (c == 128) { die("invalid input: error parsing variable length encoded number\n"); }

    while (c & 128)
    {
        if (a & (127ull << 57)) { fputs(overflow_msg, stderr); exit(1); }
        a = (a << 7) | (c & 127);
        if (!fread(&c, 1, 1, F)) { incomplete(); }
    }

    if (a & (127ull << 57)) { fputs(overflow_msg, stderr); exit(1); }
    a = (a << 7) | c;

    return a;
}


static inline void put_magic_number(unsigned char *buffer)
{
    buffer[0] = 0x28;
    buffer[1] = 0xB5;
    buffer[2] = 0x2F;
    buffer[3] = 0xFD;
}
