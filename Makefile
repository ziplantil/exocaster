
-include Makefile.inc

CC ?= gcc
CXX ?= g++
LD := $(CXX)
RM = rm -f

CUSTOM_CXXFLAGS ?= 
CUSTOM_LDFLAGS ?= 

ifeq ($(RELEASE),1)
CFLAGS=-O3 -DNDEBUG
CXXFLAGS=-O3 -DNDEBUG
LDFLAGS=-O3 -flto
else
CFLAGS=-g3 -O0 -Wall -Wextra -Werror -Wno-unused-parameter
CXXFLAGS=-g3 -O0 -Wall -Wextra -Werror -Wno-unused-parameter
LDFLAGS=
endif

CFLAGS := $(CFLAGS) -MMD -MP
CXXFLAGS := $(CXXFLAGS) -Iincludes -Isrc -Ivendor -std=c++20 -MMD -MP
LDFLAGS := $(LDFLAGS)
LDLIBS = -lm -lpthread

CXXFLAGS := $(CXXFLAGS) $(CUSTOM_CXXFLAGS)
LDFLAGS := $(LDFLAGS) $(CUSTOM_LDFLAGS)

TARGET := exocaster
OBJS := src/queue/queue.o \
		src/queue/commandqueue.o \
		src/queue/file.o \
		src/decoder/decoder.o \
		src/decoder/silence.o \
		src/decoder/testcard.o \
		src/encoder/encoder.o \
		src/encoder/pcm.o \
		src/broca/broca.o \
		src/broca/discard.o \
		src/broca/file.o \
		src/resampler/resampler.o \
		src/pcmbuffer.o \
		src/metadata.o \
		src/serverconfig.o \
		src/publisher.o \
		src/registry.o \
		src/server.o

# libraries
ifeq ($(LIBAVCODEC_ENABLE),1)
CXXFLAGS := $(CXXFLAGS) $(LIBAVCODEC_CXXFLAGS) -DEXO_LIBAVCODEC=1
LDLIBS := $(LDLIBS) $(LIBAVCODEC_LDLIBS)
OBJS := $(OBJS) \
		src/decoder/libavcodec/lavc.o \
		src/resampler/libswresample/lswr.o
endif

ifeq ($(LIBVORBIS_ENABLE),1)
CXXFLAGS := $(CXXFLAGS) $(LIBVORBIS_CXXFLAGS) -DEXO_LIBVORBIS=1
LDLIBS := $(LDLIBS) $(LIBVORBIS_LDLIBS)
OBJS := $(OBJS) \
 		src/encoder/libvorbis/oggvorbis.o
endif

ifeq ($(LIBFLAC_ENABLE),1)
CXXFLAGS := $(CXXFLAGS) $(LIBFLAC_CXXFLAGS) -DEXO_LIBFLAC=1
LDLIBS := $(LDLIBS) $(LIBFLAC_LDLIBS)
OBJS := $(OBJS) \
 		src/encoder/libflac/oggflac.o
endif

ifeq ($(LIBOPUS_ENABLE),1)
CXXFLAGS := $(CXXFLAGS) $(LIBOPUS_CXXFLAGS) -DEXO_LIBOPUS=1
LDLIBS := $(LDLIBS) $(LIBOPUS_LDLIBS)
OBJS := $(OBJS) \
 		src/encoder/libopus/oggopus.o
endif

ifeq ($(MP3LAME_ENABLE),1)
CXXFLAGS := $(CXXFLAGS) $(MP3LAME_CXXFLAGS) -DEXO_MP3LAME=1
LDLIBS := $(LDLIBS) $(MP3LAME_LDLIBS)
OBJS := $(OBJS) \
 		src/encoder/lame/mp3.o
endif

ifeq ($(SOXR_ENABLE),1)
CXXFLAGS := $(CXXFLAGS) $(SOXR_CXXFLAGS) -DEXO_SOXR=1
LDLIBS := $(LDLIBS) $(SOXR_LDLIBS)
OBJS := $(OBJS) \
		src/resampler/soxr/soxr.o
endif

ifeq ($(LIBSAMPLERATE_ENABLE),1)
CXXFLAGS := $(CXXFLAGS) $(LIBSAMPLERATE_CXXFLAGS) -DEXO_LIBSAMPLERATE=1
LDLIBS := $(LDLIBS) $(LIBSAMPLERATE_LDLIBS)
OBJS := $(OBJS) \
		src/resampler/libsamplerate/src.o
endif

ifeq ($(SHOUT_ENABLE),1)
CXXFLAGS := $(CXXFLAGS) $(SHOUT_CXXFLAGS) -DEXO_SHOUT=1
LDLIBS := $(LDLIBS) $(SHOUT_LDLIBS)
OBJS := $(OBJS) \
		src/broca/shout/shout.o
endif

ifeq ($(CURL_ENABLE),1)
CXXFLAGS := $(CXXFLAGS) $(CURL_CXXFLAGS) -DEXO_CURL=1
LDLIBS := $(LDLIBS) $(CURL_LDLIBS)
OBJS := $(OBJS) \
 		src/queue/curl/curl.o
endif

ifeq ($(ZEROMQ_ENABLE),1)
CXXFLAGS := $(CXXFLAGS) $(ZEROMQ_CXXFLAGS) -DEXO_ZEROMQ=1
LDLIBS := $(LDLIBS) $(ZEROMQ_LDLIBS)
OBJS := $(OBJS) \
 		src/queue/zeromq/zeromq.o
endif

ifeq ($(PORTAUDIO_ENABLE),1)
CXXFLAGS := $(CXXFLAGS) $(PORTAUDIO_CXXFLAGS) -DEXO_PORTAUDIO=1
LDLIBS := $(LDLIBS) $(PORTAUDIO_LDLIBS)
OBJS := $(OBJS) \
		src/broca/portaudio/playback.o
endif

DEPS := $(OBJS:.o=.d)

default: all

.PHONY: all clean
all: $(TARGET)
clean:
	$(RM) $(TARGET) $(OBJS) $(DEPS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

-include $(DEPS)
