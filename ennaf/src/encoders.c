/*
 * NAF compressor
 * Copyright (c) 2018-2021 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

static void init_encoders(void)
{
    assert(out_4bit_buffer == NULL);
    assert(file_copy_buffer == NULL);
    assert(length_units == NULL);
    assert(mask_units == NULL);

    zstd_stream_recommended_out_buffer_size = ZSTD_CStreamOutSize();

    out_4bit_buffer_size = ZSTD_CStreamOutSize();
    out_4bit_buffer = (unsigned char *) malloc_or_die(out_4bit_buffer_size);
    out_4bit_pos = out_4bit_buffer;

    file_copy_buffer = (unsigned char *) malloc_or_die(file_copy_buffer_size);

    length_units = (unsigned int *) malloc_or_die(sizeof(unsigned int) * length_units_buffer_n_units);

    mask_units = (unsigned char *) malloc_or_die(mask_units_buffer_size);
    mask_units_end = mask_units + mask_units_buffer_size;
    mask_units_pos = mask_units;
}


static void encode_dna(const unsigned char *str, size_t size)
{
    assert(str != NULL);
    assert(out_4bit_buffer != NULL);
    assert(out_4bit_pos != NULL);
    assert(SEQ.cstream != NULL);

    const unsigned char *end = str + size;
    const unsigned char *p = str;

    if (p < end && parity)
    {
        *out_4bit_pos++ |= (unsigned char)(nuc_code[*p] * 16);
        if (out_4bit_pos >= out_4bit_buffer + out_4bit_buffer_size)
        {
            compress(&SEQ, out_4bit_buffer, out_4bit_buffer_size);
            out_4bit_pos = out_4bit_buffer;
        }
        parity = false;
        p++;
    }

    const unsigned char *end1 = p + ( (unsigned long long)(end - p) & ~1ull );
    for (; p < end1; p += 2)
    {
        *out_4bit_pos++ = nuc_code[*p] | 
                          (unsigned char)(nuc_code[*(p+1)] * 16);
        if (out_4bit_pos >= out_4bit_buffer + out_4bit_buffer_size)
        {
            compress(&SEQ, out_4bit_buffer, out_4bit_buffer_size);
            out_4bit_pos = out_4bit_buffer;
        }
    }

    if (p < end)
    {
        *out_4bit_pos = nuc_code[*p];
        parity = true;
    }
}


static void add_length(size_t len)
{
    assert(length_units != NULL);
    assert(length_unit_index < length_units_buffer_n_units);
    assert(LEN.cstream != NULL);

    while (len >= 0xFFFFFFFFull)
    {
        length_units[length_unit_index++] = 0xFFFFFFFFu;
        len -= 0xFFFFFFFFull;
        if (length_unit_index >= length_units_buffer_n_units)
        {
            compress(&LEN, length_units, sizeof(unsigned int) * length_units_buffer_n_units);
            length_unit_index = 0;
        }
    }

    length_units[length_unit_index++] = (unsigned int)len;
    if (length_unit_index >= length_units_buffer_n_units)
    {
        compress(&LEN, length_units, sizeof(unsigned int) * length_units_buffer_n_units);
        length_unit_index = 0;
    }
}


static void add_mask(unsigned long long len)
{
    assert(mask_units != NULL);
    assert(mask_units_end != NULL);
    assert(mask_units_pos != NULL);
    assert(mask_units_pos < mask_units_end);
    assert(MASK.cstream != NULL);

    while (len >= 255ull)
    {
        *mask_units_pos++ = 255;
        len -= 255ull;
        if (mask_units_pos >= mask_units_end)
        {
            compress(&MASK, mask_units, mask_units_buffer_size);
            mask_units_pos = mask_units;
        }
    }

    *mask_units_pos++ = (unsigned char)len;
    if (mask_units_pos >= mask_units_end)
    {
        compress(&MASK, mask_units, mask_units_buffer_size);
        mask_units_pos = mask_units;
    }
}


static void extract_mask(const unsigned char *seq, size_t len)
{
    assert(seq != NULL);
    assert(mask_units != NULL);

    const unsigned char *end = seq + len;
    for (const unsigned char *c = seq; c < end; )
    {
        if (mask_on != (*c >= 96))
        {
            add_mask(mask_len);
            mask_len = 0;
            mask_on = !mask_on;
        }

        const unsigned char *start = c;
        if (mask_on) { while (c < end && *c >= 96) { c++; } }
        else { while (c < end && *c < 96) { c++; } }
        mask_len += (unsigned long long)(c - start);
    }
}


/*
 * Copies the content of an already open file into output stream.
 * Starts from "start", copies exactly "expected_size" bytes.
 */
static void copy_file_to_out(FILE* FROM, char *from_path, long start, unsigned long long data_size)
{
    assert(FROM != NULL);
    assert(from_path != NULL);
    assert(file_copy_buffer != NULL);
    assert(OUT != NULL);

    fflush_or_die(FROM);

    if (fseek(FROM, start, SEEK_SET) != 0) { die("can't seek to data start in \"%s\"\n", from_path); }

    unsigned long long remaining = data_size;
    while (remaining > 0)
    {
        size_t to_read = (file_copy_buffer_size <= remaining) ? file_copy_buffer_size : remaining;
        fread_or_die(file_copy_buffer, 1, to_read, FROM);
        fwrite_or_die(file_copy_buffer, 1, to_read, OUT);
        remaining -= to_read;
    }
}


static void write_variable_length_encoded_number(FILE *F, unsigned long long a)
{
    assert(F != NULL);

    unsigned char vle_buffer[10];
    unsigned char *b = vle_buffer + 10;
    *--b = (unsigned char)(a & 127ull);
    a >>= 7;
    while (a > 0)
    {
        *--b = (unsigned char)(128ull | (a & 127ull));
        a >>= 7;
    }
    size_t len = (size_t)(vle_buffer + 10 - b);
    fwrite_or_die(b, 1, len, F);
}
