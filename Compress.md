# NAF Compression Manual

## Synopsis

`ennaf --in file.fa --out file.naf` - Compress a FASTA file.

`ennaf <file.fa >file.naf` - Compress using IO redirection.

`ennaf <file.fq >file.naf` - Compress a FASTQ file (format is detected automatically).

`ennaf --level 22 <file.fa >file.naf` - Use maximum compression level.

`gzip -dc file.gz | ennaf >file.naf` - Recompress from gzip to NAF on the fly.

## Temporary storage

Compression process stores temporary data on disk.
Therefore please check the following before compressing large files:

1. Temporary directory is specified in TMPDIR or TMP environment variable,
 or in `--temp-dir` command line option of your ennaf command.
 Note that ennaf doesn't have a default setting for temporary directory, such as "/tmp", it will only use directory specified in the environment or command line.
1. Temporary directory is on your fastest SSD drive.
1. Temporary directory has sufficient space to hold the compressed data.
 About 1/4 of the uncompressed data size should be normally fine, but safer to have 1/2 or more.
 Note that storage devices and filesystems can slow down when nearly full, so having extra free temporary space is generally a good idea.

## Options

**--in FILE** - Compress FILE. If omitted, stdin is used by default.

**--out FILE** - Write compressed NAF format data to FILE.
If omitted, stdout stream is used, but only if it's redirected into a file, or piped into next tool.

**--temp-dir DIR** - Use DIR for temporary files.
If omitted, uses directory specified in enviroment variable `TMPDIR`.
If there's no such variable, tries enviroment variable `TMP`.
If both variables are not defined, the compressor exits without writing anything.

**--name NAME** - Use NAME as prefix for temporary files. It omitted, generates a random prefix.

**--title TITLE** - Store TITLE as dataset title in the output NAF file.

**--level N** - Use compression level N. This corresponds to zstd compression level.
The default is 1. Higher levels provide better compression, but are slower.
Maximum level is 22, however take care as levels above 19 are slow and use significant amount of RAM.
Negative levels are supported, down to -131072, for higher speed at the expense of compactness.

**--fasta** - Proceed only if input is in FASTA format.

**--fastq** - Proceed only if input is in FASTQ format.

**--line-length N** - Store line length N in the output NAF file.
If omitted, stores the maximum sequence line length from the input.

**--verbose** - Verbose mode.

**--keep-temp-files** - Don't delete temporary files (normally they are deleted after compression is done).

**--no-mask** - Don't store sequence mask (lower/upper characters).

**--help** - Show usage help.

**--version** - Show versions.

## Which compression level to choose?

The default level 1 is suitable when time is limited, or when the machine doing the compression is not fast enough (or has too little RAM).
For example, when transferring reads from a sequencer machine, `ennaf --level 1` can be used instead of gzip.

On the other hand, if you compress data for storing in a database,
the maximum level 22 may be preferable, even though it's slower.
In database usage, data has to be compressed only once,
while network transfer and decompression may be performed thousands of times by database users.
Optimizing user experience is more important in such cases.
So, `ennaf --level 22` is the best option for sequence databases.

## Specifying input format

Input format (FASTA of FASTQ) is automatically detected from the actual input data, so there's not need to specify it.
However, explicitly specifying the format with `--fastq` or `--fastq`
will make sure the compression occurs only if the input is in the expected format.

In additon, input file extension is checked against specified format and actual format.
A mismatching file extension produces a warning, but does not stop the compression.
