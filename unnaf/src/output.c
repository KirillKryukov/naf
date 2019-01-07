/*
 * NAF decompressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

__attribute__ ((noreturn))
static void print_list_of_parts_and_exit(void)
{
    int printed = 0;
    if (has_title)   { printf("Title"); printed++; }
    if (has_ids)     { printf("%sIDs",     printed ? ", " : ""); printed++; }
    if (has_names)   { printf("%sNames",   printed ? ", " : ""); printed++; }
    if (has_lengths) { printf("%sLengths", printed ? ", " : ""); printed++; }
    if (has_mask)    { printf("%sMask",    printed ? ", " : ""); printed++; }
    if (has_data)    { printf("%sData",    printed ? ", " : ""); printed++; }
    if (has_quality) { printf("%sQuality", printed ? ", " : ""); printed++; }
    printf("\n");
    exit(0);
}



__attribute__ ((noreturn))
static void print_part_sizes_and_exit(void)
{
    if (has_title)
    {
        unsigned long long title_size = read_number(IN);
        printf("Title: %llu\n", title_size);
        skip_ahead(title_size);
    }

    if (has_ids)
    {
        unsigned long long ids_size = read_number(IN);
        unsigned long long compressed_ids_size = read_number(IN);
        printf("IDs: %llu / %llu (%.3f%%)\n", compressed_ids_size, ids_size, (double)compressed_ids_size / (double)ids_size * 100);
        skip_ahead(compressed_ids_size);
    }

    if (has_names)
    {
        unsigned long long names_size = read_number(IN);
        unsigned long long compressed_names_size = read_number(IN);
        printf("Names: %llu / %llu (%.3f%%)\n", compressed_names_size, names_size, (double)compressed_names_size / (double)names_size * 100);
        skip_ahead(compressed_names_size);
    }

    if (has_lengths)
    {
        unsigned long long lengths_size = read_number(IN);
        unsigned long long compressed_lengths_size = read_number(IN);
        printf("Lengths: %llu / %llu (%.3f%%)\n", compressed_lengths_size, lengths_size, (double)compressed_lengths_size / (double)lengths_size * 100);
        skip_ahead(compressed_lengths_size);
    }

    if (has_mask)
    {
        unsigned long long mask_size_1 = read_number(IN);
        unsigned long long compressed_mask_size = read_number(IN);
        printf("Mask: %llu / %llu (%.3f%%)\n", compressed_mask_size, mask_size_1, (double)compressed_mask_size / (double)mask_size_1 * 100);
        skip_ahead(compressed_mask_size);
    }

    if (has_data)
    {
        unsigned long long data_size = read_number(IN);
        compressed_seq_size = read_number(IN);
        printf("Data: %llu / %llu (%.3f%%)\n", compressed_seq_size, data_size, (double)compressed_seq_size / (double)data_size * 100);
        skip_ahead(compressed_seq_size);
    }

    if (has_quality)
    {
        unsigned long long quality_size = read_number(IN);
        compressed_quality_size = read_number(IN);
        printf("Quality: %llu / %llu (%.3f%%)\n", compressed_quality_size, quality_size, (double)compressed_quality_size / (double)quality_size * 100);
        skip_ahead(compressed_quality_size);
    }

    exit(0);
}



__attribute__ ((noreturn))
static void print_title_and_exit(void)
{
    if (has_title)
    {
        unsigned long long title_size = read_number(IN);
        char *title = (char *)malloc(title_size+1);
        if (fread(title, 1, title_size, IN) != title_size) { incomplete(); }
        title[title_size] = 0;
        printf("%s\n", title);
        free(title);
    }
    else
    {
        printf("\n");
    }

    exit(0);
}



__attribute__ ((noreturn))
static void print_ids_and_exit(void)
{
    if (has_ids)
    {
        load_ids();
        for (unsigned long long i = 0; i < N; i++) { printf("%s\n", ids[i]); }
    }

    exit(0);
}



static inline void print_name(unsigned long long index)
{
    if (has_ids && !has_names)
    {
        fputs(ids[index], stdout);
    }
    else if (!has_ids && has_names)
    {
        fputs(names[index], stdout);
    }
    else if (has_ids && has_names)
    {
        fputs(ids[index], stdout);
        if (names[index][0] != 0)
        {
            fputc(header[5], stdout); 
            fputs(names[index], stdout);
        }
    }
}



static inline void print_fasta_name(unsigned long long index)
{
    fputc('>', stdout);
    print_name(index);
    fputc('\n', stdout); 
}



static inline void print_fastq_name(unsigned long long index)
{
    fputc('@', stdout);
    print_name(index);
    fputc('\n', stdout); 
}



__attribute__ ((noreturn))
static void print_names_and_exit(void)
{
    if (has_ids || has_names)
    {
        if (has_ids) { load_ids(); }
        else { skip_ids(); }

        if (has_names) { load_names(); }

        if (has_ids && !has_names)
        {
            for (unsigned long long i = 0; i < N; i++) { printf("%s\n", ids[i]); }
        }
        else if (!has_ids && has_names)
        {
            for (unsigned long long i = 0; i < N; i++) { printf("%s\n", names[i]); }
        }
        else if (has_ids && has_names)
        {
            for (unsigned long long i = 0; i < N; i++)
            {
                fputs(ids[i], stdout);
                if (names[i][0] != 0)
                {
                    fputc(header[5], stdout); 
                    fputs(names[i], stdout);
                }
                fputc('\n', stdout); 
            }
        }
    }

    exit(0);
}



__attribute__ ((noreturn))
static void print_lengths_and_exit(void)
{
    if (has_lengths)
    {
        skip_ids();
        skip_names();
        load_lengths();

        for (unsigned long long i = 0; i < n_lengths; i++)
        {
            unsigned long long len = 0;
            while (i < n_lengths && lengths_buffer[i] == 4294967295u)
            { 
                len += 4294967295llu;
                i++;
            }
            if (i < n_lengths) { len += lengths_buffer[i]; }

            printf("%llu\n", len);
        }
    }

    exit(0);
}



__attribute__ ((noreturn))
static void print_total_length_and_exit(void)
{
    if (has_lengths)
    {
        skip_ids();
        skip_names();
        skip_lengths();
        skip_mask();

        total_seq_length = read_number(IN);

        printf("%llu\n", total_seq_length);
    }

    exit(0);
}



__attribute__ ((noreturn))
static void print_mask_and_exit(void)
{
    if (has_mask)
    {
        skip_ids();
        skip_names();
        skip_lengths();
        load_mask();

        for (unsigned long long i = 0; i < mask_size; i++)
        {
            unsigned long long len = 0;
            while (i < mask_size && mask_buffer[i] == 255u)
            { 
                len += 255llu;
                i++;
            }
            if (i < mask_size) { len += mask_buffer[i]; }

            printf("%llu\n", len);
        }
    }

    exit(0);
}



__attribute__ ((noreturn))
static void print_total_mask_length_and_exit(void)
{
    if (has_mask)
    {
        skip_ids();
        skip_names();
        skip_lengths();
        load_mask();

        unsigned long long total_mask_length = 0;

        for (unsigned long long i = 0; i < mask_size; i++)
        {
            total_mask_length += mask_buffer[i];
        }
        printf("%llu\n", total_mask_length);
    }
    else
    {
        printf("0\n");
    }

    exit(0);
}



__attribute__ ((noreturn))
static void print_4bit_and_exit(void)
{
    if (has_data)
    {
        skip_ids();
        skip_names();
        skip_lengths();
        skip_mask();

        read_number(IN);
        read_number(IN);

        if (!freopen(NULL, "wb", stdout)) { fprintf(stderr, "Can't change output to binary mode\n"); exit(1); }

        size_t bytes_to_read = initialize_input_decompression();
        size_t input_size;
        while ( (input_size = read_next_chunk(in_buffer, bytes_to_read)) )
        {
            ZSTD_inBuffer in = { in_buffer, input_size, 0 };
            while (in.pos < in.size)
            {
                ZSTD_outBuffer out = { out_buffer, out_buffer_size, 0 };
                bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &in);
                if (ZSTD_isError(bytes_to_read)) { fprintf(stderr, "Can't decompress: %s\n", ZSTD_getErrorName(bytes_to_read)); exit(1); }
                fwrite(out_buffer, 1, out.pos, stdout);
            }
        }
    }

    exit(0);
}



static inline void mask_dna_buffer(unsigned char *buffer, unsigned size)
{
    unsigned pos = 0;
    while (pos < size)
    {
        unsigned advance = cur_mask_remaining;
        if (advance > size - pos) { advance = size - pos; }
        unsigned end_pos = pos + advance;

        if (mask_on)
        {
            for (unsigned i = pos; i < end_pos; i++)
            {
                buffer[i] = (unsigned char)(buffer[i] + 32);
            }
        }

        cur_mask_remaining -= advance;
        if (cur_mask_remaining == 0)
        {
            if (mask_buffer[cur_mask] != 255) { mask_on = 1 - mask_on; }
            cur_mask++;
            cur_mask_remaining = mask_buffer[cur_mask];
        }

        pos = end_pos;
    }
}



static inline void print_dna_buffer(int masking)
{
    unsigned long long n_bp_to_print = dna_buffer_pos;
    if (n_bp_to_print > total_seq_n_bp_remaining) { n_bp_to_print = total_seq_n_bp_remaining; }

    if (masking) { mask_dna_buffer(dna_buffer, (unsigned)n_bp_to_print); }

    fwrite(dna_buffer, 1, n_bp_to_print, stdout);

    total_seq_n_bp_remaining -= n_bp_to_print;
    dna_buffer_pos = 0;
}



static inline void print_dna_split_into_lines(unsigned char *buffer, size_t size)
{
    //fwrite(buffer, 1, size, stdout);

    /*char *pos = buffer;
    size_t remaining_bp = size;
    while (remaining_bp > cur_line_n_bp_remaining)
    {
        fwrite(pos, 1, cur_line_n_bp_remaining, stdout);
        fputc('\n', stdout);
        pos += cur_line_n_bp_remaining;
        remaining_bp -= cur_line_n_bp_remaining;
        cur_line_n_bp_remaining = max_line_length;
    }

    fwrite(pos, 1, remaining_bp, stdout);
    cur_line_n_bp_remaining -= remaining_bp;*/


    /*char *pos = buffer;
    size_t remaining_bp = size;
    if (remaining_bp > cur_line_n_bp_remaining)
    {
        fwrite(pos, 1, cur_line_n_bp_remaining, stdout);
        fputc('\n', stdout);
        pos += cur_line_n_bp_remaining;
        remaining_bp -= cur_line_n_bp_remaining;
        cur_line_n_bp_remaining = max_line_length;
    }

    unsigned n_full_lines = (remaining_bp - 1) / max_line_length;
    //char *pos_end = pos + n_full_lines * max_line_length;
    for (unsigned i = 0; i < n_full_lines; i++)
    {
        fwrite(pos, 1, max_line_length, stdout);
        fputc('\n', stdout);
        pos += cur_line_n_bp_remaining;
    }

    remaining_bp -= max_line_length * n_full_lines;
    fwrite(pos, 1, remaining_bp, stdout);
    cur_line_n_bp_remaining = max_line_length - remaining_bp;*/


    unsigned char *print_pos = out_print_buffer;
    unsigned char *pos = buffer;
    size_t remaining_bp = size;
    while (remaining_bp > cur_line_n_bp_remaining)
    {
        memcpy(print_pos, pos, cur_line_n_bp_remaining);
        print_pos += cur_line_n_bp_remaining;
        *print_pos = '\n';
        print_pos++;
        pos += cur_line_n_bp_remaining;
        remaining_bp -= cur_line_n_bp_remaining;
        cur_line_n_bp_remaining = max_line_length;
    }

    memcpy(print_pos, pos, remaining_bp);
    print_pos += remaining_bp;
    cur_line_n_bp_remaining -= remaining_bp;

    fwrite(out_print_buffer, 1, (size_t)(print_pos - out_print_buffer), stdout);
}



static inline void print_dna_buffer_as_fasta(int masking)
{
    //printf("\n:: print_dna_buffer_as_fasta(), %llu bp remaining, buffer has %u bp\n", total_seq_n_bp_remaining, dna_buffer_pos);
    //printf("\n:: Current sequence: %llu, current length: %llu\n", cur_seq_index, cur_seq_len_index);

    unsigned long long n_bp_to_print = dna_buffer_pos;
    if (n_bp_to_print > total_seq_n_bp_remaining) { n_bp_to_print = total_seq_n_bp_remaining; }

    if (masking) { mask_dna_buffer(dna_buffer, (unsigned)n_bp_to_print); }

    unsigned char *pos = dna_buffer;

    while (n_bp_to_print >= cur_seq_len_n_bp_remaining)
    {
        if (cur_seq_len_n_bp_remaining > 0)
        {
            //printf("\n::     seq %llu, length %llu: printing %llu bp (remains %llu bp this call, %llu bp total)\n", cur_seq_index, cur_seq_len_index, cur_seq_len_n_bp_remaining, n_bp_to_print, total_seq_n_bp_remaining);

            //fwrite(pos, 1, cur_seq_len_n_bp_remaining, stdout);
            print_dna_split_into_lines(pos, cur_seq_len_n_bp_remaining);

            pos += cur_seq_len_n_bp_remaining;
            n_bp_to_print -= cur_seq_len_n_bp_remaining;
            total_seq_n_bp_remaining -= cur_seq_len_n_bp_remaining;
        }

        if (lengths_buffer[cur_seq_len_index] != 4294967295u)
        {
            fputc('\n', stdout);
            cur_seq_index++;
            if (cur_seq_index < N)
            {
                print_fasta_name(cur_seq_index);
                cur_line_n_bp_remaining = max_line_length;
            }
        }

        cur_seq_len_index++;
        if (cur_seq_len_index >= n_lengths) { break; }

        cur_seq_len_n_bp_remaining = lengths_buffer[cur_seq_len_index];
    }

    if (n_bp_to_print > 0)
    {
        //fwrite(pos, 1, n_bp_to_print, stdout);
        print_dna_split_into_lines(pos, n_bp_to_print);

        cur_seq_len_n_bp_remaining -= n_bp_to_print;
        total_seq_n_bp_remaining -= n_bp_to_print;
    }

    dna_buffer_pos = 0;
}



static inline void write_4bit_as_dna(unsigned char *buffer, size_t size, int masking)
{
    for (unsigned int i = 0; i < size; i++)
    {
        *(unsigned short *)(&dna_buffer[dna_buffer_pos]) = codes_to_nucs[buffer[i]];
        dna_buffer_pos += 2;
    }

    if (dna_buffer_pos > dna_buffer_flush_size) { print_dna_buffer(masking); }
}



static inline void write_4bit_as_fasta(unsigned char *buffer, size_t size, int masking)
{
    for (unsigned int i = 0; i < size; i++)
    {
        *(unsigned short *)(&dna_buffer[dna_buffer_pos]) = codes_to_nucs[buffer[i]];
        dna_buffer_pos += 2;
    }

    if (dna_buffer_pos > dna_buffer_flush_size) { print_dna_buffer_as_fasta(masking); }
}



__attribute__ ((noreturn))
static void print_dna_and_exit(int masking)
{
    if (has_data)
    {
        skip_ids();
        skip_names();
        skip_lengths();

        if (masking) { load_mask(); }
        else { skip_mask(); }

        total_seq_length = read_number(IN);
        compressed_seq_size = read_number(IN);
        total_seq_n_bp_remaining = total_seq_length;

        size_t bytes_to_read = initialize_input_decompression();
        size_t input_size;
        while ( (input_size = read_next_chunk(in_buffer, bytes_to_read)) )
        {
            ZSTD_inBuffer in = { in_buffer, input_size, 0 };
            while (in.pos < in.size)
            {
                ZSTD_outBuffer out = { out_buffer, out_buffer_size, 0 };
                bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &in);
                if (ZSTD_isError(bytes_to_read)) { fprintf(stderr, "Can't decompress DNA: %s\n", ZSTD_getErrorName(bytes_to_read)); exit(1); }
                write_4bit_as_dna((unsigned char *)out_buffer, out.pos, masking);
            }
        }

        if (total_seq_n_bp_remaining > 0) { print_dna_buffer(masking); }
    }

    exit(0);
}



__attribute__ ((noreturn))
static void print_fasta_and_exit(int masking)
{
    if (has_data)
    {
        load_ids();
        load_names();
        load_lengths();

        if (masking) { load_mask(); }
        else { skip_mask(); }

        total_seq_length = read_number(IN);
        compressed_seq_size = read_number(IN);
        total_seq_n_bp_remaining = total_seq_length;
        cur_seq_len_n_bp_remaining = lengths_buffer[0];

        print_fasta_name(0);
        cur_line_n_bp_remaining = max_line_length;

        size_t bytes_to_read = initialize_input_decompression();
        size_t input_size;
        while ( total_seq_n_bp_remaining > 0 && (input_size = read_next_chunk(in_buffer, bytes_to_read)) )
        {
            ZSTD_inBuffer in = { in_buffer, input_size, 0 };
            while (in.pos < in.size)
            {
                ZSTD_outBuffer out = { out_buffer, out_buffer_size, 0 };
                bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &in);
                if (ZSTD_isError(bytes_to_read)) { fprintf(stderr, "Can't decompress DNA: %s\n", ZSTD_getErrorName(bytes_to_read)); exit(1); }
                write_4bit_as_fasta((unsigned char *)out_buffer, out.pos, masking);
            }
        }

        if (total_seq_n_bp_remaining > 0) { print_dna_buffer_as_fasta(masking); }
    }

    exit(0);
}
