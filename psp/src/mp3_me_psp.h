/*
mp3_me_psp.h - the music's MP3 decoded by the Media Engine (mp3_me_psp.c)
*/
#pragma once
#include <stdbool.h>
#include <stddef.h>

// the song stays the caller's, in memory while it plays. False when the
// Media Engine can't be used (the caller decodes on the CPU then)
bool me_mp3_open( const void *data, size_t size, int *rate, int *channels );
size_t me_mp3_read( void *out, size_t bytes ); // stereo 16 bit; 0 at the end
void me_mp3_seek( long sample );
long me_mp3_length( void ); // in samples
void me_mp3_close( void );
