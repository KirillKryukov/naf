/*
 * NAF compressor
 * Copyright (c) 2018-2021 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

static void open_input_file(void)
{
    assert(IN == NULL);
    assert(in_buffer == NULL);

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

    in_buffer = (unsigned char *) malloc_or_die(in_buffer_size);
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

    if (out_file_path != NULL && !force_stdout)
    {
        OUT = fopen(out_file_path, "wb");
        if (OUT == NULL) { die("can't create output file\n"); }
        created_output_file = true;
    }
    else
    {
#ifdef __MINGW32__
        if (_setmode(_fileno(stdout), O_BINARY) == -1) { die("can't set output stream to binary mode\n"); }
#else
        if (!freopen(NULL, "wb", stdout)) { die("can't set output stream to binary mode\n"); }
#endif
        OUT = stdout;
    }
}


static void close_input_file(void)
{
    if (IN != NULL && IN != stdin) { fclose(IN); IN = NULL; }
}


static void make_temp_prefix(void)
{
    assert(temp_dir != NULL);
    assert(temp_prefix == NULL);
    assert(temp_prefix_length == 0);

    if (dataset_name != NULL)
    {
        temp_prefix_length = strlen(dataset_name);
        temp_prefix = (char *) malloc_or_die(temp_prefix_length + 1);
        strcpy(temp_prefix, dataset_name);
    }
    else if (in_file_path != NULL)
    {
        char *in_file_name = in_file_path + strlen(in_file_path);
        while (in_file_name > in_file_path && *(in_file_name-1) != '/' && *(in_file_name-1) != '\\') { in_file_name--; }
        if (verbose) { fprintf(stderr, "Input file name: %s\n", in_file_name); }
        temp_prefix_length = strlen(in_file_name);
        temp_prefix = (char *) malloc_or_die(temp_prefix_length + 1);
        strcpy(temp_prefix, in_file_name);
    }
    else
    {
        long long pid = getpid();  // Some C std libs define pid_t as 'int', some as 'long long'.
        srand((unsigned)time(NULL));
        long r = rand() % 2147483648;
        temp_prefix = (char *) malloc_or_die(32);
        snprintf(temp_prefix, 32, "%lld-%ld", pid, r);
        temp_prefix_length = strlen(temp_prefix);
    }

    if (verbose) { msg("Temp file prefix: \"%s\"\n", temp_prefix); }

    temp_path_length = strlen(temp_dir) + temp_prefix_length + 11;
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
