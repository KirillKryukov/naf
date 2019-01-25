/*
 * NAF decompressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

enum { AHEAD_BUFFER_SIZE = 16384 };
static unsigned char ahead_buffer[AHEAD_BUFFER_SIZE];


static void skip_ahead(unsigned long long bytes)
{
    if (IN == stdin)
    {
        unsigned long long remaining = bytes;
        while (remaining > AHEAD_BUFFER_SIZE)
        {
            if (fread(ahead_buffer, 1, AHEAD_BUFFER_SIZE, IN) != AHEAD_BUFFER_SIZE) { incomplete(); }
            remaining -= AHEAD_BUFFER_SIZE;
        }
        if (fread(ahead_buffer, 1, remaining, IN) != remaining) { incomplete(); }
    }
    else
    {
        int error = fseek(IN, (long)bytes, SEEK_CUR);
        if (error) { incomplete(); }
    }
}


static void read_header(void)
{
    unsigned char first_bytes[3];
    if (fread(&first_bytes, 1, 3, IN) != 3) { incomplete(); }
    if (first_bytes[0] != 0x01 || first_bytes[1] != 0xF9 || first_bytes[2] != 0xEC) { fprintf(stderr, "Error: Not a NAF format\n"); exit(1); }

    format_version = fgetc_or_incomplete(IN);
    if (format_version > 2) { fprintf(stderr, "Error: Unknown version (%d) of NAF format\n", format_version); exit(1); }

    if (format_version > 1)
    {
        unsigned char t = fgetc_or_incomplete(IN);
        if (t == 1)
        {
            in_seq_type = seq_type_rna;
            in_seq_type_name = "RNA";
        }
        else if (t == 2)
        {
            in_seq_type = seq_type_protein;
            in_seq_type_name = "protein";
        }
        else if (t == 3)
        {
            in_seq_type = seq_type_text;
            in_seq_type_name = "text";
        }
        else { fprintf(stderr, "Error: Unknown sequence type (%d) recorded in NAF file\n", t); exit(1); }
    }

    unsigned char flags = fgetc_or_incomplete(IN);

    has_title   = (flags >> 6) & 1;
    has_ids     = (flags >> 5) & 1;
    has_names   = (flags >> 4) & 1;
    has_lengths = (flags >> 3) & 1;
    has_mask    = (flags >> 2) & 1;
    has_data    = (flags >> 1) & 1;
    has_quality =  flags       & 1;

    name_separator = fgetc_or_incomplete(IN);
}


static void skip_title(void)
{
    if (has_title)
    {
        unsigned long long title_size = read_number(IN);
        skip_ahead(title_size);
    }
}


static void skip_ids(void)
{
    if (has_ids)
    {
        read_number(IN);
        unsigned long long compressed_ids_size = read_number(IN);
        skip_ahead(compressed_ids_size);
    }
}


static void skip_names(void)
{
    if (has_names)
    {
        read_number(IN);
        unsigned long long compressed_names_size = read_number(IN);
        skip_ahead(compressed_names_size);
    }
}


static void skip_lengths(void)
{
    if (has_lengths)
    {
        read_number(IN);
        unsigned long long compressed_lengths_size = read_number(IN);
        skip_ahead(compressed_lengths_size);
    }
}


static void skip_mask(void)
{
    if (has_mask)
    {
        read_number(IN);
        unsigned long long compressed_mask_size = read_number(IN);
        skip_ahead(compressed_mask_size);
    }
}


/*static void skip_data(void)
{
    if (has_data)
    {
        read_number(IN);
        unsigned long long compressed_data_size = read_number(IN);
        skip_ahead(compressed_data_size);
    }
}*/


static void load_ids(void)
{
    unsigned long long ids_size = read_number(IN);
    unsigned long long compressed_ids_size = read_number(IN);

    ids_buffer = (char *)malloc(ids_size);
    if (!ids_buffer) { fprintf(stderr, "Can't allocate %llu bytes for ids\n", ids_size); exit(1); }
    compressed_ids_buffer = (unsigned char *)malloc(compressed_ids_size + 4);
    if (!compressed_ids_buffer) { fprintf(stderr, "Can't allocate %llu bytes for compressed ids\n", compressed_ids_size + 4); exit(1); }

    put_magic_number(compressed_ids_buffer);
    if (fread(compressed_ids_buffer + 4, 1, compressed_ids_size, IN) != compressed_ids_size) { incomplete(); }

    size_t n_dec_bytes = ZSTD_decompress( (void*)ids_buffer, ids_size, (void*)compressed_ids_buffer, compressed_ids_size + 4);
    if (n_dec_bytes != ids_size) { fprintf(stderr, "Can't decompress ids\n"); exit(1); }
    if (ids_buffer[ids_size-1] != 0) { fprintf(stderr, "Corrupted ids - not 0-terminated\n"); exit(1); }

    free(compressed_ids_buffer);
    compressed_ids_buffer = 0;

    ids = (char **)malloc(sizeof(char *) * N);
    if (!ids) { fprintf(stderr,"Can't allocate %llu bytes for table of ids\n", N * sizeof(char *)); exit(1); }

    ids[0] = ids_buffer;
    for (unsigned long long i = 1; i < N; i++)
    {
        char *ep = strchr(ids[i-1], 0);
        if (ep >= ids_buffer + ids_size - 1) { fprintf(stderr, "Currupted ids - can't read id %llu", i); exit(1); }
        ids[i] = ep + 1;
    }
}


static void load_names(void)
{
    unsigned long long names_size = read_number(IN);
    unsigned long long compressed_names_size = read_number(IN);

    names_buffer = (char *)malloc(names_size);
    if (!names_buffer) { fprintf(stderr, "Can't allocate %llu bytes for names\n", names_size); exit(1); }
    compressed_names_buffer = (unsigned char *)malloc(compressed_names_size + 4);
    if (!compressed_names_buffer) { fprintf(stderr, "Can't allocate %llu bytes for compressed names\n", compressed_names_size + 4); exit(1); }

    put_magic_number(compressed_names_buffer);
    if (fread(compressed_names_buffer + 4, 1, compressed_names_size, IN) != compressed_names_size) { incomplete(); }

    size_t n_dec_bytes = ZSTD_decompress( (void*)names_buffer, names_size, (void*)compressed_names_buffer, compressed_names_size + 4);
    if (n_dec_bytes != names_size) { fprintf(stderr, "Can't decompress names\n"); exit(1); }
    if (names_buffer[names_size-1] != 0) { fprintf(stderr, "Corrupted names - not 0-terminated\n"); exit(1); }

    free(compressed_names_buffer);
    compressed_names_buffer = 0;

    names = (char **)malloc(sizeof(char *) * N);
    if (!names) { fprintf(stderr,"Can't allocate %llu bytes for table of names\n", N * sizeof(char *)); exit(1); }

    names[0] = names_buffer;
    for (unsigned long long i = 1; i < N; i++)
    {
        char *ep = strchr(names[i-1], 0);
        if (ep >= names_buffer + names_size - 1) { fprintf(stderr, "Currupted names - can't read name %llu", i); exit(1); }
        names[i] = ep + 1;
    }
}


static void load_lengths(void)
{
    unsigned long long lengths_size = read_number(IN);
    unsigned long long compressed_lengths_size = read_number(IN);
    n_lengths = lengths_size / 4;

    lengths_buffer = (unsigned int *)malloc(lengths_size);
    if (!lengths_buffer) { fprintf(stderr, "Can't allocate %llu bytes for lengths\n", lengths_size); exit(1); }
    compressed_lengths_buffer = (unsigned char *)malloc(compressed_lengths_size + 4);
    if (!compressed_lengths_buffer) { fprintf(stderr, "Can't allocate %llu bytes for compressed lengths\n", compressed_lengths_size); exit(1); }

    put_magic_number(compressed_lengths_buffer);
    if (fread(compressed_lengths_buffer + 4, 1, compressed_lengths_size, IN) != compressed_lengths_size) { incomplete(); }

    size_t n_dec_bytes = ZSTD_decompress( (void*)lengths_buffer, lengths_size, (void*)compressed_lengths_buffer, compressed_lengths_size + 4);
    if (n_dec_bytes != lengths_size) { fprintf(stderr, "Can't decompress lengths\n"); exit(1); }

    free(compressed_lengths_buffer);
    compressed_lengths_buffer = 0;
}


static void load_mask(void)
{
    mask_size = read_number(IN);
    unsigned long long compressed_mask_size = read_number(IN);

    mask_buffer = (unsigned char *)malloc(mask_size);
    if (!mask_buffer) { fprintf(stderr, "Can't allocate %llu bytes for mask\n", mask_size); exit(1); }
    compressed_mask_buffer = (unsigned char *)malloc(compressed_mask_size + 4);
    if (!compressed_mask_buffer) { fprintf(stderr, "Can't allocate %llu bytes for compressed mask\n", compressed_mask_size); exit(1); }

    put_magic_number(compressed_mask_buffer);
    if (fread(compressed_mask_buffer + 4, 1, compressed_mask_size, IN) != compressed_mask_size) { incomplete(); }

    size_t n_dec_bytes = ZSTD_decompress( (void*)mask_buffer, mask_size, (void*)compressed_mask_buffer, compressed_mask_size + 4);
    if (n_dec_bytes != mask_size) { fprintf(stderr, "Can't decompress mask\n"); exit(1); }

    free(compressed_mask_buffer);
    compressed_mask_buffer = 0;

    if (mask_size > 0)
    {
        if (mask_buffer[0] == 0)
        {
            mask_on = 1;
            cur_mask = 1;
            if (mask_size < 2) { fprintf(stderr, "Corrupted mask\n"); exit(1); }
        }
        cur_mask_remaining = mask_buffer[cur_mask];
    }
}


static void load_compressed_sequence(void)
{
    total_seq_length = read_number(IN);
    compressed_seq_size = read_number(IN);

    compressed_seq_buffer = (unsigned char *)malloc(compressed_seq_size + 4);
    if (!compressed_seq_buffer) { fprintf(stderr, "Can't allocate %llu bytes for compressed sequence\n", compressed_seq_size); exit(1); }

    put_magic_number(compressed_seq_buffer);
    if (fread(compressed_seq_buffer + 4, 1, compressed_seq_size, IN) != compressed_seq_size) { incomplete(); }
}


static size_t initialize_input_decompression(void)
{
    in_buffer_size = ZSTD_DStreamInSize();
    in_buffer = (char *)malloc(in_buffer_size);
    if (!in_buffer) { fprintf(stderr, "Can't allocate %zu bytes for input buffer\n", in_buffer_size); exit(1); }

    out_buffer_size = ZSTD_DStreamOutSize();
    out_buffer = (char *)malloc(out_buffer_size);
    if (!out_buffer) { fprintf(stderr, "Can't allocate %zu bytes for input buffer\n", out_buffer_size); exit(1); }

    input_decompression_stream = ZSTD_createDStream();
    if (!input_decompression_stream) { fprintf(stderr, "Can't create input decompression stream\n"); exit(1); }

    size_t bytes_to_read = ZSTD_initDStream(input_decompression_stream);
    if (ZSTD_isError(bytes_to_read)) { fprintf(stderr, "Can't initialize input decompression stream: %s\n", ZSTD_getErrorName(bytes_to_read)); exit(1); }

    if (bytes_to_read < 5) { fprintf(stderr, "Can't initialize decompression\n"); exit(1); }

    put_magic_number((unsigned char *)in_buffer);

    size_t could_read = fread(in_buffer + 4, 1, bytes_to_read - 4, IN);
    if (could_read != bytes_to_read - 4) { incomplete(); }
    ZSTD_inBuffer in = { in_buffer, bytes_to_read, 0 };
    ZSTD_outBuffer out = { out_buffer, out_buffer_size, 0 };

    bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &in);

    if (ZSTD_isError(bytes_to_read)) { fprintf(stderr, "Can't decompress: %s\n", ZSTD_getErrorName(bytes_to_read)); exit(1); }
    if (in.pos != in.size) { fprintf(stderr, "Can't decompress first block\n"); exit(1); }
    if (out.pos != 0) { fprintf(stderr,"Can't decompress first block\n"); exit(1); }

    return bytes_to_read;
}


static void initialize_memory_decompression(void)
{
    mem_out_buffer_size = ZSTD_DStreamOutSize();
    mem_out_buffer = (unsigned char *)malloc(mem_out_buffer_size);
    if (!mem_out_buffer) { fprintf(stderr, "Can't allocate %zu bytes for input buffer\n", mem_out_buffer_size); exit(1); }

    memory_decompression_stream = ZSTD_createDStream();
    if (!memory_decompression_stream) { fprintf(stderr, "Can't create memory decompression stream\n"); exit(1); }

    memory_bytes_to_read = ZSTD_initDStream(memory_decompression_stream);
    if (ZSTD_isError(memory_bytes_to_read)) { fprintf(stderr, "Can't initialize memory decompression stream: %s\n", ZSTD_getErrorName(memory_bytes_to_read)); exit(1); }
}


static void initialize_quality_file_decompression(void)
{
    in_buffer_size = ZSTD_DStreamInSize();
    in_buffer = (char *)malloc(in_buffer_size);
    if (!in_buffer) { fprintf(stderr, "Can't allocate %zu bytes for input buffer\n", in_buffer_size); exit(1); }

    input_decompression_stream = ZSTD_createDStream();
    if (!input_decompression_stream) { fprintf(stderr, "Can't create input decompression stream\n"); exit(1); }

    file_bytes_to_read = ZSTD_initDStream(input_decompression_stream);
    if (ZSTD_isError(file_bytes_to_read)) { fprintf(stderr, "Can't initialize input decompression stream: %s\n", ZSTD_getErrorName(file_bytes_to_read)); exit(1); }

    if (file_bytes_to_read < 5) { fprintf(stderr, "Can't initialize decompression\n"); exit(1); }

    put_magic_number((unsigned char *)in_buffer);

    size_t could_read = fread(in_buffer + 4, 1, file_bytes_to_read - 4, IN);
    if (could_read != file_bytes_to_read - 4) { incomplete(); }

    zstd_file_in_buffer.src = in_buffer;
    zstd_file_in_buffer.size = file_bytes_to_read;
    zstd_file_in_buffer.pos = 0;
    ZSTD_outBuffer out = { quality_buffer, quality_buffer_size, 0 };

    file_bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &zstd_file_in_buffer);

    if (ZSTD_isError(file_bytes_to_read)) { fprintf(stderr, "Can't decompress first quality block: %s\n", ZSTD_getErrorName(file_bytes_to_read)); exit(1); }
    if (zstd_file_in_buffer.pos != zstd_file_in_buffer.size) { fprintf(stderr, "Can't decompress first block\n"); exit(1); }
    if (out.pos != 0) { fprintf(stderr,"Can't decompress first block\n"); exit(1); }

    quality_buffer_filling_pos = (unsigned)out.pos;
    quality_buffer_remaining = (unsigned)out.pos;
}


static inline size_t read_next_chunk(void* buffer, size_t size)
{
    size_t could_read = fread(buffer, 1, size, IN);
    if (could_read != size) { incomplete(); }
    return could_read;
}


static void refill_dna_buffer_from_memory(void)
{
    dna_buffer_filling_pos = 0;

    while ( dna_buffer_filling_pos < dna_buffer_flush_size &&
            (compressed_seq_pos < compressed_seq_size || zstd_mem_in_buffer.pos < zstd_mem_in_buffer.size) )
    {
        if (zstd_mem_in_buffer.pos >= zstd_mem_in_buffer.size)
        {
            zstd_mem_in_buffer.src = compressed_seq_buffer + compressed_seq_pos;
            zstd_mem_in_buffer.size = memory_bytes_to_read;
            zstd_mem_in_buffer.pos = 0;
            compressed_seq_pos += memory_bytes_to_read;
        }

        ZSTD_outBuffer out = { mem_out_buffer, mem_out_buffer_size, 0 };
        memory_bytes_to_read = ZSTD_decompressStream(memory_decompression_stream, &out, &zstd_mem_in_buffer);
        if (ZSTD_isError(memory_bytes_to_read)) { fprintf(stderr, "Can't decompress DNA from memory: %s\n", ZSTD_getErrorName(memory_bytes_to_read)); exit(1); }

        for (size_t i = 0; i < out.pos; i++)
        {
            dna_buffer[dna_buffer_filling_pos++] = code_to_nuc[mem_out_buffer[i] & 15];
            dna_buffer[dna_buffer_filling_pos++] = code_to_nuc[mem_out_buffer[i] >> 4];
        }
    }

    dna_buffer_remaining = dna_buffer_filling_pos;
    dna_buffer_printing_pos = 0;
}


static void refill_quality_buffer_from_file(void)
{
    quality_buffer_filling_pos = 0;

    while ( quality_buffer_filling_pos < quality_buffer_flush_size &&
            (file_bytes_to_read || zstd_file_in_buffer.pos < zstd_file_in_buffer.size) )
    {
        if (zstd_file_in_buffer.pos >= zstd_file_in_buffer.size)
        {
            size_t input_size = read_next_chunk(in_buffer, file_bytes_to_read);
            if (input_size != file_bytes_to_read) { incomplete(); }
            zstd_file_in_buffer.src = in_buffer;
            zstd_file_in_buffer.size = file_bytes_to_read;
            zstd_file_in_buffer.pos = 0;
        }

        ZSTD_outBuffer out = { quality_buffer + quality_buffer_filling_pos, quality_buffer_size - quality_buffer_filling_pos, 0 };
        file_bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &zstd_file_in_buffer);
        if (ZSTD_isError(file_bytes_to_read)) { fprintf(stderr, "Can't decompress quality: %s\n", ZSTD_getErrorName(file_bytes_to_read)); exit(1); }

        quality_buffer_filling_pos += (unsigned)out.pos;
    }

    quality_buffer_remaining = quality_buffer_filling_pos;
    quality_buffer_printing_pos = 0;
}
