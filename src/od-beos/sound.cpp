 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Support for BeOS sound
  *
  * Copyright 1997 Bernd Schmidt
  * Copyright 2003 Richard Drummond
  */

extern "C" {
#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "custom.h"
#include "gensound.h"
#include "sounddep/sound.h"
}

#include <be/media/SoundPlayer.h>
#include <be/media/MediaDefs.h>

extern "C" {
void finish_sound_buffer (void);
void update_sound (int freq);
int setup_sound (void);
void close_sound (void);
int init_sound (void);
void pause_sound (void);
void resume_sound (void);
void reset_sound (void);
}

BSoundPlayer *sndplayer;

static int have_sound = 0;
static int obtainedfreq;

uae_u16 sndbuffer2[44100];
uae_u16 *sndbuffer = &sndbuffer2[0]; /* work-around for compiler grumbling */
uae_u16 *sndbufpt;
int sndbufsize;

int stop_sound;

/* producer/consumer locks on sndbuffer */
static sem_id data_available_sem;
static sem_id data_used_sem;


static void clearbuffer (void)
{
    memset (sndbuffer, 0, sizeof sndbuffer);
}


void finish_sound_buffer (void)
{
    if (!stop_sound) {
	if (release_sem (data_available_sem) == B_NO_ERROR && !stop_sound)
	    acquire_sem (data_used_sem);
    }
}


void update_sound (int freq)
{
    int scaled_sample_evtime_orig;
    static int lastfreq =0;

    if (freq < 0)
	freq = lastfreq;
    lastfreq = freq;
    if (have_sound) {
	if (currprefs.gfx_vsync && currprefs.gfx_afullscreen) {
	    if (currprefs.ntscmode)
		scaled_sample_evtime_orig = (unsigned long)(MAXHPOS_NTSC * MAXVPOS_NTSC * freq * CYCLE_UNIT + obtainedfreq - 1) / obtainedfreq;
	else
	    scaled_sample_evtime_orig = (unsigned long)(MAXHPOS_PAL * MAXVPOS_PAL * freq * CYCLE_UNIT + obtainedfreq - 1) / obtainedfreq;
	} else {
	    scaled_sample_evtime_orig = (unsigned long)(312.0 * 50 * CYCLE_UNIT / (obtainedfreq  / 227.0));
	}
	scaled_sample_evtime = scaled_sample_evtime_orig;
    }
}


/* Try to determine whether sound is available.  This is only for GUI purposes.  */
int setup_sound (void)
{
    struct media_raw_audio_format format;

    sound_available = 0;

    format.frame_rate    = currprefs.sound_freq;
    format.format        = currprefs.sound_bits == 8
	? media_raw_audio_format::B_AUDIO_UCHAR
	: media_raw_audio_format::B_AUDIO_SHORT;
    format.channel_count = currprefs.stereo ?  2 : 1;
    format.buffer_size = 512;
#ifdef WORDS_BIGENDIAN
    format.byte_order    = B_MEDIA_BIG_ENDIAN;
#else
    format.byte_order    = B_MEDIA_LITTLE_ENDIAN;
#endif

    BSoundPlayer player (&format);

    if (player.InitCheck () == B_OK)
	sound_available = 1;
    else
	write_log ("Couldn't create sound player\n");

    return sound_available;
}


static void callback (void *cookie, void *buffer, size_t size, const media_raw_audio_format &format)
{
    if (!stop_sound) {
	if (acquire_sem (data_available_sem) == B_NO_ERROR) {
	    if (!stop_sound) {
		memcpy (buffer, sndbuffer, size);
		release_sem (data_used_sem);
	    }
	}
    }
}


static int init_sound (void)
{
    int size = currprefs.sound_maxbsiz;
    struct media_raw_audio_format format;

    size >>= 2;
    while (size & (size - 1))
	size &= size - 1;
    if (size < 512)
	size = 512;

    sndbufsize = size * (currprefs.sound_bits / 8) * (currprefs.stereo ?  2 : 1);

    format.frame_rate    = currprefs.sound_freq;
    format.format        = currprefs.sound_bits == 8
	? media_raw_audio_format::B_AUDIO_UCHAR
	: media_raw_audio_format::B_AUDIO_SHORT;
    format.channel_count = currprefs.stereo ?  2 : 1;
    format.buffer_size   = sndbufsize;
#ifdef WORDS_BIGENDIAN
    format.byte_order    = B_MEDIA_BIG_ENDIAN;
#else
    format.byte_order    = B_MEDIA_LITTLE_ENDIAN;
#endif

    sndplayer = new BSoundPlayer(&format, NULL, callback);

    if (sndplayer->InitCheck () != B_OK) {
	write_log ("Failed to initialize BeOS sound driver\n");
	return 0;
    }

    if (format.format == media_raw_audio_format::B_AUDIO_SHORT) {
	init_sound_table16 ();
	sample_handler = currprefs.stereo ? sample16s_handler : sample16_handler;
    } else {
	init_sound_table8 ();
	sample_handler = currprefs.stereo ? sample8s_handler : sample8_handler;
    }

    clearbuffer();
    obtainedfreq = currprefs.sound_freq;
    update_sound (vblank_hz);
    have_sound = 1;
    sound_available = 1;
    sndbufpt = sndbuffer;

    write_log ("BeOS sound driver found and configured for %d bits at %d Hz, buffer is %d samples\n",
	currprefs.sound_bits, currprefs.sound_freq, sndbufsize);

    resume_sound();

    return 1;
}


void close_sound (void)
{
    if (! have_sound)
	return;

    pause_sound();
    delete sndplayer;
    have_sound = 0;
}


void pause_sound (void)
{
    if (! have_sound)
        return;

    stop_sound = 1;
    delete_sem (data_available_sem);
    delete_sem (data_used_sem);
    sndplayer->SetHasData (false);
    sndplayer->Stop ();
}


void resume_sound (void)
{
    if (! have_sound)
        return;

    stop_sound = 0;
    data_available_sem = create_sem(0, NULL);
    data_used_sem = create_sem(0, NULL);
    sndplayer->Start ();
    sndplayer->SetHasData (true);
}


void reset_sound (void)
{
    clearbuffer();
}
