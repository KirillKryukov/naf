# Nucleotide Archival Format (NAF)

NAF is a binary file format for biological sequence data.
It's based on [zstd](http://www.zstd.net/), and features strong compression and fast decompression.
It can store DNA, RNA, protein or text sequences, with or without qualities.
It supports FASTA and FASTQ-formatted sequences, ambiguous IUPAC codes, masked sequence,
and has no limit on sequence length or number of sequences.
It supports Unix pipes which allows easy integration into pipelines.
See [NAF homepage](http://kirill-kryukov.com/study/naf/) for details.

| Example benchmark: SILVA 132 LSURef database (610 MB): |
|---------------------------------------------|
| <img src="http://kirill-kryukov.com/study/naf/images/SILVA-132-LSURef-ratio-vs-cd-speed-lin-log.svg" width="49%"> <img src="http://kirill-kryukov.com/study/naf/images/SILVA-132-LSURef-ratio-vs-d-speed-lin-log.svg" width="49%"> |
| From [Sequence Compression Benchmark](http://kirr.dyndns.org/sequence-compression-benchmark/) project - visit for details and more benchmarks. |

More examples:
*   [Compactness on DNA data](http://kirr.dyndns.org/sequence-compression-benchmark/?d=Mitochondrion+%28245+MB%29&amp;d=Influenza+%281.22+GB%29&amp;d=Helicobacter+%282.76+GB%29&amp;doagg=1&amp;agg=average&amp;cs=1&amp;cg=1&amp;com=yes&amp;src=all&amp;nt=4&amp;only-best=1&amp;bn=1&amp;bm=ratio&amp;sm=same&amp;tn=10&amp;bs=100&amp;rr=gzip-9&amp;tm0=name&amp;tm1=size&amp;tm2=ratio&amp;tm3=ctime&amp;tm4=dtime&amp;tm5=cdtime&amp;tm6=tdtime&amp;tm7=empty&amp;gm=same&amp;cyl=lin&amp;ccw=1500&amp;cch=500&amp;sxm=ratio&amp;sxmin=0&amp;sxmax=0&amp;sxl=lin&amp;sym=dspeed&amp;symin=0&amp;symax=0&amp;syl=lin&amp;button=Show+column+chart)
*   [Compactness vs decompression speed, on human genome](http://kirr.dyndns.org/sequence-compression-benchmark/?d=Homo+sapiens+GCA_000001405.28+(3.31+GB)&amp;doagg=1&amp;agg=sum&amp;cs=1&amp;cg=1&amp;com=yes&amp;src=all&amp;nt=4&amp;bn=1&amp;bm=tdspeed&amp;sm=same&amp;tn=10&amp;bs=100&amp;rr=gzip-9&amp;tm0=name&amp;tm1=size&amp;tm2=ratio&amp;tm3=ctime&amp;tm4=dtime&amp;tm5=cdtime&amp;tm6=tdtime&amp;tm7=empty&amp;gm=same&amp;cyl=lin&amp;ccw=1500&amp;cch=500&amp;sxm=ratio&amp;sxmin=0&amp;sxmax=0&amp;sxl=lin&amp;sym=dspeed&amp;symin=0&amp;symax=0&amp;syl=lin&amp;button=Show+scatterplot)


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

## Compressing multiple files

Working with multiple files is possible using [Multi-Multi-FASTA](https://github.com/KirillKryukov/mumu) as intermediate format.
Example commands:

Compressing:<br>
`mumu.pl --dir 'Helicobacter' 'Helicobacter pylori*' | ennaf -22 --text -o Hp.nafnaf`

Decompressing and unpacking:<br>
`unnaf Hp.nafnaf | mumu.pl --unpack --dir 'Helicobacter'`

Filename of NAF-compressed single file normally ends with a ".naf".
To avoid ambiguity, **".nafnaf"** is the recommended suffix for multi-file NAF archives.

## Citation

If you use NAF, please cite:

 * Kirill Kryukov, Mahoko Takahashi Ueda, So Nakagawa, Tadashi Imanishi (2019)
**"Nucleotide Archival Format (NAF) enables efficient lossless reference-free compression of DNA sequences"**
[Bioinformatics, 35(19), 3826-3828](https://academic.oup.com/bioinformatics/article/35/19/3826/5364265),
doi: [10.1093/bioinformatics/btz144](https://doi.org/10.1093/bioinformatics/btz144).

For compressor benchmark, please cite:

 * Kirill Kryukov, Mahoko Takahashi Ueda, So Nakagawa, Tadashi Imanishi (2020)
**"Sequence Compression Benchmark (SCB) database — A comprehensive evaluation of reference-free compressors for FASTA-formatted sequences"**
[GigaScience, 9(7), giaa072](https://academic.oup.com/gigascience/article/9/7/giaa072/5867695),
doi: [10.1093/gigascience/giaa072](https://doi.org/10.1093/gigascience/giaa072).
