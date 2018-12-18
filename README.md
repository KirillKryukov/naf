# Nucleotide Archival Format (NAF)
NAF is a binary file format for DNA sequence data.
It's based on zstd, and features strong compression and fast decompression.
It supports FASTA and FASTQ-formatted sequences, ambiguous IUPAC codes, masked sequence,
and has no limit on sequence length or number of sequences. See [NAF homepage](http://kirill-kryukov.com/study/naf/) for details and benchmarks.

## Format specification

NAF specification is in public domain: [NAFv1.pdf](NAFv1.pdf)

## NAF encoder "ennaf"

Prerequisites: perl and zstd.
(E.g., to install on Ubuntu: `sudo apt install perl zstd`).

Installing: Simply copy [ennaf.pl](ennaf/ennaf.pl) to a convenient directory and set executable permissions.

Using:
`ennaf.pl --in-format fasta <file.fasta >file.naf`

## NAF decoder "unnaf"

Prerequisites: git, C compiler, make, zstd library. (E.g., to install on Ubuntu: `sudo apt install git gcc make libzstd-dev`).

Installing:
```
git clone https://github.com/KirillKryukov/naf.git
cd naf/unnaf && make && make install
```

Using:
`unnaf -masked-fasta <file.naf >file.fasta`

See `unnaf -help` for more options.
