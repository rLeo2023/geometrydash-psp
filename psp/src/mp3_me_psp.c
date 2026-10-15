/*
mp3_me_psp.c - the music's MP3 decoded by the PSP's Media Engine

libmpg123 decoded it on the CPU, which the PSP has only one of: a slice of
every frame, and a step of the physics now and then made late by it. The
Media Engine (sceAudiocodec) decodes one MP3 frame per call instead; this
file finds the frames itself (their headers give their sizes), which also
makes seeking a lookup.

One stream at a time (the music). The song is in memory (mp3_player.c reads
it whole), the PCM always comes out stereo 16 bit.
*/
#include <pspkernel.h>
#include <pspaudiocodec.h>
#include <psputility.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "psp_port.h"
#include "mp3_me_psp.h"

#define MAX_FRAME_BYTES 2048
#define MAX_FRAME_SAMPLES 1152

static unsigned long codec[65] __attribute__(( aligned( 64 )));
static bool codec_ready;

static const u8 *song;
static size_t song_size;

static u32 *frames;      // where each frame starts
static int num_frames;
static int frame_samples; // per frame: 1152 (MPEG 1) or 576 (MPEG 2, 2.5)
static int sample_rate;

static int next_frame;   // the next one to decode
static u8 *in_buf;       // a frame, aligned for the ME
static s16 *pcm;         // its samples, stereo
static int pcm_count;    // samples (stereo pairs) in pcm
static int pcm_pos;      // ... already handed out

static const u16 bitrates[2][16] = {
	{ 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 }, // MPEG 1
	{ 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 },     // MPEG 2, 2.5
};
static const u16 rates[3] = { 44100, 48000, 32000 };

// a layer III frame header at p: its size in bytes, 0 if it isn't one
static int MP3_FrameSize( const u8 *p, int *rate, int *samples )
{
	int version, layer, br, sr, pad, mpeg1;

	if( p[0] != 0xff || ( p[1] & 0xe0 ) != 0xe0 )
		return 0;
	version = ( p[1] >> 3 ) & 3; // 3 MPEG 1, 2 MPEG 2, 0 MPEG 2.5
	layer = ( p[1] >> 1 ) & 3;   // 1 layer III
	br = p[2] >> 4;
	sr = ( p[2] >> 2 ) & 3;
	pad = ( p[2] >> 1 ) & 1;
	if( version == 1 || layer != 1 || br == 0 || br == 15 || sr == 3 )
		return 0;

	mpeg1 = version == 3;
	*rate = rates[sr] >> ( mpeg1 ? 0 : version == 2 ? 1 : 2 );
	*samples = mpeg1 ? 1152 : 576;
	return ( mpeg1 ? 144 : 72 ) * bitrates[mpeg1 ? 0 : 1][br] * 1000 / *rate + pad;
}

static bool ME_Init( void )
{
	int ret;

	if( codec_ready )
		return true;

	// already loaded is fine too
	ret = sceUtilityLoadModule( PSP_MODULE_AV_AVCODEC );
	if( ret < 0 && ret != (int)0x80111102 )
		psp_log( "mp3: avcodec module: %08x\n", ret );

	memset( codec, 0, sizeof( codec ));
	if(( ret = sceAudiocodecCheckNeedMem( codec, PSP_CODEC_MP3 )) < 0
		|| ( ret = sceAudiocodecGetEDRAM( codec, PSP_CODEC_MP3 )) < 0
		|| ( ret = sceAudiocodecInit( codec, PSP_CODEC_MP3 )) < 0 )
	{
		psp_log( "mp3: the Media Engine decoder didn't start (%08x), using the CPU\n", ret );
		return false;
	}

	in_buf = memalign( 64, MAX_FRAME_BYTES );
	pcm = memalign( 64, MAX_FRAME_SAMPLES * 4 );
	if( !in_buf || !pcm )
		return false;

	codec_ready = true;
	return true;
}

bool me_mp3_open( const void *data, size_t size, int *rate, int *channels )
{
	size_t pos = 0;
	int cap = 0;

	me_mp3_close();
	if( !ME_Init())
		return false;

	song = data;
	song_size = size;

	// an ID3v2 tag first: its size is in 7 bit bytes
	if( size > 10 && !memcmp( song, "ID3", 3 ))
		pos = 10 + (( song[6] & 0x7f ) << 21 | ( song[7] & 0x7f ) << 14 | ( song[8] & 0x7f ) << 7 | ( song[9] & 0x7f ));

	// every frame, by its header; between frames (junk, tags) one byte at a time
	sample_rate = 0;
	while( pos + 4 <= size )
	{
		int fr, fs, n = MP3_FrameSize( song + pos, &fr, &fs );

		if( !n || pos + n > size || n > MAX_FRAME_BYTES || ( sample_rate && fr != sample_rate ))
		{
			pos++;
			continue;
		}
		if( num_frames == cap )
		{
			cap = cap ? cap * 2 : 4096;
			frames = realloc( frames, cap * sizeof( *frames ));
			if( !frames )
				return false;
		}
		frames[num_frames++] = pos;
		sample_rate = fr;
		frame_samples = fs;
		pos += n;
	}

	if( !num_frames )
	{
		psp_log( "mp3: no frames found\n" );
		return false;
	}

	next_frame = 0;
	pcm_count = pcm_pos = 0;
	*rate = sample_rate;
	*channels = 2;
	return true;
}

// the next frame into pcm; false at the end
static bool ME_DecodeFrame( void )
{
	int rate, samples, n;
	const u8 *src;

	if( next_frame >= num_frames )
		return false;

	src = song + frames[next_frame++];
	n = MP3_FrameSize( src, &rate, &samples );
	memcpy( in_buf, src, n );
	sceKernelDcacheWritebackInvalidateRange( in_buf, MAX_FRAME_BYTES );
	sceKernelDcacheWritebackInvalidateRange( pcm, MAX_FRAME_SAMPLES * 4 );

	codec[6] = (unsigned long)in_buf;
	codec[7] = codec[10] = n;
	codec[8] = (unsigned long)pcm;
	codec[9] = samples * 4;
	if( sceAudiocodecDecode( codec, PSP_CODEC_MP3 ) < 0 )
	{
		// a bad frame: silence instead of stopping the song
		memset( pcm, 0, samples * 4 );
	}
	sceKernelDcacheInvalidateRange( pcm, MAX_FRAME_SAMPLES * 4 );

	pcm_count = samples;
	pcm_pos = 0;
	return true;
}

size_t me_mp3_read( void *out, size_t bytes )
{
	size_t done = 0;
	u8 *dst = out;

	while( done + 4 <= bytes )
	{
		int n;

		if( pcm_pos >= pcm_count && !ME_DecodeFrame())
			break;

		n = pcm_count - pcm_pos;
		if( n > (int)(( bytes - done ) / 4 ))
			n = ( bytes - done ) / 4;
		memcpy( dst + done, pcm + pcm_pos * 2, n * 4 );
		pcm_pos += n;
		done += n * 4;
	}
	return done;
}

void me_mp3_seek( long sample )
{
	int frame, skip;

	if( !num_frames )
		return;
	if( sample < 0 )
		sample = 0;

	frame = sample / frame_samples;
	skip = sample % frame_samples;
	if( frame >= num_frames )
	{
		frame = 0;
		skip = 0;
	}

	// a frame can lean on the one before it (the bit reservoir): decode
	// that one first and drop it
	pcm_count = pcm_pos = 0;
	if( frame > 0 )
	{
		next_frame = frame - 1;
		ME_DecodeFrame();
	}
	next_frame = frame;
	if( ME_DecodeFrame())
		pcm_pos = skip;
}

long me_mp3_length( void )
{
	return (long)num_frames * frame_samples;
}

void me_mp3_close( void )
{
	free( frames );
	frames = NULL;
	num_frames = 0;
	song = NULL;
	song_size = 0;
	pcm_count = pcm_pos = 0;
	next_frame = 0;
}
