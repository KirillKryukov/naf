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

`ennaf --fasta <file.fasta >file.naf`

Compression process stores temporary data on disk.
Therefore please check the following before compressing large files:

1. Temporary directory is specified in TMPDIR or TMP environment variable, or in --temp-dir command line option of your ennaf command.
 Note that ennaf doesn't have a default setting for temporary directory, such as "/tmp", it will only use directory specified in the environment or command line.
1. Temporary directory is on your fastest SSD drive.
1. Temporary directory has sufficient space to hold the compressed data.
 About 1/4 of the uncompressed data size should be normally fine, but safer to have 1/2 or more.
 Note that storage devices and filesystems can slow down when nearly full, so having extra free temporary space is generally a good idea.

Compression strength can be controlled with "--level N", where N is from 1 to 22.
This corresponds to zstd compression levels. 1 is the fastest with moderate compression. 22 is slow but gives the best compression.
22 is the default setting.

## Decompressing

`unnaf --masked-fasta <file.naf >file.fasta`

See `unnaf --help` for more options.

## Reference

If you use NAF, feel free to cite our preprint:

 * Kirill Kryukov, Mahoko Takahashi Ueda, So Nakagawa, Tadashi Imanishi (2018)
**"Nucleotide Archival Format (NAF) enables efficient lossless reference-free compression of DNA sequences"**
bioRxiv 501130; http://biorxiv.org/cgi/content/short/501130v1, doi: https://doi.org/10.1101/501130.
