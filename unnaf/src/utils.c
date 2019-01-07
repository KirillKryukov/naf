/*
 * NAF decompressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

unsigned char code_to_nuc[16] = {'?','T','G','K','C','Y','S','B','A','W','R','D','M','H','V','N'};
//unsigned char nuc_to_code[256];
unsigned short codes_to_nucs[256];



__attribute__ ((noreturn))
static inline void incomplete(void)
{
    fprintf(stderr, "Incomplete or truncated input\n");
    exit(1);
}



static void check_platform(void)
{
    static char* unsupported = "Unsupported platform or compiler: ";

    if (sizeof(unsigned int) != 4) { fprintf(stderr,"%sunsigned int is not 4 bytes\n",unsupported); exit(1); }
    if (sizeof(void *) != 8) { fprintf(stderr,"%svoid* is not 8 bytes\n",unsupported); exit(1); }
    if (sizeof(size_t) != 8) { fprintf(stderr,"%ssize_t is not 8 bytes\n",unsupported); exit(1); }
    if (sizeof(ptrdiff_t) != 8) { fprintf(stderr,"%sptrdiff_t is not 8 bytes\n",unsupported); exit(1); }
    if (sizeof(long long) != 8) { fprintf(stderr,"%slong long is not 8 bytes\n",unsupported); exit(1); }
    //if (sizeof(__int128) != 16) { fprintf(stderr,"%s__int128 is not 16 bytes\n",unsupported); exit(1); }

    struct stat file_stat;
    if (sizeof(file_stat.st_size) != 8) { fprintf(stderr,"%sstat.st_size is not 8 bytes\n",unsupported); exit(1); }
}



static void init_tables(void)
{
    //memset(nuc_to_code, 0, 256);
    //for (unsigned i = 1; i < 16; i++) { nuc_to_code[code_to_nuc[i]] = i; }

    for (unsigned i = 0; i < 16; i++)
    {
        for (unsigned j = 0; j < 16; j++)
        {
            codes_to_nucs[(i << 4) | j] = (unsigned short) ( ((unsigned short)code_to_nuc[i] << 8) | code_to_nuc[j] );
        }
    }
}



static unsigned long long read_number(FILE *F)
{
    unsigned long long a = 0;
    unsigned char c;

    if (!fread(&c, 1, 1, F)) { incomplete(); }

    while (c & 128)
    {
        a = (a << 7) | (c & 127);
        if (!fread(&c, 1, 1, F)) { incomplete(); }
    }

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
