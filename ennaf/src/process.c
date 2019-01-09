/*
 * NAF compressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 *
 * The FASTA/Q parser is based on Heng Li's kseq.h.
 */

#ifndef round_up_to_power_of_two
#define round_up_to_power_of_two(x) (--(x), (x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16, (x)|=(x)>>32, ++(x))
#endif


typedef struct
{
    size_t length, allocated;
    unsigned char *data;
}
string_t;


string_t name    = { 0, 0, NULL };
string_t comment = { 0, 0, NULL };
string_t seq     = { 0, 0, NULL };
string_t qual    = { 0, 0, NULL };


__attribute__((always_inline))
static inline void refill_in_buffer(void)
{
    assert(in_buffer != NULL);

    in_begin = 0;
    while (1)
    {
        ssize_t could_read = read(in_fd, in_buffer, in_buffer_size);
        if (could_read >= 0) { in_end = (size_t)could_read; return; }
        if (errno == EINTR) { continue; }
        else if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            struct pollfd p = { in_fd, POLLIN, 0 };
            poll(&p, 1, -1);
            continue;
        }
        else { fprintf(stderr, "Error reading input\n"); exit(1); }
    }
}


__attribute__((always_inline))
static inline int in_get_char(void)
{
    if (in_begin >= in_end)
    {
        refill_in_buffer();
        if (in_end == 0) { return -1; }
    }
    return in_buffer[in_begin++];
}


static inline int getuntil(bool *delim_arr, string_t *str)
{
    int c = -1;
    for (;;)
    {
        if (in_begin >= in_end)
        {
            refill_in_buffer();
            if (in_end == 0) { break; }
        }

        size_t i;
        for (i = in_begin; i < in_end; i++)
        {
            if (delim_arr[in_buffer[i]]) { c = in_buffer[i]; break; }
        }

        if (str->allocated - str->length < i - in_begin + 1)
        {
            str->allocated = str->length + (i - in_begin) + 1;
            round_up_to_power_of_two(str->allocated);
            str->data = (unsigned char *)realloc(str->data, str->allocated);
        }

        memcpy(str->data + str->length, in_buffer + in_begin, i - in_begin);
        str->length += (i - in_begin);
        in_begin = i + 1;

        if (c >= 0) { break; }
    }

    str->data[str->length] = '\0';
    return c;
}


__attribute__((always_inline))
static inline int get_fasta_seq(void)
{
    int c;

    name.length = 0;
    comment.length = 0;
    seq.length = 0;

    if ( (c = getuntil(is_space_arr, &name)) == -1) { comment.data[0] = '\0'; return -1; }

    if (is_eol_arr[c]) { comment.data[0] = '\0'; }
    else if (getuntil(is_eol_arr, &comment) == -1) { return -1; }

    size_t old_len = seq.length;
    while ( (c = getuntil(is_space_or_gt_arr, &seq)) != -1)
    {
        if (is_eol_arr[c])
        {
            if (seq.length - old_len > longest_line_length) { longest_line_length = seq.length - old_len; }
            old_len = seq.length;
        }
        else if (c == '>') { break; }
    }

    return c;
}


static void process_fasta(void)
{
    while (1)
    {
        int c = get_fasta_seq();

        if (store_ids)
        {
            ids_size_original += name.length + 1;
            write_to_cstream(ids_cstream, IDS, name.data, name.length + 1);
        }

        if (store_comm)
        {
            comm_size_original += comment.length + 1;
            write_to_cstream(comm_cstream, COMM, comment.data, comment.length + 1);
        }

        if (store_len)
        {
            add_length(seq.length);
        }

        if (store_mask)
        {
            extract_mask( (unsigned char *) seq.data, seq.length);
        }

        if (store_seq)
        {
            seq_size_original += seq.length;
            encode_dna( (unsigned char *) seq.data, seq.length);
        }

        n_sequences++;

        if (c == -1) { return; }
    }
}


__attribute__((always_inline))
static inline int get_fastq_seq(void)
{
    int c;

    name.length = 0;
    comment.length = 0;
    seq.length = 0;
    qual.length = 0;

    if ( (c = getuntil(is_space_arr, &name)) == -1) { comment.data[0] = '\0'; return -1; }

    if (is_eol_arr[c]) { comment.data[0] = '\0'; }
    else if (getuntil(is_eol_arr, &comment) == -1) { return -1; }

    size_t old_len = seq.length;
    while ( (c = getuntil(is_space_or_plus_arr, &seq)) != -1)
    {
        if (is_eol_arr[c])
        {
            if (seq.length - old_len > longest_line_length) { longest_line_length = seq.length - old_len; }
            old_len = seq.length;
        }
        else if (c == '+') { break; }
    }

    while ( (c = getuntil(is_space_arr, &qual)) != -1 && qual.length < seq.length) {}
    if (qual.length != seq.length) { fprintf(stderr, "Error: quality of sequence %lld doesn't match sequence length\n", n_sequences + 1); }

    return c;
}


static void process_fastq(void)
{
    while (1)
    {
        int c = get_fastq_seq();

        if (store_ids)
        {
            ids_size_original += name.length + 1;
            write_to_cstream(ids_cstream, IDS, name.data, name.length + 1);
        }

        if (store_comm)
        {
            comm_size_original += comment.length + 1;
            write_to_cstream(comm_cstream, COMM, comment.data, comment.length + 1);
        }

        if (store_len)
        {
            add_length(seq.length);
        }

        if (store_mask)
        {
            extract_mask( (unsigned char *) seq.data, seq.length);
        }

        if (store_seq)
        {
            seq_size_original += seq.length;
            encode_dna( (unsigned char *) seq.data, seq.length);
        }

        if (store_qual)
        {
            qual_size_original += qual.length;
            write_to_cstream(qual_cstream, QUAL, qual.data, qual.length);
        }

        n_sequences++;

        while ((c = in_get_char()) != -1 && c != '@') {}
        if (c != '@') { return; }
    }
}


static void confirm_input_format(void)
{
    assert(in_format_from_input == in_format_unknown);

    int c;
    while ((c = in_get_char()) != -1 && is_space_arr[c]) {}
    if (c == -1) { return; }

    if (c == '>') { in_format_from_input = in_format_fasta; }
    else if (c == '@') { in_format_from_input = in_format_fastq; }
    else
    {
        fprintf(stderr, "Error: Input data is in unknown format: first non-space character is neither '>' nor '@'\n");
        exit(1);
    }

    if (in_format_from_command_line != in_format_unknown &&
        in_format_from_command_line != in_format_from_input)
    {
        fprintf(stderr, "Error: Input data format is different from format specified in the command line\n");
        exit(1);
    }    

    if (in_format_from_extension != in_format_unknown &&
        in_format_from_extension != in_format_from_input)
    {
        fprintf(stderr, "Warning: Input file extension does not match its actual format\n");
    }

    if (in_format_from_extension != in_format_unknown &&
        in_format_from_command_line != in_format_unknown &&
        in_format_from_extension != in_format_from_command_line)
    {
        fprintf(stderr, "Warning: Input file extension does not match format specified in the command line\n");
    }
}


static void process(void)
{
    if (in_format_from_input == in_format_unknown) { return; }

    name.allocated = 256;
    name.data = (unsigned char *)malloc(name.allocated);
    comment.allocated = 256;
    comment.data = (unsigned char *)malloc(comment.allocated);
    seq.allocated = 256;
    seq.data = (unsigned char *)malloc(seq.allocated);

    if (in_format_from_input == in_format_fasta)
    {
        process_fasta();
    }
    else if (in_format_from_input == in_format_fastq)
    {
        qual.allocated = 256;
        qual.data = (unsigned char *)malloc(qual.allocated);
        process_fastq();
    }
    else { assert(0); }
}
