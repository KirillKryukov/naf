/*
 * NAF decompressor
 * Copyright (c) 2018-2021 Kirill Kryukov
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

    size_t could_read = fread(&first_bytes, 1, 3, IN);
    if (could_read == 0) { die("empty input"); }
    else if (could_read != 3) { incomplete(); }

    if (first_bytes[0] != 0x01 || first_bytes[1] != 0xF9 || first_bytes[2] != 0xEC) { die("not a NAF format\n"); }

    format_version = fgetc_or_incomplete(IN);
    if (format_version < 1 || format_version > 2) { die("unknown version (%d) of NAF format\n", format_version); }

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
        else { die("unknown sequence type (%d) found in NAF file\n", t); }
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
    if (name_separator < 0x20 || name_separator > 0x7E) { die("unsupported name separator character\n"); }
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

    ids_buffer = (char *) malloc_or_die(ids_size);
    compressed_ids_buffer = (unsigned char *) malloc_or_die(compressed_ids_size + 4);
    put_magic_number(compressed_ids_buffer);
    if (fread(compressed_ids_buffer + 4, 1, compressed_ids_size, IN) != compressed_ids_size) { incomplete(); }

    size_t n_dec_bytes = ZSTD_decompress( (void*)ids_buffer, ids_size, (void*)compressed_ids_buffer, compressed_ids_size + 4);
    if (n_dec_bytes != ids_size) { die("can't decompress ids\n"); }
    if (ids_buffer[ids_size-1] != 0) { die("corrupted ids - not 0-terminated\n"); }

    free(compressed_ids_buffer);
    compressed_ids_buffer = 0;

    ids = (char **) malloc_or_die(sizeof(char *) * N);
    ids[0] = ids_buffer;
    for (unsigned long long i = 1; i < N; i++)
    {
        char *ep = strchr(ids[i-1], 0);
        if (ep >= ids_buffer + ids_size - 1) { die("currupted ids - can't read id %llu\n", i); }
        ids[i] = ep + 1;
    }
}


static void load_names(void)
{
    unsigned long long names_size = read_number(IN);
    unsigned long long compressed_names_size = read_number(IN);

    names_buffer = (char *) malloc_or_die(names_size);
    compressed_names_buffer = (unsigned char *) malloc_or_die(compressed_names_size + 4);
    put_magic_number(compressed_names_buffer);
    if (fread(compressed_names_buffer + 4, 1, compressed_names_size, IN) != compressed_names_size) { incomplete(); }

    size_t n_dec_bytes = ZSTD_decompress( (void*)names_buffer, names_size, (void*)compressed_names_buffer, compressed_names_size + 4);
    if (n_dec_bytes != names_size) { die("can't decompress names\n"); }
    if (names_buffer[names_size-1] != 0) { die("corrupted names - not 0-terminated\n"); }

    free(compressed_names_buffer);
    compressed_names_buffer = 0;

    names = (char **) malloc_or_die(sizeof(char *) * N);
    names[0] = names_buffer;
    for (unsigned long long i = 1; i < N; i++)
    {
        char *ep = strchr(names[i-1], 0);
        if (ep >= names_buffer + names_size - 1) { die("corrupted names - can't read name %llu\n", i); }
        names[i] = ep + 1;
    }
}


static void load_lengths(void)
{
    unsigned long long lengths_size = read_number(IN);
    unsigned long long compressed_lengths_size = read_number(IN);
    n_lengths = lengths_size / 4;

    lengths_buffer = (unsigned int *) malloc_or_die(lengths_size);
    compressed_lengths_buffer = (unsigned char *) malloc_or_die(compressed_lengths_size + 4);
    put_magic_number(compressed_lengths_buffer);
    if (fread(compressed_lengths_buffer + 4, 1, compressed_lengths_size, IN) != compressed_lengths_size) { incomplete(); }

    size_t n_dec_bytes = ZSTD_decompress( (void*)lengths_buffer, lengths_size, (void*)compressed_lengths_buffer, compressed_lengths_size + 4);
    if (n_dec_bytes != lengths_size) { die("can't decompress lengths\n"); }

    free(compressed_lengths_buffer);
    compressed_lengths_buffer = 0;
}


static void load_mask(void)
{
    mask_size = read_number(IN);
    unsigned long long compressed_mask_size = read_number(IN);

    mask_buffer = (unsigned char *) malloc_or_die(mask_size);
    compressed_mask_buffer = (unsigned char *) malloc_or_die(compressed_mask_size + 4);
    put_magic_number(compressed_mask_buffer);
    if (fread(compressed_mask_buffer + 4, 1, compressed_mask_size, IN) != compressed_mask_size) { incomplete(); }

    size_t n_dec_bytes = ZSTD_decompress( (void*)mask_buffer, mask_size, (void*)compressed_mask_buffer, compressed_mask_size + 4);
    if (n_dec_bytes != mask_size) { die("can't decompress mask\n"); }

    free(compressed_mask_buffer);
    compressed_mask_buffer = 0;

    if (mask_size > 0)
    {
        if (mask_buffer[0] == 0)
        {
            mask_on = 1;
            cur_mask = 1;
            if (mask_size < 2) { die("corrupted mask\n"); }
        }
        cur_mask_remaining = mask_buffer[cur_mask];
    }
}


static void load_compressed_sequence(void)
{
    total_seq_length = read_number(IN);
    compressed_seq_size = read_number(IN);

    compressed_seq_buffer = (unsigned char *) malloc_or_die(compressed_seq_size + 4);
    put_magic_number(compressed_seq_buffer);
    if (fread(compressed_seq_buffer + 4, 1, compressed_seq_size, IN) != compressed_seq_size) { incomplete(); }
}


static size_t initialize_input_decompression(void)
{
    in_buffer_size = ZSTD_DStreamInSize();
    in_buffer = (char *) malloc_or_die(in_buffer_size);

    out_buffer_size = ZSTD_DStreamOutSize();
    out_buffer = (char *) malloc_or_die(out_buffer_size);

    input_decompression_stream = ZSTD_createDStream();
    if (!input_decompression_stream) { die("can't create input decompression stream\n"); }

    ZSTD_TRY(ZSTD_DCtx_setParameter(input_decompression_stream, ZSTD_d_windowLogMax, ZSTD_WINDOWLOG_MAX));

    size_t bytes_to_read = ZSTD_initDStream(input_decompression_stream);
    if (ZSTD_isError(bytes_to_read)) { die("can't initialize input decompression stream: %s\n", ZSTD_getErrorName(bytes_to_read)); }

    if (bytes_to_read < 5) { die("can't initialize decompression\n"); }

    put_magic_number((unsigned char *)in_buffer);

    size_t could_read = fread(in_buffer + 4, 1, bytes_to_read - 4, IN);
    if (could_read != bytes_to_read - 4) { incomplete(); }
    ZSTD_inBuffer in = { in_buffer, bytes_to_read, 0 };
    ZSTD_outBuffer out = { out_buffer, out_buffer_size, 0 };

    bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &in);

    if (ZSTD_isError(bytes_to_read)) { die("can't decompress: %s\n", ZSTD_getErrorName(bytes_to_read)); }
    if (in.pos != in.size) { die("can't decompress first block\n"); }
    if (out.pos != 0) { die("can't decompress first block\n"); }

    return bytes_to_read;
}


static void initialize_memory_decompression(void)
{
    mem_out_buffer_size = ZSTD_DStreamOutSize();
    mem_out_buffer = (unsigned char *) malloc_or_die(mem_out_buffer_size);

    memory_decompression_stream = ZSTD_createDStream();
    if (!memory_decompression_stream) { die("can't create memory decompression stream\n"); }

    ZSTD_TRY(ZSTD_DCtx_setParameter(memory_decompression_stream, ZSTD_d_windowLogMax, ZSTD_WINDOWLOG_MAX));

    memory_bytes_to_read = ZSTD_initDStream(memory_decompression_stream);
    if (ZSTD_isError(memory_bytes_to_read)) { die("can't initialize memory decompression stream: %s\n", ZSTD_getErrorName(memory_bytes_to_read)); }
}


static void initialize_quality_file_decompression(void)
{
    in_buffer_size = ZSTD_DStreamInSize();
    in_buffer = (char *) malloc_or_die(in_buffer_size);

    input_decompression_stream = ZSTD_createDStream();
    if (!input_decompression_stream) { die("can't create input decompression stream\n"); }

    file_bytes_to_read = ZSTD_initDStream(input_decompression_stream);
    if (ZSTD_isError(file_bytes_to_read)) { die("can't initialize input decompression stream: %s\n", ZSTD_getErrorName(file_bytes_to_read)); }

    if (file_bytes_to_read < 5) { die("can't initialize decompression\n"); }

    put_magic_number((unsigned char *)in_buffer);

    size_t could_read = fread(in_buffer + 4, 1, file_bytes_to_read - 4, IN);
    if (could_read != file_bytes_to_read - 4) { incomplete(); }

    zstd_file_in_buffer.src = in_buffer;
    zstd_file_in_buffer.size = file_bytes_to_read;
    zstd_file_in_buffer.pos = 0;
    ZSTD_outBuffer out = { quality_buffer, quality_buffer_size, 0 };

    file_bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &zstd_file_in_buffer);

    if (ZSTD_isError(file_bytes_to_read)) { die("can't decompress first quality block: %s\n", ZSTD_getErrorName(file_bytes_to_read)); }
    if (zstd_file_in_buffer.pos != zstd_file_in_buffer.size) { die("can't decompress first block\n"); }
    if (out.pos != 0) { die("can't decompress first block\n"); }

    quality_buffer_filling_pos = (unsigned)out.pos;
    quality_buffer_remaining = (unsigned)out.pos;
}


static inline size_t read_next_chunk(void* buffer, size_t size)
{
    size_t could_read = fread(buffer, 1, size, IN);
    if (could_read != size) { incomplete(); }
    return could_read;
}


static void refill_dna_buffer_from_memory_4bit(void)
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
        if (ZSTD_isError(memory_bytes_to_read)) { die("can't decompress sequence from memory: %s\n", ZSTD_getErrorName(memory_bytes_to_read)); }

        for (size_t i = 0; i < out.pos; i++)
        {
            dna_buffer[dna_buffer_filling_pos++] = code_to_nuc[mem_out_buffer[i] & 15];
            dna_buffer[dna_buffer_filling_pos++] = code_to_nuc[mem_out_buffer[i] >> 4];
        }
    }

    dna_buffer_remaining = dna_buffer_filling_pos;
    dna_buffer_printing_pos = 0;
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

        ZSTD_outBuffer out = { dna_buffer + dna_buffer_filling_pos, dna_buffer_size - dna_buffer_filling_pos, 0 };
        memory_bytes_to_read = ZSTD_decompressStream(memory_decompression_stream, &out, &zstd_mem_in_buffer);
        if (ZSTD_isError(memory_bytes_to_read)) { die("can't decompress sequence from memory: %s\n", ZSTD_getErrorName(memory_bytes_to_read)); }
        dna_buffer_filling_pos += (unsigned)out.pos;
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
        if (ZSTD_isError(file_bytes_to_read)) { die("can't decompress quality: %s\n", ZSTD_getErrorName(file_bytes_to_read)); }

        quality_buffer_filling_pos += (unsigned)out.pos;
    }

    quality_buffer_remaining = quality_buffer_filling_pos;
    quality_buffer_printing_pos = 0;
}
