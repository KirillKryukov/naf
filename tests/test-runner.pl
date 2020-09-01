#!/usr/bin/env perl
#
# Test runner script
# Copyright (c) 2018-2019 Kirill Kryukov
# See README.md and LICENSE files of this repository
#

use strict;
use File::Basename qw(basename dirname);
use File::Glob qw(:bsd_glob);

my @tests = @ARGV;
if (!scalar @tests) { die "Tests to run are not specified\n"; }

my $null = ($^O =~ /MSWin/) ? 'nul' : '/dev/null';
my ($n_tests_total, $n_tests_passed) = (0, 0);

foreach my $test (@tests)
{
    if (-e $test and -d $test) { run_test_set($test); }
    else { run_test($test); }
}

print "$n_tests_passed out of $n_tests_total tests passed\n";
if ($n_tests_passed != $n_tests_total) { exit 1; }


sub run_test_set
{
    my ($dir) = @_;
    print "===== $dir =====\n";
    if (!-e $dir or !-d $dir) { die "Can't find test set directory \"$dir\"\n"; }
    foreach my $file (bsd_glob("$dir/*.test")) { run_test($file); }
}

sub run_test
{
    my ($test_file) = @_;
    my ($dir, $name) = (dirname($test_file), basename($test_file));
    $name =~ s/\.test$//;
    my $test_prefix = "$dir/$name";
    my $group = $name;
    $group =~ s/-.*$//;
    my $group_prefix = "$dir/$group";

    print "[$dir/$name] ";
    $n_tests_total++;

    open(my $TEST, '<', $test_file) or die "Can't open \"$test_file\"\n";
    binmode $TEST;
    my @cmds;
    while (<$TEST>)
    {
        s/[\x0D\x0A]+$//;
        my $cmd = $_;
        $cmd =~ s/ennaf/..\/ennaf\/ennaf --binary-stderr/g;
        $cmd =~ s/unnaf/..\/unnaf\/unnaf --binary-stderr --binary-stdout/g;
        $cmd =~ s/\{TEST\}/$test_prefix/g;
        $cmd =~ s/\{GROUP\}/$group_prefix/g;
        push @cmds, $cmd;
        system($cmd);
    }
    close $TEST;

    my @errors;
    foreach my $ref_file (bsd_glob("$dir/$name.*-ref"))
    {
        my $out_file = $ref_file;
        $out_file =~ s/-ref$//;
        if (-e $out_file)
        {
            my $cmperr = system("diff -q $ref_file $out_file >$null");
            if ($cmperr != 0) { push @errors, "\"" . basename($out_file) . "\" differs from \"" . basename($ref_file) . "\""; }
        }
        else { push @errors, "Can't find output file \"$out_file\""; }
    }

    if (scalar(@errors) == 0)
    {
        print "OK\n";
        $n_tests_passed++;
    }
    else
    {
        print "FAIL\n";
        print "Commands:\n", join("\n", map { "    $_" } @cmds), "\n";
        print "Errors:\n", join("\n", map { "    $_" } @errors), "\n";
    }
}
