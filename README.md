# Nucleotide Archival Format (NAF)

NAF is a binary file format for DNA sequence data.
It's based on [zstd](http://www.zstd.net/), and features strong compression and fast decompression.
It supports FASTA and FASTQ-formatted sequences, ambiguous IUPAC codes, masked sequence,
and has no limit on sequence length or number of sequences. See [NAF homepage](http://kirill-kryukov.com/study/naf/) for details and benchmarks.

| Example benchmark: SILVA Database (948 MB): |
|---------------------------------------------|
| <img src="http://kirill-kryukov.com/study/naf/images/SILVA-ratio-vs-cd-speed-lin.svg" width="49%" height="49%"> <img src="http://kirill-kryukov.com/study/naf/images/SILVA-ratio-vs-d-speed-lin.svg" width="49%" height="49%"> |


## Format specification

NAF specification is in public domain: [NAFv2.pdf](NAFv2.pdf)

## Encoder and decoder

NAF encoder and decoder are called "ennaf" and "unnaf".
After compressing your data with _ennaf_, you suddenly have _enough_ space.
However, if you decompress it back with _unnaf_, your space is again _un-enough_.

## Installing

### Binaries

Check if the [latest release](https://github.com/KirillKryukov/naf/releases) has a build for your platform.

### Building from source

Prerequisites: git, gcc, make, diff, perl (diff and perl are only used for test suite).
E.g., to install on Ubuntu: `sudo apt install git gcc make diffutils perl`.
On Mac OS you may have to install Xcode Command Line Tools.

Building and installing:

```
git clone --recurse-submodules https://github.com/KirillKryukov/naf.git
cd naf && make && make test && sudo make install
```

To install in alternative location, add "prefix=DIR" to the "make install" command. E.g., `sudo make prefix=/usr/local/bio install`

For a staged install, add "DESTDIR=DIR". E.g., `make DESTDIR=/tmp/stage install`

On Windows it can be installed using [Cygwin](https://www.cygwin.com/),
and should be also possible with [WSL](https://docs.microsoft.com/en-us/windows/wsl/install-win10).
In Cygwin drop `sudo`: `cd naf && make && make test && make install`

### Building from latest unreleased source

For testing purpose only:
```
git clone --recurse-submodules --branch develop https://github.com/KirillKryukov/naf.git
cd naf && make && make test && sudo make install
```

## Compressing

`ennaf file.fa -o file.naf`

See `ennaf -h` and [Compression Manual](Compress.md) for detailed usage.

## Decompressing

`unnaf file.naf -o file.fa`

See `unnaf -h` and [Decompression Manual](Decompress.md).

## Reference

If you use NAF, please cite:

 * Kirill Kryukov, Mahoko Takahashi Ueda, So Nakagawa, Tadashi Imanishi (2019)
**"Nucleotide Archival Format (NAF) enables efficient lossless reference-free compression of DNA sequences"**
[Bioinformatics (in press), btz144](https://academic.oup.com/bioinformatics/advance-article/doi/10.1093/bioinformatics/btz144/5364265),
doi: [10.1093/bioinformatics/btz144](https://doi.org/10.1093/bioinformatics/btz144).

Previous preprint: bioRxiv 501130; http://biorxiv.org/cgi/content/short/501130v2, doi: [10.1101/501130](https://doi.org/10.1101/501130).
