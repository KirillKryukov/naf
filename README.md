# Nucleotide Archival Format (NAF)
NAF is a binary file format for DNA sequence data.
It's based on zstd, and features strong compression and fast decompression.
It supports FASTA and FASTQ-formatted sequences, ambiguous IUPAC codes, masked sequence,
and has no limit on sequence length or number of sequences. See [NAF homepage](http://kirill-kryukov.com/study/naf/) for details and benchmarks.

## Format specification

NAF specification is in public domain: [NAFv1.pdf](NAFv1.pdf)

## Encoder and decoder

NAF encoder and decoder are called "ennaf" and "unnaf".
(After compressing your data with _ennaf_, you suddenly have _enough_ space.
However, if you decompress it back with _unnaf_, your space is again _un-enough_.)

## Installing

Prerequisites: git (for downloading), perl (for encoder), zstd, C compiler, make.
(E.g., to install on Ubuntu: `sudo apt install git perl gcc make zstd libzstd-dev`).

Installing:
```
git clone https://github.com/KirillKryukov/naf.git
cd naf && make && sudo make install
```

## Compressing

`ennaf --in-format fasta <file.fasta >file.naf`

## Decompressing

`unnaf -masked-fasta <file.naf >file.fasta`

See `unnaf -help` for more options.

## Reference

If you use NAF, feel free to cite our preprint (until peer-reviewed publication is out):

 * Kirill Kryukov, Mahoko Takahashi Ueda, So Nakagawa, Tadashi Imanishi (2018)
**"Nucleotide Archival Format (NAF) enables efficient lossless reference-free compression of DNA sequences"**
bioRxiv 501130; http://biorxiv.org/cgi/content/short/501130v1, doi: https://doi.org/10.1101/501130.
