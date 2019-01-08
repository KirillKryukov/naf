#!/usr/bin/env perl
#
# Prototype NAF compressor.
# Should not be used anymore since it's now superseded by a C version.
# Left for reference only.
#
#
# ennaf - Converter from FASTA/FASTQ into NAF format
#
# Version 0.1.2 (October 9, 2018)
#
# Copyright (c) 2018 Kirill Kryukov
#
# This software is provided 'as-is', without any express or implied
# warranty. In no event will the authors be held liable for any damages
# arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
#
# 1. The origin of this software must not be misrepresented; you must not
#    claim that you wrote the original software. If you use this software
#    in a product, an acknowledgment in the product documentation would be
#    appreciated but is not required.
# 2. Altered source versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.
# 3. This notice may not be removed or altered from any source distribution.
#

use strict;
use integer;
use File::Basename qw(basename);
use Getopt::Long;
use Time::HiRes qw(gettimeofday tv_interval);

my $t0 = [gettimeofday];

my @nuc_code;
init_tables();

my ($in_file, $dataset_name, $in_format,
    $out_file, $out_4bit_file, $out_ids_file, $out_names_file, $out_lengths_file,
    $out_mask_file, $out_dna_file, $out_qual_file,
    $temp_dir, $keep_temp_files,
    $title, $name_separator, $skip_separators,
    $no_mask, $compression_level,
    $skip_zstd_check, $verbose, $help, $ver);
GetOptions(
    'in=s'              => \$in_file,
    'name=s'            => \$dataset_name,
    'in-format=s'       => \$in_format,
    'out=s'             => \$out_file,
    'out-4bit=s'        => \$out_4bit_file,
    'out-ids=s'         => \$out_ids_file,
    'out-names=s'       => \$out_names_file,
    'out-lengths=s'     => \$out_lengths_file,
    'out-mask=s'        => \$out_mask_file,
    'out-dna=s'         => \$out_dna_file,
    'out-quality=s'     => \$out_qual_file,
    'temp-dir=s'        => \$temp_dir,
    'keep-temp-files'   => \$keep_temp_files,
    'title=s'           => \$title,
    'name-separator=s'  => \$name_separator,
    'skip-separators=i' => \$skip_separators,
    'no-mask'           => \$no_mask,
    'level=i'           => \$compression_level,
    'skip-zstd-check'   => \$skip_zstd_check,
    'verbose' => \$verbose,
    'help'    => \$help,
    'version' => \$ver)
or die "Can't parse command line arguments\n";

my ($IN, $temp_name);
if (defined $in_file)
{
    if (!defined $in_format)
    {
        if ($in_file =~ /\.(fa|fna|fasta)$/i) { $in_format = 'fasta'; }
        elsif ($in_file =~ /\.(fq|fastq)$/i) { $in_format = 'fastq'; }
        else { die "Input format is not specified, and unknown file extension\n"; }
    }

    if (!-e $in_file) { die "Can't find input file \"$in_file\"\n"; }
    if (!-f $in_file) { die "Input is not a file\n"; }
    open ($IN, '<', $in_file) or die "Can't open \"$in_file\"\n";

    $temp_name = basename($in_file);
}
else
{
    if (!defined $in_format) { die "Input format is not specified\n"; }
    $IN = *STDIN{IO};
    $temp_name = defined($dataset_name) ? $dataset_name : ($$ . '-' . int(rand(4294967296)));
}
binmode $IN;

$in_format = lc($in_format);
if ($in_format ne 'fasta' and $in_format ne 'fastq')
{
    die "Unknown input format requested. Supported formats are: \"fasta\" and \"fastq\".\n";
}

if (!defined $temp_dir)
{
    if (exists $ENV{'TMPDIR'}) { $temp_dir = $ENV{'TMPDIR'}; }
    elsif (exists $ENV{'TMP'}) { $temp_dir = $ENV{'TMP'}; }
    else { die "Temp directory is not specified, and can't autodetect\nPlease either set TMPDIR or TMP environment variable, or add '--temp-dir DIR' to command line.\n"; }
}
$temp_dir =~ s/[\\\/]+$//;
if (!-e $temp_dir or !-d $temp_dir) { die "Can't find temporary directory\n"; }

if (!$skip_zstd_check) { test_zstd(); }

if (!defined $compression_level) { $compression_level = 22; }
$compression_level = int($compression_level);
if ($compression_level < 1) { $compression_level = 1; }
if ($compression_level > 22) { $compression_level = 22; }
my $zstd_command = 'zstd' . (($compression_level > 19) ? ' --ultra' : '') . " -$compression_level -c";

if (!defined($name_separator) or $name_separator eq '' or length($name_separator) > 1) { $name_separator = ' '; }

if (!defined $skip_separators) { $skip_separators = 0; }
$skip_separators = int($skip_separators);
if ($skip_separators < 0) { $skip_separators = 0; }

my $store_title = defined($title);
my $store_ids = 1;
my $store_names = 1;
my $store_lengths = 1;
my $store_mask = not defined($no_mask);
my $store_data = 1;
my $store_quality = ($in_format eq 'fastq');

my $format_version = 1;
my $format_descriptor = pack('C', 0x01) . pack('C', 0xF9) . pack('C', 0xEC) . pack('C', $format_version);

my $load_mask = $store_mask or defined($out_mask_file);

my $lock_temp_file    = "$temp_dir/$temp_name.lock";
my $ids_temp_file     = "$temp_dir/$temp_name.ids";
my $names_temp_file   = "$temp_dir/$temp_name.names";
my $lengths_temp_file = "$temp_dir/$temp_name.lengths";
my $mask_temp_file    = "$temp_dir/$temp_name.mask";
my $data_temp_file    = "$temp_dir/$temp_name.data";
my $qual_temp_file    = "$temp_dir/$temp_name.quality";




open(my $LOCK, '>', $lock_temp_file) or die "Can't create lock file\n";

my $OUT;
if (defined $out_file) { open($OUT, '>', $out_file) or die "Can't create output file\n"; }
else { $OUT = *STDOUT{IO}; }
binmode $OUT;
print $OUT $format_descriptor;

my (@ids, @names, @data_lengths, @mask_lengths);
my ($N, $mask_on, $mask_len, $line_count, $max_line_length) = (0, 0, 0, 0, 0);
my ($chunk_size, $chunk, $byte, $parity) = (16384, '', 0, 0);
my ($TMPDATA, $TMPQUAL, $OUT4BIT, $OUTDNA, $OUTQUAL);

if (defined $out_4bit_file) { open($OUT4BIT, '>', $out_4bit_file) or die "Can't create 4bit file\n"; binmode $OUT4BIT; }
if (defined $out_dna_file) { open($OUTDNA, '>', $out_dna_file) or die "Can't create dna file\n"; binmode $OUTDNA; }
if (defined $out_qual_file) { open($OUTQUAL, '>', $out_qual_file) or die "Can't create quality file\n"; binmode $OUTQUAL; }

if ($store_data)
{
    open($TMPDATA, '|-', "$zstd_command >\"$data_temp_file\"") or die "Can't create compressed data file\n";
    binmode $TMPDATA;
}

if ($store_quality)
{
    open($TMPQUAL, '|-', "$zstd_command >\"$qual_temp_file\"") or die "Can't create compressed quality file\n";
    binmode $TMPQUAL;
}

if ($in_format eq 'fasta')
{
    while (<$IN>)
    {
        s/[\x0D\x0A]+$//;
        if (substr($_, 0, 1) eq '>')
        {
            my ($id, $name) = split_name($_);
            $ids[$N] = $id;
            $names[$N] = $name;
            $data_lengths[$N] = 0;
            $N++;
        }
        else
        {
            s/\s//g;
            if ($OUTDNA) { print $OUTDNA uc($_); }
            $data_lengths[$N - 1] += length($_);
            add_mask_from($_);
            encode_dna($_);
        }
    }
}
elsif ($in_format eq 'fastq')
{
    while (!eof $IN)
    {
        my ($full_name, $seq, $qual) = get_fastq_entry();

        if ($OUTDNA) { print $OUTDNA uc($seq); }
        if ($OUTQUAL) { print $OUTQUAL $qual; }
        if ($TMPQUAL) { print $TMPQUAL $qual; }

        my ($id, $name) = split_name($full_name);
        $ids[$N] = $id;
        $names[$N] = $name;
        $data_lengths[$N] = length($seq);
        $N++;

        add_mask_from($seq);
        encode_dna($seq);
    }
}
else { die "Unsupported format\n"; }

if ($parity) { $chunk .= pack('C', $byte); }
if (length($chunk) > 0) { save_4bit_chunk(); }

if ($mask_len > 0) { push @mask_lengths, $mask_len; }

for ($OUTDNA, $OUTQUAL, $OUT4BIT, $TMPDATA, $TMPQUAL) { if ($_) { close $_; } }



my $total_length = 0;
foreach my $len (@data_lengths) { $total_length += $len; }
my $total_mask_length = 0;
foreach my $len (@mask_lengths) { $total_mask_length += $len; }
if ($load_mask and $total_mask_length != $total_length) { die "Total mask length ($total_mask_length) doesn't match total sequence length ($total_length)\n"; }

if ($verbose) { print STDERR commify($N), " sequences\n"; }

my ($ids_size, $names_size) = (0, 0);
for (@ids) { $ids_size += length($_) + 1; }
for (@names) { $names_size += length($_) + 1; }



if (defined $out_ids_file)
{
    open(my $F,'>', $out_ids_file) or die "Can't create IDs file\n";
    binmode $F;
    for (@ids) { print $F $_, pack('C', 0); }
    close $F;
}

if (defined $out_names_file)
{
    open(my $F,'>', $out_names_file) or die "Can't create names file\n";
    binmode $F;
    for (@names) { print $F $_, pack('C', 0); }
    close $F;
}

if (defined $out_lengths_file)
{
    open(my $F,'>', $out_lengths_file) or die "Can't create lengths file\n";
    binmode $F;
    foreach my $len (@data_lengths)
    {
        my $a = $len;
        while ($a >= 4294967295) { print $F pack('L<', 4294967295); $a -= 4294967295; }
        print $F pack('L<', $a);
    }
    close $F;
}

if (defined $out_mask_file)
{
    open(my $F,'>', $out_mask_file) or die "Can't create mask file\n";
    binmode $F;
    foreach my $len (@mask_lengths)
    {
        my $a = $len;
        while ($a >= 255) { print $F pack('C', 255); $a -= 255; }
        print $F pack('C', $a);
    }
    close $F;
}

if (scalar(@mask_lengths) <= 1) { $store_mask = 0; }
my $flags = ($store_title << 6) | ($store_ids << 5) | ($store_names << 4) | ($store_lengths << 3) | ($store_mask << 2) | ($store_data << 1) | $store_quality;

print $OUT pack('C', $flags);
print $OUT pack('C', ord($name_separator));
print $OUT encode_number($max_line_length);
print $OUT encode_number($N);

if ($store_title)
{
    print $OUT encode_number(length($title)), $title;
}

if ($store_ids)
{
    open(my $TMP, '|-', "$zstd_command >\"$ids_temp_file\"") or die "Can't create compressed IDs file\n";
    binmode $TMP;
    for (@ids) { print $TMP $_, pack('C', 0); }
    close $TMP;

    my $compressed_ids_size = -s $ids_temp_file;
    if ($compressed_ids_size < 5) { die "Failed to create compressed IDs file\n"; }
    $compressed_ids_size -= 4;
    print_ratio('IDs', $ids_size, $compressed_ids_size);

    print $OUT encode_number($ids_size);
    print $OUT encode_number($compressed_ids_size);

    print_file_tail($ids_temp_file, 4, $OUT);
}

@ids = ();

if ($store_names)
{
    open(my $TMP, '|-', "$zstd_command >\"$names_temp_file\"") or die "Can't create compressed names file\n";
    binmode $TMP;
    for (@names) { print $TMP $_, pack('C', 0); }
    close $TMP;

    my $compressed_names_size = -s $names_temp_file;
    if ($compressed_names_size < 5) { die "Failed to create compressed names file\n"; }
    $compressed_names_size -= 4;
    print_ratio('Names', $names_size, $compressed_names_size);

    print $OUT encode_number($names_size);
    print $OUT encode_number($compressed_names_size);

    print_file_tail($names_temp_file, 4, $OUT);
}

@names = ();

if ($store_lengths)
{
    my $lengths_size = 0;
    open(my $TMP, '|-', "$zstd_command >\"$lengths_temp_file\"") or die "Can't create compressed lengths file\n";
    binmode $TMP;
    foreach my $len (@data_lengths)
    {
        my $a = $len;
        while ($a >= 4294967295) { print $TMP pack('L<', 4294967295); $a -= 4294967295; $lengths_size += 4; }
        print $TMP pack('L<', $a); $lengths_size += 4;
    }
    close $TMP;

    my $compressed_lengths_size = -s $lengths_temp_file;
    if ($compressed_lengths_size < 5) { die "Failed to create compressed names file\n"; }
    $compressed_lengths_size -= 4;
    print_ratio('Lengths', $lengths_size, $compressed_lengths_size);

    print $OUT encode_number($lengths_size);
    print $OUT encode_number($compressed_lengths_size);

    print_file_tail($lengths_temp_file, 4, $OUT);
}

@data_lengths = ();

if ($store_mask)
{
    my $mask_size = 0;
    open(my $TMP, '|-', "$zstd_command >\"$mask_temp_file\"") or die "Can't create compressed mask file\n";
    binmode $TMP;
    foreach my $len (@mask_lengths)
    {
        my $a = $len;
        while ($a >= 255) { print $TMP pack('C', 255); $a -= 255; $mask_size++; }
        print $TMP pack('C', $a); $mask_size++;
    }
    close $TMP;

    my $compressed_mask_size = -s $mask_temp_file;
    if ($compressed_mask_size < 5) { die "Failed to create compressed mask file\n"; }
    $compressed_mask_size -= 4;
    print_ratio('Mask', $mask_size, $compressed_mask_size);

    print $OUT encode_number($mask_size);
    print $OUT encode_number($compressed_mask_size);

    print_file_tail($mask_temp_file, 4, $OUT);
}

@mask_lengths = ();

if ($store_data)
{
    my $compressed_data_size = -s $data_temp_file;
    if ($compressed_data_size < 5) { die "Failed to create compressed data file\n"; }
    $compressed_data_size -= 4;
    print_ratio('Data', $total_length, $compressed_data_size);

    print $OUT encode_number($total_length);
    print $OUT encode_number($compressed_data_size);

    print_file_tail($data_temp_file, 4, $OUT);
}

if ($store_quality)
{
    my $compressed_qual_size = -s $qual_temp_file;
    if ($compressed_qual_size < 5) { die "Failed to create compressed quality file\n"; }
    $compressed_qual_size -= 4;
    print_ratio('Quality', $total_length, $compressed_qual_size);

    print $OUT encode_number($total_length);
    print $OUT encode_number($compressed_qual_size);

    print_file_tail($qual_temp_file, 4, $OUT);
}

if (defined $out_file) { close $OUT; }

if (!$keep_temp_files)
{
    if (-e $ids_temp_file) { unlink $ids_temp_file; }
    if (-e $names_temp_file) { unlink $names_temp_file; }
    if (-e $lengths_temp_file) { unlink $lengths_temp_file; }
    if (-e $mask_temp_file) { unlink $mask_temp_file; }
    if (-e $data_temp_file) { unlink $data_temp_file; }
    if (-e $qual_temp_file) { unlink $qual_temp_file; }
}

close $LOCK;
if (!$keep_temp_files) { unlink $lock_temp_file; }

my $elapsed = tv_interval($t0);
if ($verbose) { print STDERR sprintf("%.3f sec\n", $elapsed); }



sub save_4bit_chunk
{
    if ($OUT4BIT) { print $OUT4BIT $chunk; }
    if ($TMPDATA) { print $TMPDATA $chunk; }
}

sub encode_dna
{
    my ($dna) = @_;
    my $len = length($dna);
    if ($len > $max_line_length) { $max_line_length = $len; }

    for (my $j = 0; $j < $len; $j++)
    {
        my $c = ord(substr($dna, $j, 1));
        if ($parity)
        {
            $chunk .= pack('C', $byte + $nuc_code[$c] * 16);
            if (length($chunk) >= $chunk_size) { save_4bit_chunk(); $chunk = ''; }
            $parity = 0;
        }
        else
        {
            $byte = $nuc_code[$c];
            $parity = 1;
        }
    }
}

sub get_fastq_entry
{
    my $truncated = "Truncated FASTQ file\n";

    my $name = <$IN>;
    $line_count++;
    if (substr($name, 0, 1) ne '@') { die "Error parsing input line $line_count: FASTQ entry does not start with '\@':\n$name"; }
    $name =~ s/[\x0D\x0A]+$//;

    if (eof $IN) { print STDERR $truncated; return ('', '', ''); }
    my $seq = <$IN>;
    $line_count++;
    $seq =~ s/[\x0D\x0A]+$//;
    my $slen = length($seq);

    if (eof $IN) { print STDERR $truncated; return ('', '', ''); }
    my $plus_line = <$IN>;
    $line_count++;
    if (substr($plus_line, 0, 1) ne '+') { die "Error parsing input line $line_count: Expecting '+' as first character, found '$plus_line'"; }
    my $name2 = substr($plus_line, 1);
    $name2 =~ s/[\x0D\x0A]+$//;
    my $n2len = length($name2);
    if ($verbose and $name2 ne '' and $name2 ne $name) { print STDERR "Warning: Misformatted FASTQ entry in input line $line_count: Sequence identifiers don't match:\n\@$name\n+$name2\n"; }

    if (eof $IN) { print STDERR $truncated; return ('', '', ''); }
    my $qual = <$IN>;
    $line_count++;
    $qual =~ s/[\x0D\x0A]+$//;
    my $qlen = length($qual);
    if ($qlen != $slen)
    {
        if (eof($IN)) { print STDERR $truncated; }
        elsif ($verbose) { print STDERR "Warning: Misformatted FASTQ entry in input line $line_count: quality length ($qlen) differs from sequence length ($slen):\n$seq\n$qual\n"; }
        return ('', '', '');
    }

    return ($name, $seq, $qual);
}

sub split_name
{
    my ($full_name) = @_;

    my $sep_num = -1;
    my $sep_pos = 0;
    while ($sep_num < $skip_separators and $sep_pos >= 0)
    {
        $sep_pos = index($full_name, $name_separator, $sep_pos + 1);
        $sep_num++;
    }

    if ($sep_pos >= 0)
    {
        return (substr($full_name, 1, $sep_pos - 1), substr($full_name, $sep_pos + 1));
    }
    else
    {
        return (substr($full_name, 1), '');
    }
}

sub add_mask_from
{
    my ($seq) = @_;
    my $len = length($seq);

    if ($load_mask)
    {
        my $i = 0;
        while ($i < $len)
        {
            if ($mask_on != (ord(substr($seq, $i, 1)) >= 96))
            {
                push @mask_lengths, $mask_len;
                $mask_len = 0;
                $mask_on = (ord(substr($seq, $i, 1)) >= 96);
            }

            my $start = $i;
            if ($mask_on) { while ($i < $len and ord(substr($seq, $i, 1)) >= 96) { $i++; } }
            else { while ($i < $len and ord(substr($seq, $i, 1)) < 96) { $i++; } }
            $mask_len += $i - $start;
        }
    }
}

sub print_file_tail
{
    my ($file, $pos, $OUTFILE) = @_;
    my $file_size = -s $file;
    my $remaining_bytes = $file_size - $pos;
    my $chunk_size = 131072;
    open(my $F, '<', $file) or die "Can't open \"$file\"\n";
    binmode $F;
    seek($F, $pos, 0);
    while ($remaining_bytes >= $chunk_size)
    {
        my $buffer;
        my $could_read = read($F, $buffer, $chunk_size);
        if ($could_read != $chunk_size) { die "Can't read $chunk_size bytes from \"$file\"\n"; }
        print $OUT $buffer;
        $remaining_bytes -= $chunk_size;
    }
    if ($remaining_bytes > 0)
    {
        my $buffer;
        my $could_read = read($F, $buffer, $remaining_bytes);
        if ($could_read != $remaining_bytes) { die "Can't read $remaining_bytes bytes from \"$file\"\n"; }
        print $OUT $buffer;
    }
    close $F;
}

sub init_tables
{
    @nuc_code = (15) x 256;

    $nuc_code[ord('A')] = 8;  $nuc_code[ord('a')] = 8;
    $nuc_code[ord('C')] = 4;  $nuc_code[ord('c')] = 4;
    $nuc_code[ord('G')] = 2;  $nuc_code[ord('g')] = 2;
    $nuc_code[ord('T')] = 1;  $nuc_code[ord('t')] = 1;
    $nuc_code[ord('U')] = 1;  $nuc_code[ord('u')] = 1;
    $nuc_code[ord('R')] = 10; $nuc_code[ord('r')] = 10;
    $nuc_code[ord('Y')] = 5;  $nuc_code[ord('y')] = 5;
    $nuc_code[ord('S')] = 6;  $nuc_code[ord('s')] = 6;
    $nuc_code[ord('W')] = 9;  $nuc_code[ord('w')] = 9;
    $nuc_code[ord('K')] = 3;  $nuc_code[ord('k')] = 3;
    $nuc_code[ord('M')] = 12; $nuc_code[ord('m')] = 12;
    $nuc_code[ord('B')] = 7;  $nuc_code[ord('b')] = 7;
    $nuc_code[ord('D')] = 11; $nuc_code[ord('d')] = 11;
    $nuc_code[ord('H')] = 13; $nuc_code[ord('h')] = 13;
    $nuc_code[ord('V')] = 14; $nuc_code[ord('v')] = 14;
}

sub print_ratio
{
    my ($what, $size, $compsize) = @_;
    if (!$verbose) { return; }
    print STDERR $what, ': ', commify($size), ' => ', commify($compsize);
    {
        no integer;
        print STDERR ' (', sprintf('%.2f', $compsize / $size * 100), "%)\n";
    }
}

sub encode_number
{
    my ($a) = @_;
    my $b = pack('C', $a & 127);
    $a >>= 7;
    while ($a > 0) { $b = pack('C', 128 | ($a & 127)) . $b; $a >>= 7; }
    return $b;
}

sub commify
{
    my $text = reverse $_[0];
    $text =~ s/(\d\d\d)(?=\d)(?!\d*\.)/$1,/g;
    return scalar reverse $text;
}

sub test_zstd
{
    my $ver = qx(zstd --version 2>&1);
    if ($? != 0) { die "Can't find zstd command - please install Zstandard\n"; }
    if ($ver !~ 'zstd command line') { die "Unknown or incompatible version of zstd\n"; }
}
