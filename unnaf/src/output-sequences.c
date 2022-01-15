/*
 * NAF decompressor
 * Copyright (c) 2018-2022 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

static inline void print_dna_buffer_as_sequences(int masking)
{
    unsigned long long n_bp_to_print = dna_buffer_pos;
    if (n_bp_to_print > total_seq_n_bp_remaining) { n_bp_to_print = total_seq_n_bp_remaining; }

    if (masking) { mask_dna_buffer(dna_buffer, (unsigned)n_bp_to_print); }

    unsigned char *pos = dna_buffer;

    while (n_bp_to_print >= cur_seq_len_n_bp_remaining)
    {
        if (cur_seq_len_n_bp_remaining > 0)
        {
            fwrite(pos, 1, cur_seq_len_n_bp_remaining, OUT);
            pos += cur_seq_len_n_bp_remaining;
            n_bp_to_print -= cur_seq_len_n_bp_remaining;
            total_seq_n_bp_remaining -= cur_seq_len_n_bp_remaining;
        }

        if (lengths_buffer[cur_seq_len_index] != 4294967295u)
        {
            fputc('\n', OUT);
            cur_seq_index++;
        }

        cur_seq_len_index++;
        if (cur_seq_len_index >= n_lengths) { break; }

        cur_seq_len_n_bp_remaining = lengths_buffer[cur_seq_len_index];
    }

    if (n_bp_to_print > 0)
    {
        fwrite(pos, 1, n_bp_to_print, OUT);
        cur_seq_len_n_bp_remaining -= n_bp_to_print;
        total_seq_n_bp_remaining -= n_bp_to_print;
    }

    dna_buffer_pos = 0;
}


static inline void write_4bit_as_sequences(unsigned char *buffer, size_t size, int masking)
{
    for (unsigned int i = 0; i < size; i++)
    {
        *(unsigned short *)(&dna_buffer[dna_buffer_pos]) = codes_to_nucs[buffer[i]];
        dna_buffer_pos += 2;
    }

    if (dna_buffer_pos > dna_buffer_flush_size) { print_dna_buffer_as_sequences(masking); }
}


static void print_sequences(int masking)
{
    if (!has_data) { return; }

    skip_ids();
    skip_names();
    load_lengths();

    if (masking) { load_mask(); }
    else { skip_mask(); }

    total_seq_length = read_number(IN);
    compressed_seq_size = read_number(IN);
    total_seq_n_bp_remaining = total_seq_length;
    cur_seq_len_n_bp_remaining = lengths_buffer[0];

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
                write_4bit_as_sequences((unsigned char *)out_buffer, out.pos, masking);
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
                print_dna_buffer_as_sequences(masking);
            }
        }
    }

    if (total_seq_n_bp_remaining > 0)
    {
        if (in_seq_type >= seq_type_protein && !use_mask) { uppercase_dna_buffer(); }
        print_dna_buffer_as_sequences(masking);
    }
}
