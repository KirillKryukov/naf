/*
 * NAF decompressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

static unsigned char code_to_nuc[16] = {'-','T','G','K','C','Y','S','B','A','W','R','D','M','H','V','N'};
static unsigned short codes_to_nucs[256];


__attribute__ ((noreturn))
static inline void incomplete(void)
{
    fprintf(stderr, "Incomplete or truncated input\n");
    exit(1);
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


/*
 * Reads a number in variable length encoding.
 */
static unsigned long long read_number(FILE *F)
{
    assert(F != NULL);

    static const char *overflow_msg = "Invalid input: overflow reading a variable length encoded number\n";

    unsigned long long a = 0;
    unsigned char c;

    if (!fread(&c, 1, 1, F)) { incomplete(); }

    if (c == 128) { fputs("Invalid input: error parsing variable length encoded number\n", stderr); exit(1); }

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
