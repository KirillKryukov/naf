/*
 * NAF compressor
 * Copyright (c) 2018 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

static void init_encoders(void)
{
    assert(out_buffer_size != 0);
    assert(out_4bit_buffer == NULL);
    assert(file_copy_buffer == NULL);
    assert(length_units == NULL);
    assert(mask_units == NULL);

    out_4bit_buffer = (unsigned char *) malloc(out_buffer_size);
    out_4bit_pos = out_4bit_buffer;

    file_copy_buffer = (unsigned char *) malloc(file_copy_buffer_size);

    length_units = (unsigned int *) malloc(sizeof(unsigned int) * length_units_buffer_n_units);

    mask_units = (unsigned char *) malloc(mask_units_buffer_size);
    mask_units_end = mask_units + mask_units_buffer_size;
    mask_units_pos = mask_units;
}


static void encode_dna(const char *str, size_t size)
{
    assert(str != NULL);
    assert(out_4bit_buffer != NULL);
    assert(out_4bit_pos != NULL);
    assert(seq_cstream != NULL);
    assert(SEQ != NULL);

    const char *end = str + size;
    const char *p = str;

    if (p < end && parity)
    {
        *out_4bit_pos++ |= (unsigned char)(nuc_code[ * (const unsigned char *) p ] * 16);
        if (out_4bit_pos >= out_4bit_buffer + out_buffer_size)
        {
            write_to_cstream(seq_cstream, SEQ, out_4bit_buffer, out_buffer_size);
            out_4bit_pos = out_4bit_buffer;
        }
        parity = false;
        p++;
    }

    const char *end1 = p + ( (unsigned long long)(end - p) & ~1ull );
    for (; p < end1; p += 2)
    {
        *out_4bit_pos++ = nuc_code[ * (const unsigned char *) p ] | 
                          (unsigned char)(nuc_code[ * (const unsigned char *) (p+1) ] * 16);
        if (out_4bit_pos >= out_4bit_buffer + out_buffer_size)
        {
            write_to_cstream(seq_cstream, SEQ, out_4bit_buffer, out_buffer_size);
            out_4bit_pos = out_4bit_buffer;
        }
    }

    if (p < end)
    {
        *out_4bit_pos = nuc_code[ * (const unsigned char *) p ];
        parity = true;
    }
}


static void add_length(size_t len)
{
    assert(length_units != NULL);
    assert(length_unit_index < length_units_buffer_n_units);
    assert(len_cstream != NULL);
    assert(LEN != NULL);

    while (len >= 0xFFFFFFFFull)
    {
        length_units[length_unit_index++] = 0xFFFFFFFFu;
        len -= 0xFFFFFFFFull;
        if (length_unit_index >= length_units_buffer_n_units)
        {
            write_to_cstream(len_cstream, LEN, length_units, sizeof(unsigned int) * length_units_buffer_n_units);
            n_length_units_stored += length_units_buffer_n_units;
            length_unit_index = 0;
        }
    }

    length_units[length_unit_index++] = (unsigned int)len;
    if (length_unit_index >= length_units_buffer_n_units)
    {
        write_to_cstream(len_cstream, LEN, length_units, sizeof(unsigned int) * length_units_buffer_n_units);
        n_length_units_stored += length_units_buffer_n_units;
        length_unit_index = 0;
    }
}


static void add_mask(unsigned long long len)
{
    assert(mask_units != NULL);
    assert(mask_units_end != NULL);
    assert(mask_units_pos != NULL);
    assert(mask_units_pos < mask_units_end);
    assert(mask_cstream != NULL);
    assert(MASK != NULL);

    while (len >= 255ull)
    {
        *mask_units_pos++ = 255;
        len -= 255ull;
        if (mask_units_pos >= mask_units_end)
        {
            write_to_cstream(mask_cstream, MASK, mask_units, mask_units_buffer_size);
            n_mask_units_stored += mask_units_buffer_size;
            mask_units_pos = mask_units;
        }
    }

    *mask_units_pos++ = (unsigned char)len;
    if (mask_units_pos >= mask_units_end)
    {
        write_to_cstream(mask_cstream, MASK, mask_units, mask_units_buffer_size);
        n_mask_units_stored += mask_units_buffer_size;
        mask_units_pos = mask_units;
    }
}


static void extract_mask(const char *seq, size_t len)
{
    assert(seq != NULL);
    assert(mask_units != NULL);

    const char *end = seq + len;
    for (const char *c = seq; c < end; )
    {
        if (mask_on != (*c >= 96))
        {
            add_mask(mask_len);
            mask_len = 0;
            mask_on = !mask_on;
        }

        const char *start = c;
        if (mask_on) { while (c < end && *c >= 96) { c++; } }
        else { while (c < end && *c < 96) { c++; } }
        mask_len += (unsigned long long)(c - start);
    }
}


static void copy_file_to_out(char* from_path)
{
    assert(from_path != NULL);
    assert(file_copy_buffer != NULL);
    assert(OUT != NULL);

    FILE *F = fopen(from_path, "rb");
    if (F == NULL) { fprintf(stderr, "Can't open \"%s\"\n", from_path); exit(1); }

    if (fseeko(F, 0, SEEK_END) != 0) { fprintf(stderr, "Can't seek to the end of file \"%s\"\n", from_path); exit(1); }
    off_t file_size = ftello(F);
    if (file_size < 0) { fprintf(stderr, "Can't determine size of \"%s\"\n", from_path); exit(1); }
    if (file_size < 4) { fprintf(stderr, "\"%s\" is smaller than 4 bytes\n", from_path); exit(1); }

    size_t data_size = (size_t)(file_size - 4);
    write_variable_length_encoded_number(OUT, data_size);

    if (fseeko(F, 4, SEEK_SET) != 0) { fprintf(stderr, "Can't seek to data start in \"%s\"\n", from_path); exit(1); }

    size_t remaining = data_size;
    while (remaining > 0)
    {
        size_t to_read = (file_copy_buffer_size <= remaining) ? file_copy_buffer_size : remaining;
        fread_or_die(file_copy_buffer, 1, to_read, F);
        fwrite_or_die(file_copy_buffer, 1, to_read, OUT);
        remaining -= to_read;
    }

    fclose(F);
}
