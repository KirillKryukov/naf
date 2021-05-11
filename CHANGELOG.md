# NAF Changelog

## Current
- Updated zstd to v1.4.9.
- Added support for empty sequences.
- Added `--binary` shortcut option to _unnaf_. 

## 1.2.0 - 2020-09-01
- Added `--sequences` option to _unnaf_.
- Added `--binary-stdout` option to _unnaf_.
- Added `--binary-stderr` option to both _ennaf_ and _unnaf_.
- Updated zstd to v1.4.5.
- Improved compatibility with MinGW.

## 1.1.0 - 2019-10-01
- Added support for RNA, protein and text sequences, enabled with `--rna`, `--protein` and `--text` switches.
- Added report for number of unknown characters at the end of compression.
- Added strict compression mode (`--strict` switch). In this mode _ennaf_ fails on any unexpected input character.
- Installation from source no longer requires pre-installing zstd.
- _ennaf_ no longer loads each entire input sequence to memory.
- _ennaf_ no longer creates temporary files if the input is small.
- Added `-o` and `-c` arguments to _unnaf_.
- Added `--charcount` output to _unnaf_.
- Added test suite.
- Updated zstd to v1.4.3.
- Fixed streaming mode in MinGW builds.

## 1.0.0 - 2019-01-17
- Initial release.
- Supports DNA sequences in FASTA and FASTQ formats.
- Extracts and stores sequence mask, with the option to ignore it, for both compression and decompression.
- Supports alignments (sequences with gap marked as '-').
- Supports N and other ambiguous IUPAC nucleotide codes (R, Y, S, W, K, M, B, D, H, V).
- Autodetects, stores and recovers FASTA line length.
- Can pipe all input and output, enabling use in pipelines.
- Has partial decompression options: concatenated sequence, sequence ids, names, lengths, mask, 4-bit encoded sequence.
- Very fast on low compression levels, while still providing useful compression.
- Provides state of the art compression strength on high compression levels.
- Works on Windows, Linux and Mac.
