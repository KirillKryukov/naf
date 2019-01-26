/*
 * NAF compressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

static void open_input_file(void)
{
    assert(IN == NULL);
    assert(in_buffer == NULL);

    if (in_file_path == NULL)
    {
        if (!freopen(NULL, "rb", stdin)) { die("Can't read input in binary mode\n"); }
        IN = stdin;
    }
    else
    {
        IN = fopen(in_file_path, "rb");
        if (IN == NULL) { die("Can't open input file\n"); }
    }

    in_buffer = (unsigned char *) malloc(in_buffer_size);
}


static void open_output_file(void)
{
    assert(OUT == NULL);

    if (out_file_path != NULL && !force_stdout)
    {
        OUT = fopen(out_file_path, "wb");
        if (OUT == NULL) { die("Can't create output file\n"); }
        created_output_file = true;
    }
    else
    {
        if (!freopen(NULL, "wb", stdout)) { die("Can't set output stream to binary mode\n"); }
        OUT = stdout;
    }
}


static void remove_temp_file(char * const path)
{
    assert(path != NULL);

    if (access(path, F_OK) != 0) { return; }
    if (remove(path) != 0) { err("Error removing temporary file \"%s\"\n", path); }
}


static void close_input_file(void)
{
    if (IN != NULL && IN != stdin) { fclose(IN); IN = NULL; }
}


static void close_temp_files(void)
{
    if (IDS  != NULL) { fclose_or_die(IDS ); IDS  = NULL; }
    if (COMM != NULL) { fclose_or_die(COMM); COMM = NULL; }
    if (LEN  != NULL) { fclose_or_die(LEN ); LEN  = NULL; }
    if (MASK != NULL) { fclose_or_die(MASK); MASK = NULL; }
    if (SEQ  != NULL) { fclose_or_die(SEQ ); SEQ  = NULL; }
    if (QUAL != NULL) { fclose_or_die(QUAL); QUAL = NULL; }
}


static void make_temp_files(void)
{
    assert(temp_dir != NULL);
    assert(temp_prefix == NULL);
    assert(temp_prefix_length == 0);

    if (dataset_name != NULL)
    {
        temp_prefix_length = strlen(dataset_name);
        temp_prefix = (char*)malloc(temp_prefix_length + 1);
        strcpy(temp_prefix, dataset_name);
    }
    else if (in_file_path != NULL)
    {
        char *in_file_name = in_file_path + strlen(in_file_path);
        while (in_file_name > in_file_path && *(in_file_name-1) != '/' && *(in_file_name-1) != '\\') { in_file_name--; }
        if (verbose) { fprintf(stderr, "Input file name: %s\n", in_file_name); }
        temp_prefix_length = strlen(in_file_name);
        temp_prefix = (char*)malloc(temp_prefix_length + 1);
        strcpy(temp_prefix, in_file_name);
    }
    else
    {
        long long pid = getpid();  // Some C std libs define pid_t as 'int', some as 'long long'.
        srand((unsigned)time(NULL));
        long r = rand() % 2147483648;
        temp_prefix = (char*)malloc(32);
        snprintf(temp_prefix, 32, "%lld-%ld", pid, r);
        temp_prefix_length = strlen(temp_prefix);
    }

    if (verbose) { msg("Temp file prefix: \"%s\"\n", temp_prefix); }

    temp_path_length = strlen(temp_dir) + temp_prefix_length + 11;

    if (store_ids ) { ids_path  = (char*)malloc(temp_path_length); }
    if (store_comm) { comm_path = (char*)malloc(temp_path_length); }
    if (store_len ) { len_path  = (char*)malloc(temp_path_length); }
    if (store_mask) { mask_path = (char*)malloc(temp_path_length); }
    if (store_seq ) { seq_path  = (char*)malloc(temp_path_length); }
    if (store_qual) { qual_path = (char*)malloc(temp_path_length); }

    if (store_ids ) { snprintf(ids_path , temp_path_length, "%s/%s.ids",      temp_dir, temp_prefix); }
    if (store_comm) { snprintf(comm_path, temp_path_length, "%s/%s.comments", temp_dir, temp_prefix); }
    if (store_len ) { snprintf(len_path , temp_path_length, "%s/%s.lengths",  temp_dir, temp_prefix); }
    if (store_mask) { snprintf(mask_path, temp_path_length, "%s/%s.mask",     temp_dir, temp_prefix); }
    if (store_seq ) { snprintf(seq_path , temp_path_length, "%s/%s.sequence", temp_dir, temp_prefix); }
    if (store_qual) { snprintf(qual_path, temp_path_length, "%s/%s.quality",  temp_dir, temp_prefix); }

    if (verbose)
    {
        if (store_ids ) { msg("Temp ids file     : \"%s\"\n", ids_path ); }
        if (store_comm) { msg("Temp names file   : \"%s\"\n", comm_path); }
        if (store_len ) { msg("Temp lengths file : \"%s\"\n", len_path ); }
        if (store_mask) { msg("Temp mask file    : \"%s\"\n", mask_path); }
        if (store_seq ) { msg("Temp sequence file: \"%s\"\n", seq_path ); }
        if (store_qual) { msg("Temp quality file : \"%s\"\n", qual_path); }
    }

    if (store_ids ) { IDS  = create_temp_file(ids_path , "ids"     ); }
    if (store_comm) { COMM = create_temp_file(comm_path, "comments"); }
    if (store_len ) { LEN  = create_temp_file(len_path , "lengths" ); }
    if (store_mask) { MASK = create_temp_file(mask_path, "mask"    ); }
    if (store_seq ) { SEQ  = create_temp_file(seq_path , "sequence"); }
    if (store_qual) { QUAL = create_temp_file(qual_path, "quality" ); }
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
    if (fchmod(fileno(OUT), input_stat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) != 0) { err("Can't transfer permissions from input to output file\n"); }
#endif
#ifdef HAVE_CHOWN
    if (fchown(fileno(OUT), input_stat.st_uid, input_stat.st_gid) != 0) { err("Can't transfer ownership from input to output file\n"); }
#endif

#if defined(HAVE_FUTIMENS)
    struct timespec input_timestamp[2];
    input_timestamp[0].tv_sec = A_TIME_SEC(input_stat);
    input_timestamp[1].tv_sec = M_TIME_SEC(input_stat);
    input_timestamp[0].tv_nsec = A_TIME_NSEC(input_stat);
    input_timestamp[1].tv_nsec = M_TIME_NSEC(input_stat);
    if (futimens(fileno(OUT), input_timestamp) != 0) { err("Can't transfer timestamp from input to output file\n"); }
    //if (verbose) { msg("Changed output timestamp using futimens()\n"); }
#elif defined(HAVE_FUTIMES)
    struct timeval input_timestamp[2];
    input_timestamp[0].tv_sec = A_TIME_SEC(input_stat);
    input_timestamp[1].tv_sec = M_TIME_SEC(input_stat);
    input_timestamp[0].tv_usec = A_TIME_NSEC(input_stat) / 1000;
    input_timestamp[1].tv_usec = M_TIME_NSEC(input_stat) / 1000;
    if (futimes(fileno(OUT), input_timestamp) != 0) { err("Can't transfer timestamp from input to output file\n"); }
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
    if (utime(out_file_path, &input_timestamp) != 0) { err("Can't transfer timestamp from input to output file\n"); }
    //if (verbose) { msg("Changed output timestamp using utime()\n"); }
#endif
}
