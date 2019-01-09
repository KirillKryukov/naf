# Nucleotide Archival Format (NAF)
NAF is a binary file format for DNA sequence data.
It's based on [zstd](http://www.zstd.net/), and features strong compression and fast decompression.
It supports FASTA and FASTQ-formatted sequences, ambiguous IUPAC codes, masked sequence,
and has no limit on sequence length or number of sequences. See [NAF homepage](http://kirill-kryukov.com/study/naf/) for details and benchmarks.

## Format specification

NAF specification is in public domain: [NAFv1.pdf](NAFv1.pdf)

## Encoder and decoder

NAF encoder and decoder are called "ennaf" and "unnaf".
(After compressing your data with _ennaf_, you suddenly have _enough_ space.
However, if you decompress it back with _unnaf_, your space is again _un-enough_.)

## Installing

Prerequisites: git (for downloading), zstd, gcc, make.
(E.g., to install on Ubuntu: `sudo apt install git gcc make zstd libzstd-dev`).

Installing:
```
git clone https://github.com/KirillKryukov/naf.git
cd naf && make && sudo make install
```

To install in alternative location, add "prefix=DIR" to the "make install" command. E.g., `sudo make prefix=/usr/local/bio install`

For a staged install, add "DESTDIR=DIR". E.g., `make DESTDIR=/tmp/stage install`

On Windows it can be installed using [cygwin](https://www.cygwin.com/),
and should be also possible with [WSL](https://docs.microsoft.com/en-us/windows/wsl/install-win10).

## Compressing

`ennaf <file.fasta >file.naf`

See `ennaf --help` and [Compression Manual](Compress.md) for detailed usage.

## Decompressing

`unnaf file.naf >file.fasta`

See `unnaf --help` for options.

## Reference

If you use NAF, feel free to cite our preprint:

 * Kirill Kryukov, Mahoko Takahashi Ueda, So Nakagawa, Tadashi Imanishi (2018)
**"Nucleotide Archival Format (NAF) enables efficient lossless reference-free compression of DNA sequences"**
bioRxiv 501130; http://biorxiv.org/cgi/content/short/501130v1, doi: https://doi.org/10.1101/501130.
