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
LIBS     := -static-libgcc -static-libstdc++ -lcomctl32 -lcomdlg32 -lole32 -lsndfile -lgorilla

CC       := gcc
CFLAGS   := -Wall

CXX      := g++
CXXFLAGS := -Wall

WINDRES  := windres

OBJS := src/main.o src/resource.o src/audio.o src/reg.o src/encode.o src/capture.o src/ui.o src/MurmurHash3.o
HDRS := src/main.hpp src/resource.h src/audio.hpp src/reg.hpp src/encode.hpp src/capture.hpp src/ui.hpp

all: armageddon-recorder.exe dsound.dll

clean:
	rm -f armageddon-recorder.exe $(OBJS)
	rm -f dsound.dll src/dsound_c.o src/dsound_s.o src/dsound.s

armageddon-recorder.exe: $(OBJS)
	$(CXX) $(CXXFLAGS) -mwindows -o armageddon-recorder.exe $(OBJS) $(LIBS)
	strip -s armageddon-recorder.exe

src/resource.o: src/resource.rc src/resource.h
	$(WINDRES) src/resource.rc src/resource.o

dsound.dll: src/dsound_c.o src/dsound_s.o src/MurmurHash3.o
	$(CC) $(CFLAGS) -Wl,--enable-stdcall-fixup -shared -o $@ $^

src/dsound_c.o: src/dsound.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

src/dsound.s: src/IDirectSound_hook.txt src/IDirectSoundBuffer_hook.txt
	perl mkhooks.pl IDirectSound > $@ < src/IDirectSound_hook.txt
	perl mkhooks.pl IDirectSoundBuffer >> $@ < src/IDirectSoundBuffer_hook.txt

src/dsound_s.o: src/dsound.s
	nasm -f win32 -o $@ $<

src/%.o: src/%.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<
