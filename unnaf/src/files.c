/*
 * NAF decompressor
 * Copyright (c) 2018-2022 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

static void open_input_file(void)
{
    assert(IN == NULL);

    if (in_file_path == NULL)
    {
#ifdef __MINGW32__
        if (_setmode(_fileno(stdin), O_BINARY) == -1) { die("can't read input in binary mode\n"); }
#else
        if (!freopen(NULL, "rb", stdin)) { die("can't read input in binary mode\n"); }
#endif
        IN = stdin;
    }
    else
    {
        IN = fopen(in_file_path, "rb");
        if (IN == NULL) { die("can't open input file\n"); }
    }
}


static void change_stderr_to_binary(void)
{
#ifdef __MINGW32__
    if (_setmode(_fileno(stderr), O_BINARY) == -1) { die("can't set error stream to binary mode\n"); }
#else
    if (!freopen(NULL, "wb", stderr)) { die("can't set error stream to binary mode\n"); }
#endif
}


static void open_output_file(void)
{
    assert(OUT == NULL);
    assert(out_type != UNDECIDED);

    bool extracting_to_original_format = has_quality ? (out_type == FASTA) : (out_type == FASTQ);

    bool is_large_output = (out_type == IDS || out_type == NAMES || out_type == LENGTHS || out_type == MASK || out_type == FOUR_BIT ||
                            out_type == DNA || out_type == MASKED_DNA || out_type == UNMASKED_DNA || out_type == SEQ ||
                            out_type == FASTA || out_type == MASKED_FASTA || out_type == UNMASKED_FASTA || out_type == FASTQ);

    if (extracting_to_original_format && !force_stdout && in_file_path != NULL && out_file_path == NULL && isatty(fileno(stdout)))
    {
        size_t len = strlen(in_file_path);
        if (len > 4 && strcmp(in_file_path + len - 4, ".naf") == 0 &&
            in_file_path[len - 5] != '/' && in_file_path[len - 5] != '\\')
        {
            out_file_path_auto = (char *) malloc_or_die(len - 3);
            memcpy(out_file_path_auto, in_file_path, len - 4);
            out_file_path_auto[len - 4] = 0;
            out_file_path = out_file_path_auto;
        }
    }

    if (out_file_path != NULL && !force_stdout)
    {
        OUT = fopen(out_file_path, "wb");
        if (OUT == NULL) { die("can't create output file\n"); }
        created_output_file = true;
    }
    else
    {
        OUT = stdout;
    }

    if ( binary_stdout || (out_type == FOUR_BIT && force_stdout) )
    {
#ifdef __MINGW32__
        if (_setmode(_fileno(stdout), O_BINARY) == -1) { die("can't set output stream to binary mode\n"); }
#else
        if (!freopen(NULL, "wb", stdout)) { die("can't set output stream to binary mode\n"); }
#endif
    }

    if (is_large_output && !force_stdout && isatty(fileno(OUT)))
    {
        die("output file not specified - please either specify output file with '-o' or '>', or use '-c' option to force writing to console\n");
    }
}


static void close_input_file(void)
{
    if (IN != NULL && IN != stdin) { fclose(IN); IN = NULL; }
}


static void close_output_file(void)
{
    if (OUT == NULL) { return; }
    fclose_or_die(OUT);
    OUT = NULL;
}


static void close_output_file_and_set_stat(void)
{
    fflush_or_die(OUT);

#ifdef HAVE_CHMOD
    if (fchmod(fileno(OUT), input_stat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) != 0) { err("can't transfer permissions from input to output file\n"); }
#endif
#ifdef HAVE_CHOWN
    if (fchown(fileno(OUT), input_stat.st_uid, input_stat.st_gid) != 0) { err("can't transfer ownership from input to output file\n"); }
#endif

#if defined(HAVE_FUTIMENS)
    struct timespec input_timestamp[2];
    input_timestamp[0].tv_sec = A_TIME_SEC(input_stat);
    input_timestamp[1].tv_sec = M_TIME_SEC(input_stat);
    input_timestamp[0].tv_nsec = A_TIME_NSEC(input_stat);
    input_timestamp[1].tv_nsec = M_TIME_NSEC(input_stat);
    if (futimens(fileno(OUT), input_timestamp) != 0) { err("can't transfer timestamp from input to output file\n"); }
    //if (verbose) { msg("Changed output timestamp using futimens()\n"); }
#elif defined(HAVE_FUTIMES)
    struct timeval input_timestamp[2];
    input_timestamp[0].tv_sec = A_TIME_SEC(input_stat);
    input_timestamp[1].tv_sec = M_TIME_SEC(input_stat);
    input_timestamp[0].tv_usec = A_TIME_NSEC(input_stat) / 1000;
    input_timestamp[1].tv_usec = M_TIME_NSEC(input_stat) / 1000;
    if (futimes(fileno(OUT), input_timestamp) != 0) { err("can't transfer timestamp from input to output file\n"); }
    //if (verbose) { msg("Changed output timestamp using futimes()\n"); }
#elif defined(HAVE_UTIME)
#endif

    fclose_or_die(OUT);
    OUT = NULL;

#if defined(HAVE_FUTIMENS)
#elif defined(HAVE_FUTIMES)
#elif defined(HAVE_UTIME)
    struct utimbuf input_timestamp;
    input_timestamp.actime = A_TIME_SEC(input_stat);
    input_timestamp.modtime = M_TIME_SEC(input_stat);
    if (utime(out_file_path, &input_timestamp) != 0) { err("can't transfer timestamp from input to output file\n"); }
    //if (verbose) { msg("Changed output timestamp using utime()\n"); }
#endif
}
