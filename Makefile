# Armageddon Recorder - Makefile
# Copyright (C) 2012 Daniel Collins <solemnwarning@solemnwarning.net>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

INCLUDES := -D_WIN32_WINNT=0x0501 -D_WIN32_IE=0x0300 -I./include/ -I../directx/
LIBS     := -static-libgcc -static-libstdc++ -lcomctl32 -lcomdlg32 -lole32 -lsndfile

CC       := gcc
CFLAGS   := -Wall -std=c99

CXX      := g++
CXXFLAGS := -Wall -std=c++0x

WINDRES  := windres

OBJS := src/main.o src/resource.o src/audio.o src/reg.o src/encode.o \
	src/capture.o src/ui.o

HDRS := src/main.hpp src/resource.h src/audio.hpp src/reg.hpp src/encode.hpp \
	src/capture.hpp src/ui.hpp src/resample.hpp

all: armageddon-recorder.exe dsound.dll dump.exe

clean:
	rm -f armageddon-recorder.exe $(OBJS)
	rm -f dsound.dll src/ds-capture.o
	rm -f dump.exe src/dump.o

armageddon-recorder.exe: $(OBJS)
	$(CXX) $(CXXFLAGS) -mwindows -o armageddon-recorder.exe $(OBJS) $(LIBS)
	strip -s armageddon-recorder.exe

dump.exe: src/dump.o
	$(CXX) $(CXXFLAGS) -o $@ $< -static-libgcc -static-libstdc++ -lsndfile

src/resource.o: src/resource.rc src/resource.h
	$(WINDRES) src/resource.rc src/resource.o

dsound.dll: src/ds-capture.o
	$(CC) $(CFLAGS) -Wl,--enable-stdcall-fixup -shared -o $@ $^

src/ds-capture.o: src/ds-capture.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

src/%.o: src/%.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<
