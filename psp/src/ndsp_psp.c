/*
ndsp_psp.c - the 3DS DSP channels, mixed in software

A thread mixes every channel's queued wave buffers into one 44100 Hz stereo
stream (linear resampling from each channel's rate) and hands it to a PSP
hardware audio channel; the blocking output paces it. Buffers go QUEUED ->
PLAYING -> DONE like on the 3DS, and the callback the game sets (the mp3
player's "refill now") runs after every mixed block.
*/
#include <pspkernel.h>
#include <pspsdk.h>
#include <pspaudio.h>
#include <string.h>

#include <3ds.h>
#include "psp_port.h"

#define OUT_RATE    44100
#define OUT_FRAMES  1024

typedef struct {
	ndspWaveBuf *head;      // playing, then the queued ones through ->next
	ndspWaveBuf *tail;
	float rate;
	int channels;
	bool paused;
	float gain_l, gain_r;
	// frames into head, and the 1/65536ths past that: fixed point, as a
	// double here was a software-emulated operation (the PSP's FPU has no
	// doubles) on every sample of every channel
	u32 pos, frac;
} Channel;

static Channel chn[NDSP_NUM_CHANNELS];
static SceUID mix_lock = -1;
static SceUID mix_thread = -1;
static int hw_channel = -1;
static volatile bool running;
static ndspCallback callback;
static void *callback_data;

static short __attribute__(( aligned( 64 ))) out_buf[2][OUT_FRAMES * 2];
static int mix_l[OUT_FRAMES], mix_r[OUT_FRAMES];

/*
Past about 7/8 of full scale the sum bends towards the limit instead of
hitting it: the music is loud already, and the sound effects on top of it
were cut flat (clicks). Below that it is left as it is
*/
#define KNEE 28672
static inline short SoftClip( int v )
{
	int a = v < 0 ? -v : v;
	if( a > KNEE )
	{
		// KNEE + room * x / (x + room): never quite reaches 32767
		int room = 32767 - KNEE, x = a - KNEE;
		a = KNEE + (int)(( (long long)room * x ) / ( x + room ));
	}
	return v < 0 ? -a : a;
}

static void Lock( void )   { sceKernelWaitSema( mix_lock, 1, NULL ); }
static void Unlock( void ) { sceKernelSignalSema( mix_lock, 1 ); }

static void ResetChannel( Channel *c )
{
	ndspWaveBuf *b;

	for( b = c->head; b; b = b->next )
		b->status = NDSP_WBUF_DONE;
	c->head = c->tail = NULL;
	c->rate = OUT_RATE;
	c->channels = 1;
	c->paused = false;
	c->gain_l = c->gain_r = 1.0f;
	c->pos = c->frac = 0;
}

static void MixChannel( Channel *c )
{
	u32 step = (u32)( c->rate * ( 65536.0f / OUT_RATE ));
	int i;

	for( i = 0; i < OUT_FRAMES && c->head; i++ )
	{
		ndspWaveBuf *b = c->head;
		const s16 *s = b->data_pcm16;
		u32 n = b->nsamples;
		u32 f = c->pos;
		float t = c->frac * ( 1.0f / 65536.0f );
		float l, r;

		if( f + 1 < n )
		{
			if( c->channels == 2 )
			{
				l = s[f * 2] + ( s[f * 2 + 2] - s[f * 2] ) * t;
				r = s[f * 2 + 1] + ( s[f * 2 + 3] - s[f * 2 + 1] ) * t;
			}
			else
			{
				l = r = s[f] + ( s[f + 1] - s[f] ) * t;
			}
		}
		else if( f < n && b->next && b->next->nsamples )
		{
			// the last frame: on to the next buffer's first one, not a step
			const s16 *s2 = b->next->data_pcm16;
			if( c->channels == 2 )
			{
				l = s[f * 2] + ( s2[0] - s[f * 2] ) * t;
				r = s[f * 2 + 1] + ( s2[1] - s[f * 2 + 1] ) * t;
			}
			else
			{
				l = r = s[f] + ( s2[0] - s[f] ) * t;
			}
		}
		else if( f < n )
		{
			l = s[c->channels == 2 ? f * 2 : f];
			r = s[c->channels == 2 ? f * 2 + 1 : f];
		}
		else
		{
			l = r = 0;
		}

		mix_l[i] += (int)( l * c->gain_l );
		mix_r[i] += (int)( r * c->gain_r );

		c->frac += step;
		c->pos += c->frac >> 16;
		c->frac &= 0xffff;
		if( c->pos >= n )
		{
			// on to the next buffer
			c->pos -= n;
			if( b->looping )
				continue;
			b->status = NDSP_WBUF_DONE;
			c->head = b->next;
			if( !c->head )
			{
				c->tail = NULL;
				c->pos = c->frac = 0;
			}
			else
			{
				c->head->status = NDSP_WBUF_PLAYING;
			}
		}
	}
}

static int MixThread( SceSize args, void *argp )
{
	int cur = 0;

	(void)args; (void)argp;
	pspSdkDisableFPUExceptions(); // a real PSP traps on float errors (ctru_psp.c)
	while( running )
	{
		short *out = out_buf[cur];
		int i;

		memset( mix_l, 0, sizeof( mix_l ));
		memset( mix_r, 0, sizeof( mix_r ));

		Lock();
		for( i = 0; i < NDSP_NUM_CHANNELS; i++ )
			if( !chn[i].paused && chn[i].head )
				MixChannel( &chn[i] );
		Unlock();

		for( i = 0; i < OUT_FRAMES; i++ )
		{
			out[i * 2]     = SoftClip( mix_l[i] );
			out[i * 2 + 1] = SoftClip( mix_r[i] );
		}

		sceAudioOutputPannedBlocking( hw_channel, PSP_AUDIO_VOLUME_MAX, PSP_AUDIO_VOLUME_MAX, out );
		cur ^= 1;

		if( callback )
			callback( callback_data );
	}
	return 0;
}

Result ndspInit( void )
{
	int i;

	if( running )
		return 0;

	mix_lock = sceKernelCreateSema( "ndsp", 0, 1, 1, NULL );
	for( i = 0; i < NDSP_NUM_CHANNELS; i++ )
		ResetChannel( &chn[i] );

	hw_channel = sceAudioChReserve( PSP_AUDIO_NEXT_CHANNEL, OUT_FRAMES, PSP_AUDIO_FORMAT_STEREO );
	if( hw_channel < 0 )
	{
		psp_log( "ndspInit: no audio channel (%x)\n", hw_channel );
		return -1;
	}

	running = true;
	mix_thread = sceKernelCreateThread( "ndsp_mix", MixThread, 0x12, 0x8000, PSP_THREAD_ATTR_USER, NULL );
	if( mix_thread < 0 )
	{
		running = false;
		return -1;
	}
	sceKernelStartThread( mix_thread, 0, NULL );
	return 0;
}

void ndspExit( void )
{
	if( !running )
		return;
	running = false;
	sceKernelWaitThreadEnd( mix_thread, NULL );
	sceKernelDeleteThread( mix_thread );
	sceAudioChRelease( hw_channel );
}

void ndspSetOutputMode( ndspOutputMode mode ) { (void)mode; }

void ndspSetCallback( ndspCallback cb, void *data )
{
	callback = cb;
	callback_data = data;
}

#define CHN( id ) if(( id ) < 0 || ( id ) >= NDSP_NUM_CHANNELS ) return

void ndspChnReset( int id )
{
	CHN( id );
	Lock();
	ResetChannel( &chn[id] );
	Unlock();
}

void ndspChnSetInterp( int id, ndspInterpType type ) { (void)id; (void)type; }

void ndspChnSetRate( int id, float rate )
{
	CHN( id );
	chn[id].rate = rate > 0 ? rate : OUT_RATE;
}

void ndspChnSetFormat( int id, u16 format )
{
	CHN( id );
	chn[id].channels = ( format & 3 ) == 2 ? 2 : 1;
}

void ndspChnSetMix( int id, float mix[12] )
{
	CHN( id );
	chn[id].gain_l = mix[0];
	chn[id].gain_r = mix[1];
}

void ndspChnSetPaused( int id, bool paused )
{
	CHN( id );
	chn[id].paused = paused;
}

bool ndspChnIsPaused( int id )
{
	if( id < 0 || id >= NDSP_NUM_CHANNELS )
		return false;
	return chn[id].paused;
}

bool ndspChnIsPlaying( int id )
{
	if( id < 0 || id >= NDSP_NUM_CHANNELS )
		return false;
	return chn[id].head && !chn[id].paused;
}

void ndspChnWaveBufAdd( int id, ndspWaveBuf *buf )
{
	Channel *c;

	CHN( id );
	if( !buf || !buf->data_vaddr || !buf->nsamples )
	{
		if( buf )
			buf->status = NDSP_WBUF_DONE;
		return;
	}

	c = &chn[id];
	Lock();
	buf->next = NULL;
	buf->status = NDSP_WBUF_QUEUED;
	if( c->tail )
	{
		c->tail->next = buf;
		c->tail = buf;
	}
	else
	{
		c->head = c->tail = buf;
		c->pos = c->frac = 0;
		buf->status = NDSP_WBUF_PLAYING;
	}
	Unlock();
}

void ndspChnWaveBufClear( int id )
{
	ndspChnReset( id );
}
