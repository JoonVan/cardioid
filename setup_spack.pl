#!/usr/bin/perl -w

use strict;

sub usage {
   print <<HERE
$0 yourenv

Makes a spack environment called yourenv and preloads it with all the
libraries taken from your path.

HERE
       ;
   die join("\n", @_) if @_;
}

usage("ERROR: Command only accepts one argument\n") if (scalar @ARGV != 1);
my ($env) = @ARGV;

usage("ERROR: Need to be able to run spack first.\nRun source spack/share/spack/setup-env.sh\n") if not findCommandDir("spack");

sub getCommandString {
   my ($command, $regex) = @_;

   open(my $perlhandle, "$command |") or die "Can't run $command: $!\n";;
   my $perlversion = "";
   while (my $line = <$perlhandle>)
   {
      if ($line =~ m,$regex,)
      {
         $perlversion = $1;
      }
   }
   close $perlhandle;

   return $perlversion;
}

sub getVersionString {
   my ($command, $versionPrelude) = @_;

   return getCommandString($command, $versionPrelude . "(\\d+\\.\\d+(?:\\.\\d+)?)");
}

sub which {
   my ($command) = @_;
   if (open (my $proghandle, "which $command |")) {
      my $fullCommand = <$proghandle>;
      $fullCommand = "" if not $fullCommand;
      chomp $fullCommand;
      close $proghandle;
      return $fullCommand;
   }
   return "";
}

sub findCommandDir {
   my ($command) = @_;

   my $fullCommand = which($command);
   $fullCommand =~ s,\/bin.*?$,,;
   return $fullCommand;
}

my $perldir = findCommandDir("perl");
my $perlversion = getVersionString("$perldir/bin/perl -v", "v") if $perldir;
my $cmakedir = findCommandDir("cmake");
my $cmakeversion = getVersionString("$cmakedir/bin/cmake --version", " ") if $cmakedir;

my $cudadir = findCommandDir("nvcc");
my $cudaversion;
$cudaversion = getVersionString("$cudadir/bin/nvcc --version", "V") if $cudadir;

my $mpitype = "";
my $mpidir = "";
my $mpiversion = "";
my $mpicompiler = "";

if ($mpidir = findCommandDir("mpichversion")) {
   $mpitype = "mpich";
   $mpiversion = getVersionString("$mpidir/bin/mpichversion", "MPICH Version:\\s+");
   $mpicompiler = which(getCommandString("$mpidir/bin/mpichversion", "MPICH CC:\\s+([A-Za-z0-9.@\/\-]+)"));
} elsif ($mpidir = findCommandDir("ompi_info")) {
   open(my $temp, "$mpidir/bin/ompi_info |");
   my @lines = <$temp>;
   close $temp;
   my $alllines = join("", @lines);
   if ($alllines =~ m/spectrum/i)
   {
      $mpitype = "spectrum";
      $mpiversion = getVersionString("$mpidir/bin/ompi_info", "Spectrum MPI:\\s+");
   } else {
      $mpitype = "openmpi";
      $mpiversion = getVersionString("$mpidir/bin/ompi_info", "Open MPI:\\s+");
   }
   $mpicompiler = which(getCommandString("$mpidir/bin/ompi_info", "C compiler:\\s+([A-Za-z0-9.@\/\-]+)"));
} elsif ($mpidir = findCommandDir("mpiname")) {
   $mpitype = "mvapich";
   $mpiversion = getVersionString("$mpidir/bin/mpiname -v", "");
   $mpicompiler = which(getCommandString("$mpidir/bin/mpiname -c", "^CC:\\s+([A-Za-z0-9.@\/\-]+)"));
} elsif ($mpidir = findCommandDir("impi_info")) {
   if ($mpidir =~ m/(\d+\.\d+(?:\.\d+)?)/) {
      $mpitype = "intel-mpi";
      $mpiversion = $1;
      $mpicompiler = which(getCommandString("$mpidir/bin/mpicc -show", "^([A-Za-z0-9.@\/\-]+)"));
   }
}

my $spackcompiler="";
if ($mpitype and findCommandDir("spack")) {
   #find the compiler
   #get the spack name for the compiler
   open(my $spackprog, "spack config get compilers |");
   my %nameFromCompiler;
   my $name = "";
   my $compiler = "";
   while (my $line = <$spackprog>) {
      if ($line =~ /^- compiler:/) {
         if ($name and $compiler) {
            $nameFromCompiler{$compiler} = $name;
         }
         $name = $compiler = "";
      }
      if ($line =~ m/cc:\s+(.*)$/) {
         $compiler = $1;
      } elsif ($line =~ m/spec:\s+(.*)$/) {
         $name = $1;
      }
   }
   if ($name and $compiler) {
      $nameFromCompiler{$compiler} = $name;
   }
   $spackcompiler = $nameFromCompiler{$mpicompiler};
}


open(my $envfile, ">", "arch/$env.yaml");

print $envfile <<HERE
# This is a Spack Environment file.
#
# It describes a set of packages to be installed, along with
# configuration settings.
spack:
  # add package specs to the `specs` list
  specs: []
  mirrors: {}
  modules:
    enable: []
  repos: []
  packages:
HERE
    ;
if ($cmakedir) {
   print $envfile <<HERE
    cmake:
        paths:
            cmake\@$cmakeversion: $cmakedir
        buildable: False
HERE
       ;
}
if ($perldir) {
   print $envfile <<HERE
    perl:
        paths:
            perl\@$perlversion: $perldir
        buildable: False
HERE
       ;
}
if ($cudadir) {
   print $envfile <<HERE
    cuda:
        paths:
            cuda\@$cudaversion: $cudadir
        buildable: False
HERE
       ;
}
if ($mpitype and $spackcompiler) {
   print $envfile <<HERE
    $mpitype:
        compiler: [$spackcompiler]
        version: [$mpiversion]
        paths:
            $mpitype\@$mpiversion\%$spackcompiler: $mpidir
        buildable: False
HERE
       ;
}
print $envfile <<HERE
    all:
      providers:
        blas: [netlib-lapack]
        lapack: [netlib-lapack]
HERE
    ;
print $envfile "        mpi: [$mpitype]\n" if $mpitype and $spackcompiler;
print $envfile <<HERE
  config: {}
HERE
       ;

#now, create a spack environment
system("spack env create $env arch/$env.yaml");
    
