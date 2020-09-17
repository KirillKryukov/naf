# NAF Compression Manual

## Synopsis

`ennaf file.fa -o file.naf` - Compress a FASTA file with default compression level 1.

`ennaf file.fa` - Compress into automatically named 'file.fa.naf' (the original file is never deleted).

`ennaf -c <file.fa >file.naf` - Compress using IO redirection.

`ennaf file.fq -o file.naf` - Compress a FASTQ file (format is detected automatically).

`ennaf -22 file.fa -o file.naf` - Use maximum compression level.

`gzip -dc file.gz | ennaf -o file.naf` - Recompress from gzip to NAF on the fly.

## Options

**-o FILE** - Write compressed NAF format data to FILE.

**-c** - Write compressed output to console (standard output stream).

**-#** - Use compression level #. This corresponds to zstd compression level.
The default is 1. Higher levels provide better compression, but are slower.
Maximum level is 22, however take care as levels above 19 are slow and use significant amount of RAM.

**--level #** - Use compression level #.
Same with `-#`, but also supports even faster negative levels, down to -131072.

**--temp-dir DIR** - Use DIR for temporary files.
If omitted, uses directory specified in enviroment variable `TMPDIR`.
If there's no such variable, tries enviroment variable `TMP`.
If both variables are not defined, the compressor exits without writing anything.

**--name NAME** - Use NAME as prefix for temporary files. It omitted, generates a random prefix.

**--title TITLE** - Store TITLE as dataset title in the output NAF file.

**--fasta** - Proceed only if input is in FASTA format.

**--fastq** - Proceed only if input is in FASTQ format.

**--dna** - Input contains DNA sequences (default).
Valid sequences can include: ACGT, RYSWKMBDHV, N (as well as all these letters in lower case), '-'.

**--rna** - Input contains RNA sequences.
Valid sequences can include: ACGU, RYSWKMBDHV, N (as well as all these letters in lower case), '-'.

**--protein** - Input has protein sequences.
Recognized amino acid codes:
'ARNDCQEGHILKMFPSTWYV' (standard 20 amino acids),
'U' (selenocysteine), 'O' (pyrrolysine),
'J' (leucine or isoleucine, 'L' or 'I'),
'B' (aspartic acid or asparagine, 'D' or 'N'),
'Z' (glutamic acid or glutamine, 'E' or 'Q'),
'X' (any amino acid),
'\*' (stop codon),
and '-' (gap).
I.e., entire Latin alphabet (in both upper and lower case), asterisk and dash.

**--text** - Input has text sequences.
Each sequence can include any printable single byte characters, which means characters in code ranges: 33..126 and 128..254.

**--strict** - Fail on encountering any non-standard sequence character.
The list of standard characters depends on sequence type, selected using `--dna`, `--rna`, `--protein` or `--text` option.
Without `--strict` , the compressor will simply replace any unknown characters with the default substitution character
('N' for DNA/RNA, 'X' for protein, '?' for text), and report the total number of occurrences for each unexpected character.

**--line-length N** - Store line length N in the output NAF file.
If omitted, stores the maximum sequence line length from the input.

**--verbose** - Verbose mode.

**--keep-temp-files** - Don't delete temporary files (normally they are deleted after compression is done).
Specifying this option forces creation of temporary files even in cases
where they would be otherwise not created due to data being small.

**--no-mask** - Don't store sequence mask (lower/upper characters).
Converts the sequences to upper case before compression.

**--binary-stderr** - Set stderr stream to binary mode. Mainly useful for running test suite on Windows.

**-h**, **--help** - Show usage help.

**-V**, **--version** - Show version.

## Temporary storage

Compression process may store temporary data on disk.
Therefore please check the following before running `ennaf`:

1. Temporary directory is specified in TMPDIR or TMP environment variable,
 or in `--temp-dir` command line option of your `ennaf` command.
 Note that `ennaf` doesn't have a default setting for temporary directory, such as "/tmp", it will only use directory specified in the environment or command line.
1. Temporary directory is on your fastest SSD drive.
1. Temporary directory has sufficient space to hold the compressed data.
 About 1/4 of the uncompressed data size should be normally fine, but safer to have 1/2 or more.
 Note that storage devices and filesystems can slow down when nearly full, so having extra free temporary space is generally a good idea.

## Which compression level to choose?

The default level 1 is suitable when time is limited, or when the machine doing the compression is not fast enough (or has too little RAM).
For example, when transferring reads from a sequencer machine, `ennaf -1` can be used instead of gzip.
(Equivalent to just `ennaf`, since `-1` is the default level).

On the other hand, if you compress data for storing in a database,
the maximum level 22 may be preferable, even though it's slower.
In database usage, data has to be compressed only once,
while network transfer and decompression may be performed thousands of times by database users.
Optimizing user experience is more important in such cases.
So, `ennaf -22` is the best option for sequence databases.

## Specifying input format

Input format (FASTA of FASTQ) is automatically detected from the actual input data, so there's not need to specify it.
However, explicitly specifying the format with `--fasta` or `--fastq`
will make sure the compression occurs only if the input is in the expected format.

In additon, input file extension is checked against specified format and actual format.
A mismatching file extension produces a warning, but does not stop the compression.

## Where the compressed output goes?

  * If `-c` is specified, the compressed output is written to stdout (standard output stream).
  * If `-o FILE` is specified, the compressed output is written to FILE.
  * Otherwise (no `-o` or `-c`)
    * If output is redirected away from console, compressed output is sent to stdout (standard output stream).
    * If stdout is console and input file is specified, output file is automatically named by appending ".naf" to input path.
    * Otherwise an error message is shown.

## What characters are supported in sequences?

Input sequence type can be selected by `--dna`, `--rna`, `--protein` or `--text` argument.
If not specified, by default input is assumed to be DNA.

Recognized characters in each sequence type:
  * DNA: "ACGTacgt" (nucleotide codes), "RYSWKMBDHVNryswkmbdhvn" (ambiguous codes), '-' (gap).
  * RNA: "ACGUacgu" (nucleotide codes), "RYSWKMBDHVNryswkmbdhvn" (ambiguous codes), '-' (gap).
  * Protein: 'A' to 'Z' and 'a' to 'z', '\*' (stop codon), '-' (gap).
  * Text: Characters with codes 33..126 and 128..254 (printable non-space ASCII and extended ASCII).

If `--no-mask` is specified, all lower case characters are stored in upper case.

Note that text sequences can include the '>' character.
However, in FASTA-formatted input any such character occurring at the line start
are interpreted as starting the header of the next sequence.
So, if you use FASTA-formatted text sequences, you have to either not use '>' as part of the sequence,
or make sure that such characters are not placed at the beginning of a line where they can be mistaken for start of the next sequence.

In case of FASTQ input there is no such ambiguity, because `ennaf` only supports single line FASTQ sequences.
A '+' character can occur anywhere, including the beginning of such sequence
(relevant only for _text_ sequences in FASTQ format).

## What happens to unsupported characters?

Any spaces and tabs found in the input sequences are silently discarded.
They never appear in decompressed sequences.

As for any other unknown characters:
If `--strict` option is used, any such character causes compression to fail with error message.
(No output file is produced).

Without `--strict` the unsupported characters are replaced by:
  * 'N' for DNA/RNA
  * 'X' for protein
  * '?' for text

The compressor also reports the number of each unknown character.

## What FASTQ variants are supported?

Only single line sequence and quality are supported in FASTQ input.

The compressor ignores the content of the '+' line, and does not verify it for identity with the '@' line.
In the decompressed FASTQ output, the '+' line is always empty (has nothing except the '+'),
regardless of what it contained before compression.

Quality can include characters with codes from 33 to 126 (printable non-space ASCII).

By default DNA sequences are expected,
however `--rna`, `--protein` and `--text` options are available for FASTQ as well.

## Preserving non-standard sequence characters

For example, your DNA sequence may use 'Z' for methilated cytosine.
It will change into 'N' if you compress it as DNA.
Therefore, please switch to protein mode (`--protein`) to preserve 'Z' and other non-standard codes.

Similarly, if you mix DNA and RNA ('T' and 'U') in a single FASTA file,
you have to use protein mode.

If your non-standard codes go beyond alphabet and include digits or punctuation
(such as '.' for identical base with first sequence),
you have to switch to text mode (`--text`).

## Using text mode for DNA data

Since both `--dna` and `--text` modes can be used for DNA data, which is better?
Short answer: `--dna` is faster and has stronger compression.
For details, see [this benchmark page](http://kirill-kryukov.com/study/naf/benchmark-text-vs-dna-Spur.html).

## Can it compress multiple files into single archive?

Yes, with the help of a [Multi-Multi-FASTA file format](https://github.com/KirillKryukov/mumu).
It works similarly to gzipping a tar file:
First you combine individual FASTA files into a single Multi-Multi-FASTA stream, then compress it using _ennaf_.
Example commands:

Compressing:<br>
`mumu.pl --dir 'Helicobacter' 'Helicobacter pylori*' | ennaf -22 --text -o Hp.nafnaf`

Decompressing and unpacking:<br>
`unnaf Hp.nafnaf | mumu.pl --unpack --dir 'Helicobacter'`

"**nafnaf**" is the recommended filename extension for such archives containing multiple FASTA files.
