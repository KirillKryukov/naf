# NAF Decompression Manual

## Synopsis

`unnaf file.naf >file.fa` - Decompress into default format
(FASTQ if input NAF file contains qualities, FASTA otherwise).

`unnaf file.fa.naf` - Decompress into automatically named 'file.fa' (the .naf file is never deleted).

`unnaf --number file.naf` - Show number of sequences stored in "file.naf".

`unnaf --ids file.naf >ids.txt` - Extract sequence IDs (accession numbers).

`unnaf --fasta file.naf >file.fa` - Extract FASTA-formatted sequences (even if "file.naf" has qualities).

`unnaf --fastq file.naf >file.fq` - Decompress into FASTQ format (fails if "file.naf" has no qualities).

## Options for specifying output type

Only one of these options should be specified:

**--fasta** - FASTA formatted sequences (with mask, if available).

**--fastq** - FASTQ format. Will fail if input has no qualities.

**--seq** - All sequences concatenated into one, without names or line breaks.

**--number** - Number of sequences.

**--title** - Dataset title (if available).

**--ids** - List of sequence IDs.

**--names** - List of complete sequence names.

**--lengths** - List of sequence lengths.

**--total-length** - Total sequence length (sum of lengths).

**--mask** - List of mask interval lengths.

**--4bit** - All sequences, concatenated, in 4-bit encoding (binary data).
(Only works for DNA or RNA sequences).

**--part-list** - List of sections of NAF file.

**--sizes** - Sizes of sections of NAF file.

**--format** - Sequence type and version of NAF format found in the input.

## Other options

**-o FILE** - Write output to FILE.

**-c** - Write to standard output.

**--line-length N** - Divide sequences into lines of N bp, ignoring line length stored in the NAF file.
Effective only for `--fasta` output. Line length of 0 means unlimited lines, i.e., each sequence printed in single line.

**--no-mask** - Ignore mask, useful only for `--fasta` and `--dna` outputs.

**-h**, **--help** - Show usage help.

**-V**, **--version** - Show version.
