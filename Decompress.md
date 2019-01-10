# NAF Decompression Manual

## Synopsis

`unnaf file.naf >file.fa` - Decompress into default format
(FASTQ if input NAF file contains qualities, FASTA otherwise).

`unnaf --number file.naf` - Show number of sequences stored in "file.naf".

`unnaf --ids file.naf >ids.txt` - Extract sequence IDs (accession numbers).

`unnaf --fasta file.naf >file.fa` - Extract FASTA-formatted sequences (even if "file.naf" has qualities).

`unnaf --fastq file.naf >file.fq` - Decompress into FASTQ format (fails if "file.naf" has no qualities).

## Output stream

unnaf prints all output to standard output stream.
Take care to not decompress a massive file to your terminal.
Try to always redirect output to file, or pipe into next command,
other than for small outputs such as `--number`.

## Options for specifying output type

Only one of these options should be specified:

**--fasta** - FASTA formatted sequences (with mask, if available).

**--fastq** - FASTQ format. Will fail if input has no qualities.

**--dna** - All sequences concatenated into one, without names or line breaks.

**--number** - Number of sequences.

**--title** - Dataset title (if available).

**--ids** - List of sequence IDs.

**--names** - List of complete sequence names.

**--lengths** - List of sequence lengths.

**--total-length** - Total sequence length (sum of lengths).

**--mask** - List of mask interval lengths.

**--4bit** - All sequences, concatenated, in 4-bit encoding (binary data).

**--part-list** - List of sections of NAF file.

**--sizes** - Sizes of sections of NAF file.

**--format** - Version of NAF format of input file.

## Other options

**--no-mask** - Ignore mask, useful only for `--fasta` and `--dna` outputs.

**--help** - Show usage help.

**--version** - Show version.
