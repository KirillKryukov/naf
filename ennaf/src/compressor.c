/*
 * NAF compressor
 * Copyright (c) 2018-2021 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

static ZSTD_CStream* create_zstd_cstream(int level, int window_size_log)
{
    ZSTD_CStream *s = ZSTD_createCStream();
    if (s == NULL) { die("ZSTD_createCStream() error\n"); }

    if (window_size_log != 0)
    {
        ZSTD_TRY(ZSTD_CCtx_setParameter(s, ZSTD_c_enableLongDistanceMatching, 1));
        ZSTD_TRY(ZSTD_CCtx_setParameter(s, ZSTD_c_windowLog, window_size_log));
    }

    size_t const initResult = ZSTD_initCStream(s, level);
    if (ZSTD_isError(initResult)) { die("ZSTD_initCStream() error: %s\n", ZSTD_getErrorName(initResult)); }
    return s;
}


static void compressor_init(compressor_t *w, const char *name, int window_size_log)
{
    assert(w != NULL);
    assert(w->allocated == 0);
    assert(w->fill == 0);
    assert(w->uncompressed_size == 0);
    assert(w->compressed_size == 0);
    assert(w->written == 0);
    assert(w->cstream == NULL);
    assert(w->file == NULL);
    assert(w->path == NULL);
    assert(w->buf == NULL);
    assert(temp_path_length != 0);
    assert(temp_dir != NULL);
    assert(temp_prefix != NULL);
    assert(name != NULL);
    assert(strlen(temp_dir) + 1 + strlen(temp_prefix) + 1 + strlen(name) <= temp_path_length);

    w->allocated = COMPRESSED_BUFFER_SIZE;
    w->buf = (unsigned char *) malloc_or_die(w->allocated);
    w->cstream = create_zstd_cstream(compression_level, window_size_log);
    w->path = (char *) malloc_or_die(temp_path_length + 1);
    snprintf(w->path, temp_path_length, "%s/%s.%s", temp_dir, temp_prefix, name);
    if (verbose) { msg("Temp %s file: \"%s\"\n", name, w->path); }
}


__attribute__((always_inline))
static inline void compressor_create_file(compressor_t *w)
{
    assert(w != NULL);

    if (w->file == NULL)
    {
        w->file = fopen(w->path, "wb+");
        if (w->file == NULL) { die("can't create temporary file \"%s\"\n", w->path); }
    }
}


static void compressor_end_stream(compressor_t *w)
{
    assert(w != NULL);

    if (w->cstream != NULL)
    {
        assert(w->buf != NULL);
        assert(w->fill <= w->allocated);

        if (w->fill + zstd_stream_recommended_out_buffer_size > w->allocated)
        {
            compressor_create_file(w);
            fwrite_or_die(w->buf, 1, w->fill, w->file);
            w->written += w->fill;
            w->fill = 0;
        }

        ZSTD_outBuffer output = { w->buf + w->fill, w->allocated - w->fill, 0 };
        size_t const remainingToFlush = ZSTD_endStream(w->cstream, &output);
        if (remainingToFlush != 0) { die("can't end zstd stream\n"); }
        w->fill += output.pos;
        w->compressed_size += output.pos;
        w->cstream = NULL;

        if (keep_temp_files)
        {
            compressor_create_file(w);
            fwrite_or_die(w->buf, 1, w->fill, w->file);
            w->written += w->fill;
            w->fill = 0;
        }
    }
}


static void compressor_done(compressor_t *w)
{
    assert(w != NULL);

    if (w->buf != NULL) { free(w->buf); w->buf = NULL; }

    if (w->file != NULL)
    {
        fclose_or_die(w->file);
        w->file = NULL;

        if (!keep_temp_files)
        {
            assert(w->path != NULL);
            if (remove(w->path) != 0) { err("can't remove temporary file \"%s\"\n", w->path); }
        }
    }
}


__attribute__((always_inline))
static inline void compress(compressor_t *w, const void *data, size_t size)
{
    assert(w != NULL);
    assert(data != NULL);
    assert(w->buf != NULL);

    ZSTD_inBuffer input = { data, size, 0 };
    while (input.pos < input.size)
    {
        assert(w->fill < w->allocated);

        ZSTD_outBuffer output = { w->buf + w->fill, w->allocated - w->fill, 0 };
        size_t toRead = ZSTD_compressStream(w->cstream, &output, &input);
        if (ZSTD_isError(toRead)) { die("ZSTD_compressStream() error: %s\n", ZSTD_getErrorName(toRead)); }
        w->fill += output.pos;
        w->compressed_size += output.pos;

        if (w->fill + zstd_stream_recommended_out_buffer_size >= w->allocated)
        {
            compressor_create_file(w);
            fwrite_or_die(w->buf, 1, w->fill, w->file);
            w->written += w->fill;
            w->fill = 0;
        }
    }

    w->uncompressed_size += size;
}


static void write_compressed_data(FILE *F, compressor_t *w)
{
    assert(F != NULL);
    assert(w != NULL);
    assert(w->buf != NULL);

    if (w->compressed_size < 4) { die("compression failed\n"); }

    write_variable_length_encoded_number(F, w->compressed_size - 4);

    if (w->file == NULL)
    {
        if (w->fill > 0)
        {
            if (w->fill < 4) { die("compression failed\n"); }
            fwrite_or_die(w->buf + 4, 1, w->fill - 4, F);
        }
    }
    else
    {
        copy_file_to_out(w->file, w->path, 4, w->written - 4);
        if (w->fill > 0) { fwrite_or_die(w->buf, 1, w->fill, F); }
    }
}
