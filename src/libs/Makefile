HOME = ../..
COREIR = $(HOME)/../coreir
LPATH = -L$(HOME)/lib
INCS = -I$(HOME)/include -I. -I$(COREIR)/include
SRCFILES = $(wildcard [^_]*.cpp)
OBJS = $(patsubst %.cpp,build/%.o,$(SRCFILES))

DYLIBS = $(patsubst %.cpp,build/%.dylib,$(SRCFILES))
SOLIBS = $(patsubst %.cpp,build/%.so,$(SRCFILES))

all: $(DYLIBS) $(SOLIBS)

so: $(SOLIBS)

dylib: $(DYLIBS)

clean:
	rm -rf build/*

build/%.so: build/%.o
	$(CXX) -shared $(LPATH) $(LDFLAGS) -o $@ $^ -lcoreir -static-libgcc -static-libstdc++
	cp $@ $(HOME)/lib/libcoreir-$*.so

build/%.dylib: build/%.o
	$(CXX) -install_name "@rpath/libcoreir-$*.dylib" -dynamiclib $(LPATH) $(LDFLAGS) -lcoreir -o $@ $^
	cp $@ $(HOME)/lib/libcoreir-$*.dylib

build/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCS) -c -o $@ $<
