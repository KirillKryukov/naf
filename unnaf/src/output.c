/*
 * NAF decompressor
 * Copyright (c) 2018-2021 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

static void print_list_of_parts(void)
{
    int printed = 0;
    if (has_title)   { fprintf(OUT, "Title"); printed++; }
    if (has_ids)     { fprintf(OUT, "%sIDs",     printed ? ", " : ""); printed++; }
    if (has_names)   { fprintf(OUT, "%sNames",   printed ? ", " : ""); printed++; }
    if (has_lengths) { fprintf(OUT, "%sLengths", printed ? ", " : ""); printed++; }
    if (has_mask)    { fprintf(OUT, "%sMask",    printed ? ", " : ""); printed++; }
    if (has_data)    { fprintf(OUT, "%sData",    printed ? ", " : ""); printed++; }
    if (has_quality) { fprintf(OUT, "%sQuality", printed ? ", " : ""); printed++; }
    fprintf(OUT, "\n");
}


static void print_part_sizes(void)
{
    if (has_title)
    {
        unsigned long long title_size = read_number(IN);
        fprintf(OUT, "Title: %llu\n", title_size);
        skip_ahead(title_size);
    }

    if (has_ids)
    {
        unsigned long long ids_size = read_number(IN);
        unsigned long long compressed_ids_size = read_number(IN);
        fprintf(OUT, "IDs: %llu / %llu (%.3f%%)\n", compressed_ids_size, ids_size, (double)compressed_ids_size / (double)ids_size * 100);
        skip_ahead(compressed_ids_size);
    }

    if (has_names)
    {
        unsigned long long names_size = read_number(IN);
        unsigned long long compressed_names_size = read_number(IN);
        fprintf(OUT, "Names: %llu / %llu (%.3f%%)\n", compressed_names_size, names_size, (double)compressed_names_size / (double)names_size * 100);
        skip_ahead(compressed_names_size);
    }

    if (has_lengths)
    {
        unsigned long long lengths_size = read_number(IN);
        unsigned long long compressed_lengths_size = read_number(IN);
        fprintf(OUT, "Lengths: %llu / %llu (%.3f%%)\n", compressed_lengths_size, lengths_size, (double)compressed_lengths_size / (double)lengths_size * 100);
        skip_ahead(compressed_lengths_size);
    }

    if (has_mask)
    {
        unsigned long long mask_size_1 = read_number(IN);
        unsigned long long compressed_mask_size = read_number(IN);
        fprintf(OUT, "Mask: %llu / %llu (%.3f%%)\n", compressed_mask_size, mask_size_1, (double)compressed_mask_size / (double)mask_size_1 * 100);
        skip_ahead(compressed_mask_size);
    }

    if (has_data)
    {
        unsigned long long data_size = read_number(IN);
        compressed_seq_size = read_number(IN);
        fprintf(OUT, "Data: %llu / %llu (%.3f%%)\n", compressed_seq_size, data_size, (double)compressed_seq_size / (double)data_size * 100);
        skip_ahead(compressed_seq_size);
    }

    if (has_quality)
    {
        unsigned long long quality_size = read_number(IN);
        compressed_quality_size = read_number(IN);
        fprintf(OUT, "Quality: %llu / %llu (%.3f%%)\n", compressed_quality_size, quality_size, (double)compressed_quality_size / (double)quality_size * 100);
        skip_ahead(compressed_quality_size);
    }
}


static void print_title(void)
{
    if (has_title)
    {
        unsigned long long title_size = read_number(IN);
        char *title = (char *) malloc_or_die(title_size + 1);
        if (fread(title, 1, title_size, IN) != title_size) { incomplete(); }
        title[title_size] = 0;
        fputs(title, OUT);
        free(title);
    }
    fputc('\n', OUT);
}


static void print_ids(void)
{
    if (has_ids)
    {
        load_ids();
        for (unsigned long long i = 0; i < N; i++) { fprintf(OUT, "%s\n", ids[i]); }
    }
}


static inline void print_name(unsigned long long index)
{
    if (has_ids && !has_names)
    {
        fputs(ids[index], OUT);
    }
    else if (!has_ids && has_names)
    {
        fputs(names[index], OUT);
    }
    else if (has_ids && has_names)
    {
        fputs(ids[index], OUT);
        if (names[index][0] != 0)
        {
            fputc(name_separator, OUT); 
            fputs(names[index], OUT);
        }
    }
}


static inline void print_fasta_name(unsigned long long index)
{
    fputc('>', OUT);
    print_name(index);
    fputc('\n', OUT); 
}


static inline void print_fastq_name(unsigned long long index)
{
    fputc('@', OUT);
    print_name(index);
    fputc('\n', OUT); 
}


static void print_names(void)
{
    if (has_ids || has_names)
    {
        if (has_ids) { load_ids(); }
        else { skip_ids(); }

        if (has_names) { load_names(); }

        if (has_ids && !has_names)
        {
            for (unsigned long long i = 0; i < N; i++) { fprintf(OUT, "%s\n", ids[i]); }
        }
        else if (!has_ids && has_names)
        {
            for (unsigned long long i = 0; i < N; i++) { fprintf(OUT, "%s\n", names[i]); }
        }
        else if (has_ids && has_names)
        {
            for (unsigned long long i = 0; i < N; i++)
            {
                fputs(ids[i], OUT);
                if (names[i][0] != 0)
                {
                    fputc(name_separator, OUT); 
                    fputs(names[i], OUT);
                }
                fputc('\n', OUT); 
            }
        }
    }
}


static void print_lengths(void)
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

            fprintf(OUT, "%llu\n", len);
        }
    }
}


static void print_total_length(void)
{
    if (has_lengths)
    {
        skip_ids();
        skip_names();
        skip_lengths();
        skip_mask();

        total_seq_length = read_number(IN);

        fprintf(OUT, "%llu\n", total_seq_length);
    }
}


static void print_mask(void)
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

            fprintf(OUT, "%llu\n", len);
        }
    }
}


static void print_total_mask_length(void)
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
        fprintf(OUT, "%llu\n", total_mask_length);
    }
    else
    {
        fprintf(OUT, "0\n");
    }
}


static void print_4bit(void)
{
    if (has_data)
    {
        skip_ids();
        skip_names();
        skip_lengths();
        skip_mask();

        read_number(IN);
        read_number(IN);

        size_t bytes_to_read = initialize_input_decompression();
        size_t input_size;
        while ( (input_size = read_next_chunk(in_buffer, bytes_to_read)) )
        {
            ZSTD_inBuffer in = { in_buffer, input_size, 0 };
            while (in.pos < in.size)
            {
                ZSTD_outBuffer out = { out_buffer, out_buffer_size, 0 };
                bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &in);
                if (ZSTD_isError(bytes_to_read)) { die("can't decompress: %s\n", ZSTD_getErrorName(bytes_to_read)); }
                fwrite(out_buffer, 1, out.pos, OUT);
            }
        }
    }
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

    fwrite(dna_buffer, 1, n_bp_to_print, OUT);

    total_seq_n_bp_remaining -= n_bp_to_print;
    dna_buffer_pos = 0;
}


static inline void print_dna_split_into_lines(unsigned char *buffer, size_t size)
{
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

    fwrite(out_print_buffer, 1, (size_t)(print_pos - out_print_buffer), OUT);
}


static inline void uppercase_dna_buffer(void)
{
    for (size_t i = 0; i < dna_buffer_pos; i++) { dna_buffer[i] = (unsigned char) toupper(dna_buffer[i]); }
}


static inline void print_dna_buffer_as_fasta(int masking)
{
    unsigned long long n_bp_to_print = dna_buffer_pos;
    if (n_bp_to_print > total_seq_n_bp_remaining) { n_bp_to_print = total_seq_n_bp_remaining; }

    if (masking) { mask_dna_buffer(dna_buffer, (unsigned)n_bp_to_print); }

    unsigned char *pos = dna_buffer;

    while (n_bp_to_print >= cur_seq_len_n_bp_remaining)
    {
        if (cur_seq_len_n_bp_remaining > 0)
        {
            if (max_line_length > 0) { print_dna_split_into_lines(pos, cur_seq_len_n_bp_remaining); }
            else { fwrite(pos, 1, cur_seq_len_n_bp_remaining, OUT); }

            pos += cur_seq_len_n_bp_remaining;
            n_bp_to_print -= cur_seq_len_n_bp_remaining;
            total_seq_n_bp_remaining -= cur_seq_len_n_bp_remaining;
        }

        if (lengths_buffer[cur_seq_len_index] == 4294967295u)
        {
            cur_seq_len_index++;
        }
        else
        {
            fputc('\n', OUT);
            cur_seq_len_index++;
            cur_seq_index++;

            // Print empty sequences without empty lines.
            while (cur_seq_len_index < n_lengths && cur_seq_index < N && lengths_buffer[cur_seq_len_index] == 0)
            {
                print_fasta_name(cur_seq_index);
                cur_seq_len_index++;
                cur_seq_index++;
            }

            if (cur_seq_index < N)
            {
                print_fasta_name(cur_seq_index);
                cur_line_n_bp_remaining = max_line_length;
            }
        }

        if (cur_seq_len_index >= n_lengths) { break; }

        cur_seq_len_n_bp_remaining = lengths_buffer[cur_seq_len_index];
    }

    if (n_bp_to_print > 0)
    {
        if (max_line_length > 0) { print_dna_split_into_lines(pos, n_bp_to_print); }
        else { fwrite(pos, 1, n_bp_to_print, OUT); }

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


static void print_dna(int masking)
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

        if (in_seq_type < seq_type_protein)
        {
            while ( (input_size = read_next_chunk(in_buffer, bytes_to_read)) )
            {
                ZSTD_inBuffer in = { in_buffer, input_size, 0 };
                while (in.pos < in.size)
                {
                    ZSTD_outBuffer out = { out_buffer, out_buffer_size, 0 };
                    bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &in);
                    if (ZSTD_isError(bytes_to_read)) { die("can't decompress sequence: %s\n", ZSTD_getErrorName(bytes_to_read)); }
                    write_4bit_as_dna((unsigned char *)out_buffer, out.pos, masking);
                }
            }
        }
        else
        {
            while ( (input_size = read_next_chunk(in_buffer, bytes_to_read)) )
            {
                ZSTD_inBuffer in = { in_buffer, input_size, 0 };
                while (in.pos < in.size)
                {
                    ZSTD_outBuffer out = { dna_buffer, dna_buffer_size, 0 };
                    bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &in);
                    if (ZSTD_isError(bytes_to_read)) { die("can't decompress sequence: %s\n", ZSTD_getErrorName(bytes_to_read)); }
                    dna_buffer_pos = (unsigned)out.pos;
                    if (!use_mask) { uppercase_dna_buffer(); }
                    print_dna_buffer(masking);
                }
            }
        }

        if (total_seq_n_bp_remaining > 0)
        {
            if (in_seq_type >= seq_type_protein && !use_mask) { uppercase_dna_buffer(); }
            print_dna_buffer(masking);
        }
    }
}


static void count_dna_buffer_sequence_characters(unsigned long long *counts, int masking)
{
    unsigned long long n_bp_to_print = dna_buffer_pos;
    if (n_bp_to_print > total_seq_n_bp_remaining) { n_bp_to_print = total_seq_n_bp_remaining; }

    if (masking) { mask_dna_buffer(dna_buffer, (unsigned)n_bp_to_print); }

    unsigned char *end = dna_buffer + n_bp_to_print;
    for (unsigned char *c = dna_buffer; c < end; c++)
    {
        counts[*c]++;
    }

    total_seq_n_bp_remaining -= n_bp_to_print;
    dna_buffer_pos = 0;
}


static void count_4bit_sequence_characters(unsigned long long *counts, unsigned char *buffer, size_t size, int masking)
{
    for (unsigned int i = 0; i < size; i++)
    {
        *(unsigned short *)(&dna_buffer[dna_buffer_pos]) = codes_to_nucs[buffer[i]];
        dna_buffer_pos += 2;
    }
    count_dna_buffer_sequence_characters(counts, masking);
}


static void print_charcount(int masking)
{
    if (!has_data) { return; }

    skip_ids();
    skip_names();
    skip_lengths();

    if (masking) { load_mask(); }
    else { skip_mask(); }

    unsigned long long counts[256];
    memset(counts, 0, sizeof(unsigned long long) * 256);

    total_seq_length = read_number(IN);
    compressed_seq_size = read_number(IN);
    total_seq_n_bp_remaining = total_seq_length;

    size_t bytes_to_read = initialize_input_decompression();
    size_t input_size;

    if (in_seq_type < seq_type_protein)
    {
        while ( (input_size = read_next_chunk(in_buffer, bytes_to_read)) )
        {
            ZSTD_inBuffer in = { in_buffer, input_size, 0 };
            while (in.pos < in.size)
            {
                ZSTD_outBuffer out = { out_buffer, out_buffer_size, 0 };
                bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &in);
                if (ZSTD_isError(bytes_to_read)) { die("can't decompress sequence: %s\n", ZSTD_getErrorName(bytes_to_read)); }
                count_4bit_sequence_characters(counts, (unsigned char *)out_buffer, out.pos, masking);
            }
        }
    }
    else
    {
        while ( (input_size = read_next_chunk(in_buffer, bytes_to_read)) )
        {
            ZSTD_inBuffer in = { in_buffer, input_size, 0 };
            while (in.pos < in.size)
            {
                ZSTD_outBuffer out = { dna_buffer, dna_buffer_size, 0 };
                bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &in);
                if (ZSTD_isError(bytes_to_read)) { die("can't decompress sequence: %s\n", ZSTD_getErrorName(bytes_to_read)); }
                dna_buffer_pos = (unsigned)out.pos;
                if (!use_mask) { uppercase_dna_buffer(); }
                count_dna_buffer_sequence_characters(counts, masking);
            }
        }
    }

    if (total_seq_n_bp_remaining > 0)
    {
        if (in_seq_type >= seq_type_protein && !use_mask) { uppercase_dna_buffer(); }
        count_dna_buffer_sequence_characters(counts, masking);
    }

    for (unsigned i = 0; i < 33; i++) { if (counts[i] != 0) { fprintf(OUT, "\\x%02X\t%llu\n", i, counts[i]); } }
    for (unsigned i = 33; i < 127; i++) { if (counts[i] != 0) { fprintf(OUT, "%c\t%llu\n", (unsigned char)i, counts[i]); } }
    for (unsigned i = 127; i < 256; i++) { if (counts[i] != 0) { fprintf(OUT, "\\x%02X\t%llu\n", i, counts[i]); } }
}


static void print_fasta(int masking)
{
    if (!has_data) { return; }

    load_ids();
    load_names();
    load_lengths();

    if (masking) { load_mask(); }
    else { skip_mask(); }

    total_seq_length = read_number(IN);
    compressed_seq_size = read_number(IN);
    total_seq_n_bp_remaining = total_seq_length;

    while (cur_seq_len_index < n_lengths && cur_seq_index < N && lengths_buffer[cur_seq_len_index] == 0)
    {
        print_fasta_name(cur_seq_index);
        cur_seq_len_index++;
        cur_seq_index++;
    }
    if (cur_seq_index >= N) { return; }

    print_fasta_name(cur_seq_index);
    cur_line_n_bp_remaining = max_line_length;
    cur_seq_len_n_bp_remaining = lengths_buffer[cur_seq_len_index];

    size_t bytes_to_read = initialize_input_decompression();
    size_t input_size;

    if (in_seq_type < seq_type_protein)
    {
        while ( total_seq_n_bp_remaining > 0 && (input_size = read_next_chunk(in_buffer, bytes_to_read)) )
        {
            ZSTD_inBuffer in = { in_buffer, input_size, 0 };
            while (in.pos < in.size)
            {
                ZSTD_outBuffer out = { out_buffer, out_buffer_size, 0 };
                bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &in);
                if (ZSTD_isError(bytes_to_read)) { die("can't decompress sequence: %s\n", ZSTD_getErrorName(bytes_to_read)); }
                write_4bit_as_fasta((unsigned char *)out_buffer, out.pos, masking);
            }
        }
    }
    else
    {
        while ( total_seq_n_bp_remaining > 0 && (input_size = read_next_chunk(in_buffer, bytes_to_read)) )
        {
            ZSTD_inBuffer in = { in_buffer, input_size, 0 };
            while (in.pos < in.size)
            {
                ZSTD_outBuffer out = { dna_buffer, dna_buffer_size, 0 };
                bytes_to_read = ZSTD_decompressStream(input_decompression_stream, &out, &in);
                if (ZSTD_isError(bytes_to_read)) { die("can't decompress sequence: %s\n", ZSTD_getErrorName(bytes_to_read)); }
                dna_buffer_pos = (unsigned)out.pos;
                if (!use_mask) { uppercase_dna_buffer(); }
                print_dna_buffer_as_fasta(masking);
            }
        }
    }

    if (total_seq_n_bp_remaining > 0)
    {
        if (in_seq_type >= seq_type_protein && !use_mask) { uppercase_dna_buffer(); }
        print_dna_buffer_as_fasta(masking);
    }
}
