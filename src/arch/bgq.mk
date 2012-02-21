CXX=/bgsys/drivers/ppcfloor/comm/xl/bin/mpixlcxx_r
CC=/bgsys/drivers/ppcfloor/comm/xl/bin/mpixlc_r
LD=$(CXX)

DFLAGS = -DWITH_PIO -DWITH_MPI -DBGQ \
	 -DADD_ -DUSE_CSTDIO_LFS -DMPICH_IGNORE_CXX_SEEK

INCLUDE = -I/usr/gapps/emhm/include
OPTFLAGS = -g -O3
CFLAGS_BASE =   -qsmp=omp $(INCLUDE) $(DFLAGS)
CXXFLAGS_BASE = -qsmp=omp $(INCLUDE) $(DFLAGS)
LDFLAGS_BASE = -lc -lnss_files -lnss_dns -lresolv

HAVE_GSL = 1
ifeq ($(HAVE_GSL),1) 
   CFLAGS_BASE  += -DHAVE_GSL
   CXXFLAGS_BASE  += -DHAVE_GSL
   LDFLAGS_BASE += -L/usr/gapps/emhm/lib/gsl-1.15-bgq -lgsl -lgslcblas
endif

CFLAGS_OPT =   $(CFLAGS_BASE) -g -O3  
CFLAGS_DEBUG = $(CFLAGS_BASE) -g -O0
CFLAGS_PROF =  $(CFLAGS_BASE) -g -pg -O3 -DPROFILE

CXXFLAGS_OPT =   $(CXXFLAGS_BASE) -g -O3  
CXXFLAGS_DEBUG = $(CXXFLAGS_BASE) -g -O0
CXXFLAGS_PROF =  $(CXXFLAGS_BASE) -g -pg -O3 -DPROFILE


LDFLAGS_OPT   = $(LDFLAGS_BASE) $(CFLAGS_OPT) $(CXXFLAGS_OPT)
LDFLAGS_DEBUG = $(LDFLAGS_BASE) $(CFLAGS_DEBUG) $(CXXFLAGS_DEBUG)
LDFLAGS_PROF  = $(LDFLAGS_BASE) $(CFLAGS_PROF) $(CXXFLAGS_PROF)
