/*
 * NAF compressor
 * Copyright (c) 2018-2019 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 *
 * The FASTA/Q parser was originally based on Heng Li's kseq.h.
 */

#define INEOF 256

#define STR_ALLOCATED 131072


static void name_writer(unsigned char *str, size_t size)
{
    ids_size_original += size;
    ids_size_compressed += write_to_cstream(ids_cstream, IDS, str, size);
}


static void comm_writer(unsigned char *str, size_t size)
{
    comm_size_original += size;
    comm_size_compressed += write_to_cstream(comm_cstream, COMM, str, size);
}


static void seq_writer_masked_4bit(unsigned char *str, size_t size)
{
    extract_mask(str, size);
    seq_size_original += size;
    encode_dna(str, size);
}


static void seq_writer_nonmasked_4bit(unsigned char *str, size_t size)
{
    seq_size_original += size;
    encode_dna(str, size);
}


static void seq_writer_masked_text(unsigned char *str, size_t size)
{
    extract_mask(str, size);
    seq_size_original += size;
    seq_size_compressed += write_to_cstream(seq_cstream, SEQ, str, size);
}


static void seq_writer_nonmasked_text(unsigned char *str, size_t size)
{
    seq_size_original += size;
    seq_size_compressed += write_to_cstream(seq_cstream, SEQ, str, size);
}


static void qual_writer(unsigned char *str, size_t size)
{
    qual_size_original += size;
    qual_size_compressed += write_to_cstream(qual_cstream, QUAL, str, size);
}


typedef struct
{
    size_t length;
    unsigned char *data;
    void (*writer)(unsigned char *, size_t);
}
string_t;


static string_t name    = { 0, NULL, &name_writer };
static string_t comment = { 0, NULL, &comm_writer };
static string_t seq     = { 0, NULL, NULL };
static string_t qual    = { 0, NULL, &qual_writer };


static void report_unexpected_char_stats(unsigned long long *n, const char *seq_type_name)
{
    unsigned long long total = 0;
    for (unsigned i = 0; i < 257; i++) { total += n[i]; }
    if (total > 0)
    {
        msg("input has %" PRINT_ULL " unexpected %s codes:\n", total, seq_type_name);
        for (unsigned i = 0; i < 32; i++) { if (n[i] != 0) { msg("    '\\x%02X': %" PRINT_ULL "\n", i, n[i]); } }
        for (unsigned i = 32; i < 127; i++) { if (n[i] != 0) { msg("    '%c': %" PRINT_ULL "\n", (unsigned char)i, n[i]); } }
        for (unsigned i = 127; i < 256; i++) { if (n[i] != 0) { msg("    '\\x%02X': %" PRINT_ULL "\n", i, n[i]); } }
        if (n[256] != 0) { msg("    EOF: %" PRINT_ULL "\n", n[256]); }
    }
}


static void report_unexpected_input_char_stats(void)
{
    report_unexpected_char_stats(n_unexpected_seq_characters, in_seq_type_name);
    report_unexpected_char_stats(n_unexpected_qual_characters, "quality");
}


static void unexpected_input_char(unsigned c)
{
    if (abort_on_unexpected_code)
    {
        die("Error: Unexpected %s code '%c' in sequence %" PRINT_ULL "\n", in_seq_type_name, (unsigned char)c, n_sequences + 1);
    }
    else { n_unexpected_seq_characters[c]++; }
}


static void unexpected_quality_char(unsigned c)
{
    if (abort_on_unexpected_code)
    {
        die("Error: Unexpected quality code '%c' in sequence %" PRINT_ULL "\n", (unsigned char)c, n_sequences + 1);
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
    unsigned d = INEOF;
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
            if (delim_arr[in_buffer[i]]) { d = in_buffer[i]; break; }
        }

        in_begin = i + 1;
        if (d != INEOF) { break; }
    }
    return d;
}


/*
 * Reads input until a delimiter character is found (any matching 'delim_arr').
 * Stores text until delimiter into 'str' (not including delimiter).
 * Returns delimiter, or INEOF at end of input.
 * Does NOT zero-terminate the text stored in 'str'.
 * Whenever 'str' fills, writes it out.
 */
static inline unsigned in_get_until(const bool *delim_arr, string_t *str)
{
    unsigned d = INEOF;
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
            if (delim_arr[in_buffer[i]]) { d = in_buffer[i]; break; }
        }

        size_t s = i - in_begin;

        if (str->length + s >= STR_ALLOCATED)
        {
            size_t s1 = STR_ALLOCATED - str->length;
            size_t s2 = s - s1;
            memcpy(str->data + str->length, in_buffer + in_begin, s1);
            str->writer(str->data, STR_ALLOCATED);
            memcpy(str->data, in_buffer + in_begin + s1, s2); 
            str->length = s2;
        }
        else
        {
            memcpy(str->data + str->length, in_buffer + in_begin, s);
            str->length += s;
        }

        in_begin = i + 1;

        if (d != INEOF) { break; }
    }

    return d;
}


__attribute__((always_inline))
static inline void str_append_char(string_t *str, unsigned char c)
{
    str->data[str->length] = c;
    str->length++;
    if (str->length >= STR_ALLOCATED)
    {
        str->writer(str->data, STR_ALLOCATED);
        str->length = 0;
    }
}


static void process_fasta(void)
{
    unsigned c;
    do {
        // At this point the '>' was already read, so we immediately proceed to read the name.
        c = in_get_until(is_space_arr, &name);
        str_append_char(&name, '\0');

        // Always write '\0' to comment stream.
        if (c != INEOF && !is_eol_arr[c]) { c = in_get_until(is_eol_arr, &comment); }
        str_append_char(&comment, '\0');

        unsigned long long old_total_seq_size = seq_size_original + seq.length;
        if (c != INEOF)
        {
            unsigned long long old_len = old_total_seq_size;
            while ( (c = in_get_until(is_unexpected_arr, &seq)) != INEOF)
            {
                if (is_eol_arr[c])
                {
                    unsigned long long new_len = seq_size_original + seq.length;
                    if (new_len - old_len > longest_line_length) { longest_line_length = new_len - old_len; }
                    old_len = new_len;

                    c = in_get_char();
                    if (!is_unexpected_arr[c]) { str_append_char(&seq, (unsigned char)c); continue; }
                    else if (c == '>' || c == INEOF) { break; }
                    else if (is_eol_arr[c])
                    {
                        while (c != INEOF && is_eol_arr[c]) { c = in_get_char(); }
                        if (c == '>' || c == INEOF) { break; }
                        else if (!is_unexpected_arr[c]) { str_append_char(&seq, (unsigned char)c); continue; }
                        else if (is_space_arr[c]) {}
                        else { unexpected_input_char(c); str_append_char(&seq, unexpected_char_replacement); }
                    }
                    else if (is_space_arr[c]) {}
                    else { unexpected_input_char(c); str_append_char(&seq, unexpected_char_replacement); }
                }
                else if (is_space_arr[c]) {}
                else { unexpected_input_char(c); str_append_char(&seq, unexpected_char_replacement); }
            }

            // If the last line is the longest, and has no end-of-line character, handle it correctly.
            if (c == INEOF)
            {
                unsigned long long new_len = seq_size_original + seq.length;
                if (new_len - old_len > longest_line_length) { longest_line_length = new_len - old_len; }
            }
        }

        add_length(seq_size_original + seq.length - old_total_seq_size);
        n_sequences++;
    }
    while (c != INEOF);
}


static void process_fastq(void)
{
    unsigned c;
    do {
        c = in_get_until(is_space_arr, &name);
        str_append_char(&name, '\0');

        if (c != INEOF && !is_eol_arr[c]) { c = in_get_until(is_eol_arr, &comment); }
        str_append_char(&comment, '\0');

        unsigned long long read_length = 0;
        if (c != INEOF)
        {
            unsigned long long old_len = seq_size_original + seq.length;
            while ( (c = in_get_until(is_unexpected_arr, &seq)) != INEOF)
            {
                if (is_eol_arr[c]) { break; }
                else if (is_space_arr[c]) {}
                else { unexpected_input_char(c); str_append_char(&seq, unexpected_char_replacement); }
            }
            unsigned long long new_len = seq_size_original + seq.length;
            read_length = new_len - old_len;
            if (read_length > longest_line_length) { longest_line_length = read_length; }

            if (c == INEOF) { die("Error: truncated FASTQ input: last sequence has no quality\n"); }

            do { c = in_get_char(); } while (is_eol_arr[c]);
            if (c != '+') { die("Error: truncated FASTQ input: last sequence has no quality\n"); }

            c = in_skip_until(is_eol_arr);
            if (!is_eol_arr[c]) { die("Error: truncated FASTQ input: last sequence has no quality\n"); }

            do { c = in_get_char(); } while (is_eol_arr[c]);
            if (c == INEOF) { die("Error: truncated FASTQ input: last sequence has no quality\n"); }

            old_len = qual_size_original + qual.length;
            str_append_char(&qual, (unsigned char)c);
            while ( (c = in_get_until(is_unexpected_qual_arr, &qual)) != INEOF)
            {
                if (is_eol_arr[c]) { break; }
                else if (is_space_arr[c]) {}
                else { unexpected_quality_char(c); str_append_char(&qual, '!'); }  // Unknown character can only mean poor quality.
            }
            new_len = qual_size_original + qual.length;
            unsigned long long qual_length = new_len - old_len;
            if (qual_length != read_length)
            {
                die("Error: quality length of sequence %" PRINT_ULL " (%" PRINT_ULL ") doesn't match sequence length (%" PRINT_ULL ")\n",
                    n_sequences + 1, qual_length, read_length);
            }
        }

        add_length(read_length);
        n_sequences++;

        do { c = in_get_char(); } while (is_eol_arr[c]);
    }
    while (c == '@');
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
        if (c == '>' || c == '@') { die("Invalid input: First '%c' is not at the beginning of the line\n", (unsigned char)c); }
        else { die("Input data is in unknown format: first non-space character is neither '>' nor '@'\n"); }
    }

    if (in_format_from_command_line != in_format_unknown &&
        in_format_from_command_line != in_format_from_input)
    {
        die("Error: Input data format is different from format specified in the command line\n");
    }    

    if (in_format_from_extension != in_format_unknown &&
        in_format_from_extension != in_format_from_input)
    {
        err("Warning: Input file extension does not match its actual format\n");
    }

    if (in_format_from_extension != in_format_unknown &&
        in_format_from_command_line != in_format_unknown &&
        in_format_from_extension != in_format_from_command_line)
    {
        err("Warning: Input file extension does not match format specified in the command line\n");
    }
}


static void process(void)
{
    // If input format is unknown at this point, it indicates empty input.
    if (in_format_from_input == in_format_unknown) { return; }

    name.data    = (unsigned char *) malloc_or_die(STR_ALLOCATED);
    comment.data = (unsigned char *) malloc_or_die(STR_ALLOCATED);
    seq.data     = (unsigned char *) malloc_or_die(STR_ALLOCATED);

    seq.writer = store_mask ? ((in_seq_type < seq_type_protein) ? &seq_writer_masked_4bit : &seq_writer_masked_text)
                            : ((in_seq_type < seq_type_protein) ? &seq_writer_nonmasked_4bit : &seq_writer_nonmasked_text);

    if (in_format_from_input == in_format_fasta)
    {
        process_fasta();
    }
    else if (in_format_from_input == in_format_fastq)
    {
        qual.data = (unsigned char *) malloc_or_die(STR_ALLOCATED);
        process_fastq();
        if (qual.length != 0) { qual.writer(qual.data, qual.length); qual.length = 0; }
    }
    else { assert(0); }

    if (name.length != 0) { name.writer(name.data, name.length); name.length = 0; }
    if (comment.length != 0) { comment.writer(comment.data, comment.length); comment.length = 0; }
    if (seq.length != 0) { seq.writer(seq.data, seq.length); seq.length = 0; }
}
