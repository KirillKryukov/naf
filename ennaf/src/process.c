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


static void report_unexpected_char_stats(unsigned long long *n, const char *seq_type_name)
{
    unsigned long long total = 0;
    for (unsigned i = 0; i < 257; i++) { total += n[i]; }
    if (total > 0)
    {
        fprintf(stderr, "input has %llu unexpected %s codes:\n", total, seq_type_name);
        for (unsigned i = 0; i < 32; i++) { if (n[i] != 0) { fprintf(stderr, "    '\\%u': %llu\n", i, n[i]); } }
        for (unsigned i = 32; i < 127; i++) { if (n[i] != 0) { fprintf(stderr, "    '%c': %llu\n", (unsigned char)i, n[i]); } }
        for (unsigned i = 127; i < 256; i++) { if (n[i] != 0) { fprintf(stderr, "    '\\%u': %llu\n", i, n[i]); } }
        if (n[256] != 0) { fprintf(stderr, "    EOF: %llu\n", n[256]); }
    }
}


static void report_unexpected_input_char_stats(void)
{
    report_unexpected_char_stats(n_unexpected_seq_characters, in_seq_type_name);
    report_unexpected_char_stats(n_unexpected_qual_characters, "quality");
}


static void unexpected_input_char(unsigned c, unsigned char *seq_name)
{
    if (abort_on_unexpected_code)
    {
        fprintf(stderr, "Error: Unexpected %s code '%c' in sequence \"%s\"\n", in_seq_type_name, (unsigned char)c, seq_name);
        exit(1);
    }
    else { n_unexpected_seq_characters[c]++; }
}


static void unexpected_quality_char(unsigned c, unsigned char *seq_name)
{
    if (abort_on_unexpected_code)
    {
        fprintf(stderr, "Error: Unexpected quality code '%c' in sequence \"%s\"\n", (unsigned char)c, seq_name);
        exit(1);
    }
    else { n_unexpected_qual_characters[c]++; }
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


static inline unsigned in_skip_until(const bool *delim_arr)
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

        in_begin = i + 1;
        if (c != INEOF) { break; }
    }
    return c;
}


static inline unsigned in_get_until(const bool *delim_arr, string_t *str)
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
    if ( (c = in_get_until(is_space_arr, &name)) == INEOF) { comment.data[0] = '\0'; return INEOF; } // No need to 0-terminate sequence.

    if (is_eol_arr[c]) { comment.data[0] = '\0'; }
    else if (in_get_until(is_eol_arr, &comment) == INEOF) { return INEOF; }

    size_t old_len = seq.length;
    while ( (c = in_get_until(is_unexpected_arr, &seq)) != INEOF)
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
                else { unexpected_input_char(c, name.data); str_append_char(&seq, unexpected_char_replacement); }
            }
            else if (is_space_arr[c]) {}
            else { unexpected_input_char(c, name.data); str_append_char(&seq, unexpected_char_replacement); }
        }
        else if (is_space_arr[c]) {}
        else { unexpected_input_char(c, name.data); str_append_char(&seq, unexpected_char_replacement); }
    }

    // If the last line is the longest, and has no end-of-line character, handle it correctly.
    if (c == INEOF)
    {
        if (seq.length - old_len > longest_line_length) { longest_line_length = seq.length - old_len; }
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
            if (in_seq_type < seq_type_protein) { encode_dna(seq.data, seq.length); }
            else { seq_size_compressed += write_to_cstream(seq_cstream, SEQ, seq.data, seq.length); }
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

    if ( (c = in_get_until(is_space_arr, &name)) == INEOF) { comment.data[0] = '\0'; return INEOF; }

    if (is_eol_arr[c]) { comment.data[0] = '\0'; }
    else if (in_get_until(is_eol_arr, &comment) == INEOF) { return INEOF; }

    while ( (c = in_get_until(is_unexpected_arr, &seq)) != INEOF)
    {
        if (is_eol_arr[c]) { break; }
        else if (is_space_arr[c]) {}
        else { unexpected_input_char(c, name.data); str_append_char(&seq, unexpected_char_replacement); }
    }
    if (seq.length > longest_line_length) { longest_line_length = seq.length; }
    if (c == INEOF) { fprintf(stderr, "Error: truncated FASTQ input: last sequence has no quality\n"); exit(1); }

    do { c = in_get_char(); } while (is_eol_arr[c]);
    if (c != '+') { fprintf(stderr, "Error: truncated FASTQ input: last sequence has no quality\n"); exit(1); }

    c = in_skip_until(is_eol_arr);
    if (!is_eol_arr[c]) { fprintf(stderr, "Error: truncated FASTQ input: last sequence has no quality\n"); exit(1); }

    do { c = in_get_char(); } while (is_eol_arr[c]);
    if (c == INEOF) { fprintf(stderr, "Error: truncated FASTQ input: last sequence has no quality\n"); exit(1); }
    qual.length = 1;
    qual.data[0] = (unsigned char)c;

    while ( (c = in_get_until(is_unexpected_qual_arr, &qual)) != INEOF)
    {
        if (is_eol_arr[c]) { break; }
        else if (is_space_arr[c]) {}
        else { unexpected_quality_char(c, name.data); str_append_char(&qual, '!'); }  // Unknown character can only mean poor quality.
    }
    if (qual.length != seq.length)
    {
        fprintf(stderr, "Error: quality length of sequence %llu (%llu) doesn't match sequence length (%llu)\n",
                        n_sequences + 1, (unsigned long long)qual.length, (unsigned long long)seq.length);
        exit(1);
    }

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
            if (in_seq_type < seq_type_protein) { encode_dna(seq.data, seq.length); }
            else { seq_size_compressed += write_to_cstream(seq_cstream, SEQ, seq.data, seq.length); }
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
