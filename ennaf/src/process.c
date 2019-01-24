/*
 * NAF compressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 *
 * The FASTA/Q parser was originally based on Heng Li's kseq.h.
 */

#ifndef round_up_to_power_of_two
#if __x86_64__
#define round_up_to_power_of_two(x) (--(x), (x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16, (x)|=(x)>>32, ++(x))
#else
#define round_up_to_power_of_two(x) (--(x), (x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16, ++(x))
#endif
#endif

#define INEOF 256


typedef struct
{
    size_t length, allocated;
    unsigned char *data;
}
string_t;


static string_t name    = { 0, 0, NULL };
static string_t comment = { 0, 0, NULL };
static string_t seq     = { 0, 0, NULL };
static string_t qual    = { 0, 0, NULL };


static void report_unexpected_input_char_stats(void)
{
    unsigned long long total = 0;
    for (unsigned i = 0; i < 257; i++) { total += n_unexpected_charactes[i]; }
    if (total > 0)
    {
        fprintf(stderr, "input has %llu unexpected %s codes\n", total, in_seq_type_name);
    }
}


static void unexpected_input_char(unsigned c)
{
    if (abort_on_unexpected_code)
    {
        fprintf(stderr, "Error: Unexpected %s code '%c'\n", in_seq_type_name, (unsigned char)c);
        exit(1);
    }
    else { n_unexpected_charactes[c]++; }
}


__attribute__((always_inline))
static inline void refill_in_buffer(void)
{
    assert(in_buffer != NULL);

    in_begin = 0;
    in_end = fread(in_buffer, 1, in_buffer_size, IN);
}


__attribute__((always_inline))
static inline unsigned in_get_char(void)
{
    if (in_begin >= in_end)
    {
        refill_in_buffer();
        if (in_end == 0) { return INEOF; }
    }
    return in_buffer[in_begin++];
}


static inline unsigned getuntil(const bool *delim_arr, string_t *str)
{
    unsigned c = INEOF;
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

        if (c != INEOF) { break; }
    }

    str->data[str->length] = '\0';
    return c;
}


__attribute__((always_inline))
static inline void str_append_char(string_t *str, unsigned char c)
{
    if (str->allocated < str->length + 1)
    {
        str->allocated = str->length + 1;
        round_up_to_power_of_two(str->allocated);
        str->data = (unsigned char *)realloc(str->data, str->allocated);
    }

    str->data[str->length] = c;
    str->length++;
}


__attribute__((always_inline))
static inline unsigned get_fasta_seq(void)
{
    unsigned c;

    name.length = 0;
    comment.length = 0;
    seq.length = 0;

    // At this point the '>' was already read, so we immediately proceed to read the name.
    if ( (c = getuntil(is_space_arr, &name)) == INEOF) { comment.data[0] = '\0'; return INEOF; } // No need to 0-terminate sequence.

    if (is_eol_arr[c]) { comment.data[0] = '\0'; }
    else if (getuntil(is_eol_arr, &comment) == INEOF) { return INEOF; }

    size_t old_len = seq.length;
    while ( (c = getuntil(is_unexpected_arr, &seq)) != INEOF)
    {
        if (is_eol_arr[c])
        {
            if (seq.length - old_len > longest_line_length) { longest_line_length = seq.length - old_len; }
            old_len = seq.length;

            c = in_get_char();
            if (!is_unexpected_arr[c]) { str_append_char(&seq, (unsigned char)c); continue; }
            else if (c == '>' || c == INEOF) { break; }
            else if (is_eol_arr[c])
            {
                while (c != INEOF && is_eol_arr[c]) { c = in_get_char(); }
                if (c == '>' || c == INEOF) { break; }
                else if (!is_unexpected_arr[c]) { str_append_char(&seq, (unsigned char)c); continue; }
                else if (is_space_arr[c]) {}
                else { unexpected_input_char(c); str_append_char(&seq, 'N'); }
            }
            else if (is_space_arr[c]) {}
            else { unexpected_input_char(c); str_append_char(&seq, 'N'); }
        }
        else if (is_space_arr[c]) {}
        else { unexpected_input_char(c); str_append_char(&seq, 'N'); }
    }

    return c;
}


static void process_fasta(void)
{
    for (;;)
    {
        unsigned c = get_fasta_seq();

        if (store_ids)
        {
            ids_size_original += name.length + 1;
            ids_size_compressed += write_to_cstream(ids_cstream, IDS, name.data, name.length + 1);
        }

        if (store_comm)
        {
            comm_size_original += comment.length + 1;
            comm_size_compressed += write_to_cstream(comm_cstream, COMM, comment.data, comment.length + 1);
        }

        if (store_len)
        {
            add_length(seq.length);
        }

        if (store_mask)
        {
            extract_mask(seq.data, seq.length);
        }

        if (store_seq)
        {
            seq_size_original += seq.length;
            encode_dna(seq.data, seq.length);
        }

        n_sequences++;

        if (c == INEOF) { return; }
    }
}


__attribute__((always_inline))
static inline unsigned get_fastq_seq(void)
{
    unsigned c;

    name.length = 0;
    comment.length = 0;
    seq.length = 0;
    qual.length = 0;

    if ( (c = getuntil(is_space_arr, &name)) == INEOF) { comment.data[0] = '\0'; return INEOF; }

    if (is_eol_arr[c]) { comment.data[0] = '\0'; }
    else if (getuntil(is_eol_arr, &comment) == INEOF) { return INEOF; }

    size_t old_len = seq.length;
    while ( (c = getuntil(is_space_or_plus_arr, &seq)) != INEOF)
    {
        if (is_eol_arr[c])
        {
            if (seq.length - old_len > longest_line_length) { longest_line_length = seq.length - old_len; }
            old_len = seq.length;
        }
        else if (c == '+') { break; }
    }

    while ( (c = getuntil(is_space_arr, &qual)) != INEOF && qual.length < seq.length) {}
    if (qual.length != seq.length) { fprintf(stderr, "Error: quality of sequence %llu doesn't match sequence length\n", n_sequences + 1); }

    return c;
}


static void process_fastq(void)
{
    for (;;)
    {
        unsigned c = get_fastq_seq();

        if (store_ids)
        {
            ids_size_original += name.length + 1;
            ids_size_compressed += write_to_cstream(ids_cstream, IDS, name.data, name.length + 1);
        }

        if (store_comm)
        {
            comm_size_original += comment.length + 1;
            comm_size_compressed += write_to_cstream(comm_cstream, COMM, comment.data, comment.length + 1);
        }

        if (store_len)
        {
            add_length(seq.length);
        }

        if (store_mask)
        {
            extract_mask(seq.data, seq.length);
        }

        if (store_seq)
        {
            seq_size_original += seq.length;
            encode_dna(seq.data, seq.length);
        }

        if (store_qual)
        {
            qual_size_original += qual.length;
            qual_size_compressed += write_to_cstream(qual_cstream, QUAL, qual.data, qual.length);
        }

        n_sequences++;

        while ((c = in_get_char()) != INEOF && c != '@') {}
        if (c != '@') { return; }
    }
}


static void confirm_input_format(void)
{
    assert(in_format_from_input == in_format_unknown);

    unsigned last_c = '\n';
    unsigned c;

    while ((c = in_get_char()) != INEOF && is_space_arr[c]) { last_c = c; }
    if (c == INEOF) { return; }

    if (c == '>' && is_eol_arr[last_c]) { in_format_from_input = in_format_fasta; }
    else if (c == '@' && is_eol_arr[last_c]) { in_format_from_input = in_format_fastq; }
    else
    {
        if (c == '>' || c == '@') { fprintf(stderr, "Invalid input: First '%c' is not at the beginning of the line\n", (unsigned char)c); }
        else { fputs("Input data is in unknown format: first non-space character is neither '>' nor '@'\n", stderr); }
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
    // If input format is unknown at this point, it indicates empty input.
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
