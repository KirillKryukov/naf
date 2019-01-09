/*
 * NAF decompressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

static void print_dna_from_memory(unsigned int len)
{
    unsigned int remaining_bp = len;
    while (remaining_bp > 0)
    {
        if (dna_buffer_remaining == 0) { refill_dna_buffer_from_memory(); }

        unsigned int n_bp_to_print = remaining_bp;
        if (n_bp_to_print > dna_buffer_remaining) { n_bp_to_print = dna_buffer_remaining; }

        fwrite(dna_buffer + dna_buffer_printing_pos, 1, n_bp_to_print, stdout);
        dna_buffer_printing_pos += n_bp_to_print;
        dna_buffer_remaining -= n_bp_to_print;
        remaining_bp -= n_bp_to_print;
    }
}


static void print_next_sequence_from_memory(void)
{
    while (lengths_buffer[cur_seq_len_index] == 4294967295u)
    {
        print_dna_from_memory(lengths_buffer[cur_seq_len_index]);
        cur_seq_len_index++;
    }
    print_dna_from_memory(lengths_buffer[cur_seq_len_index]);
    cur_seq_len_index++;
    fputc('\n', stdout);
}


static void print_quality_from_file(unsigned int len)
{
    unsigned int remaining_bp = len;
    while (remaining_bp > 0)
    {
        if (quality_buffer_remaining == 0) { refill_quality_buffer_from_file(); }

        unsigned int n_bp_to_print = remaining_bp;
        if (n_bp_to_print > quality_buffer_remaining) { n_bp_to_print = quality_buffer_remaining; }

        fwrite(quality_buffer + quality_buffer_printing_pos, 1, n_bp_to_print, stdout);
        quality_buffer_printing_pos += n_bp_to_print;
        quality_buffer_remaining -= n_bp_to_print;
        remaining_bp -= n_bp_to_print;
    }
}


static void print_next_quality_from_file(void)
{
    while (lengths_buffer[cur_qual_len_index] == 4294967295u)
    {
        print_quality_from_file(lengths_buffer[cur_qual_len_index]);
        cur_qual_len_index++;
    }
    print_quality_from_file(lengths_buffer[cur_qual_len_index]);
    cur_qual_len_index++;
    fputc('\n', stdout);
}


__attribute__ ((noreturn))
static void print_fastq_and_exit(int masking)
{
    if (has_data)
    {
        quality_buffer_flush_size = ZSTD_DStreamOutSize();
        quality_buffer_size = quality_buffer_flush_size * 2 + 10;
        quality_buffer = (char *)malloc(quality_buffer_size);
        if (!quality_buffer) { fprintf(stderr, "Can't allocate %zu bytes for quality buffer\n", quality_buffer_size); exit(1); }

        load_ids();
        load_names();
        load_lengths();

        if (masking) { load_mask(); }
        else { skip_mask(); }

        load_compressed_sequence();
        initialize_memory_decompression();

        zstd_mem_in_buffer.src = compressed_seq_buffer;
        zstd_mem_in_buffer.size = memory_bytes_to_read;
        zstd_mem_in_buffer.pos = 0;
        compressed_seq_pos = memory_bytes_to_read;

        total_quality_length = read_number(IN);
        compressed_quality_size = read_number(IN);

        initialize_quality_file_decompression();

        for (unsigned long long ri = 0; ri < N; ri++)
        {
            print_fastq_name(ri);
            print_next_sequence_from_memory();
            fputs("+\n", stdout);
            print_next_quality_from_file();
        }
    }

    exit(0);
}
