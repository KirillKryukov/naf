# NAF Changelog

## Unreleased
- Added support for RNA, protein and text sequences, enabled with `--dna`, `--protein` and `--text` switches.
- Added report for number of unknown characters at the end of compression.
- Added strict compression mode (`--strict` switch). In this mode _ennaf_ fails on any unexpected input character.
- Installation from source no longer requires pre-installing zstd.
- _ennaf_ no longer loads each entire input sequence to memory.
- Added `-o` and `-c` arguments to _unnaf_.
- Temporary files are no longer used with small input.

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
