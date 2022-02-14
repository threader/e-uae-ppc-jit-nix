/*
 * UAE - The Un*x Amiga Emulator
 * 
 * Win32 interface
 *
 * Copyright 1997 Mathias Ortmann
 * Copyright 1997-2001 Brian King
 * Copyright 2000-2002 Bernd Roesch
 */
    

#define NATIVBUFFNUM 4
#define RECORDBUFFER 50 //survive 9 sec of blocking at 44100
#define ENFORCESIZE 1024
#ifdef __GNUC__
#define INITGUID
#endif 
#include <windows.h>
#include <stdlib.h>
#include <stdarg.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <io.h>
#include "winspool.h"
#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "memory.h"
#include "events.h"
#include "custom.h"
#include "gensound.h"
#include "newcpu.h"
#include "threaddep/thread.h"
#include <ctype.h>
#include <assert.h>
#include "od-win32/win32.h"
#include "gui.h"
#include "picasso96_win.h"
#include "sounddep/sound.h"
#include "od-win32/ahidsound.h"
#include "vfw.h"
#include "win32.h"
#include "win32gfx.h"
#include "dxwrap.h"
#include "inputdevice.h"
#include "avioutput.h"
#include "parser.h"
static long samples,playchannel,intcount,norec;
int ahi_on;
extern uae_u32 natmem_offset;
static char *sndptrmax, soundneutral,sndptr,*tempmem;
static HWND dsound_tmpw;

static WAVEFORMATEX wavfmt;

static LPSTR lpData,sndptrout;
extern uae_u32 chipmem_mask;
unsigned int samplecount,*sndbufrecpt;
static char *ahisndbuffer,*sndrecbuffer; 
static int ahisndbufsize,oldpos,*ahisndbufpt,ahitweak;;
static unsigned int dwBytes,dwBytes1,dwBytes2,espstore;
static LPVOID dwData1,dwData2;

int sound_freq_ahi;
int flashscreen=0;


	
		static int vin,devicenum;
static int amigablksize;

DWORD sound_flushes2  = 0;

extern HWND hAmigaWnd;
#ifdef __GNUC__
DEFINE_GUID(IID_IDirectSoundNotify, 0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);
#endif

static LPDIRECTSOUND lpDS2 = NULL;
static LPDIRECTSOUNDBUFFER lpDSBprimary2 = NULL;
static LPDIRECTSOUNDBUFFER lpDSB2 = NULL;
static LPDIRECTSOUNDNOTIFY lpDSBN2 = NULL;

// for record
static LPDIRECTSOUNDCAPTURE lpDS2r = NULL;
static LPDIRECTSOUNDCAPTUREBUFFER lpDSBprimary2r = NULL;
static LPDIRECTSOUNDCAPTUREBUFFER lpDSB2r = NULL;

struct winuae        //this struct is put in a6 if you call 
	                 //execute native function
{
	HWND			amigawnd;    //adress of amiga Window Windows Handle
    unsigned int    changenum;   //number to detect screen close/open 
    unsigned int    z3offset;    //the offset to add to acsess Z3 mem from Dll side
};

struct winuae uaevar;
struct winuae *a6;

#define CREATE_NATIVE_FUNC_PTR2 uae_u32 (* native_func)( uae_u32, uae_u32, uae_u32, uae_u32, uae_u32, uae_u32, uae_u32, \
						 uae_u32, uae_u32, uae_u32, uae_u32, uae_u32, uae_u32,uae_u32,uae_u32)
#define SET_NATIVE_FUNC2(x) native_func = (uae_u32 (*)(uae_u32, uae_u32, uae_u32, uae_u32, uae_u32, uae_u32, uae_u32, uae_u32, uae_u32, uae_u32, uae_u32, uae_u32, uae_u32,uae_u32,uae_u32))(x)
#define CALL_NATIVE_FUNC2( d1,d2,d3,d4,d5,d6,d7,a1,a2,a3,a4,a5,a6,a7) if(native_func) return native_func( d1,d2,d3,d4,d5,d6,d7,a1,a2,a3,a4,a5,a6,a7,regs_ )
	
static uae_u32 emulib_ExecuteNativeCode2 (void)
{ 
    uae_u8* object_UAM = (uae_u8*) m68k_areg( regs, 0 );
    uae_u32 d1 = m68k_dreg( regs, 1 );
    uae_u32 d2 = m68k_dreg( regs, 2 );
    uae_u32 d3 = m68k_dreg( regs, 3 );
    uae_u32 d4 = m68k_dreg( regs, 4 );
    uae_u32 d5 = m68k_dreg( regs, 5 );
    uae_u32 d6 = m68k_dreg( regs, 6 );
    uae_u32 d7 = m68k_dreg( regs, 7 );
    uae_u32 a1 = m68k_areg( regs, 1 );
    uae_u32 a2 = m68k_areg( regs, 2 );
    uae_u32 a3 = m68k_areg( regs, 3 );
    uae_u32 a4 = m68k_areg( regs, 4 );
    uae_u32 a5 = m68k_areg( regs, 5 );
    uae_u32 a7 = m68k_areg( regs, 7 );
    uae_u32 regs_ = &regs;
    CREATE_NATIVE_FUNC_PTR2;
    uaevar.z3offset = (uae_u32)(get_real_address (0x10000000) - 0x10000000);
    uaevar.amigawnd = hAmigaWnd;
    a6 = &uaevar;    
    if( object_UAM ) 
    {
	SET_NATIVE_FUNC2(object_UAM );
    __asm
	{   mov espstore,esp
	    push regs_
		push a7
		push a6
		push a5
		push a4
		push a3
		push a2
		push a1
        push d7
		push d6
        push d5
		push d4
		push d3
		push d2
		push d1
		call native_func
		mov esp,espstore
	}
	//CALL_NATIVE_FUNC2( d1, d2,d3, d4, d5, d6, d7, a1, a2, a3, a4 , a5 , a6 , a7);
    }
	else
    return 0;
}

char *DSError2( HRESULT error )
{
    switch( error )
    {
    case DSERR_ALLOCATED:
        return "Allocated";

    case DSERR_CONTROLUNAVAIL:
        return "Control Unavailable";

    case DSERR_INVALIDPARAM:
        return "Invalid Parameter";

    case DSERR_INVALIDCALL:
        return "Invalid Call";

    case DSERR_GENERIC:
        return "Generic";

    case DSERR_PRIOLEVELNEEDED:
        return "Priority Level Needed";

    case DSERR_OUTOFMEMORY:
        return "Out of Memory";

    case DSERR_BADFORMAT:
        return "Bad Format";

    case DSERR_UNSUPPORTED:
        return "Unsupported";

    case DSERR_NODRIVER:
        return "No Driver";

    case DSERR_ALREADYINITIALIZED:
        return "Already Initialized";

    case DSERR_NOAGGREGATION:
        return "No Aggregation";

    case DSERR_BUFFERLOST:
        return "Buffer Lost";

    case DSERR_OTHERAPPHASPRIO:
        return "Other Application Has Priority";

    case DSERR_UNINITIALIZED:
        return "Uninitialized";

    case DSERR_NOINTERFACE:
        return "No Interface";

    default:
        return "Unknown";
    }
}


void ahi_close_sound (void)
{
 	 HRESULT hr = DS_OK;

   
	 if (!ahi_on)
	     return;
		ahi_on=0;
        ahisndbufpt =(int*) ahisndbuffer;
        samplecount = 0;
		if( lpDSB2 )
		{
        hr = IDirectSoundBuffer_Stop( lpDSB2 );
		}
        
        if( hr != DS_OK )
        {
            write_log( "SoundStop() failure: %s\n", DSError2( hr ) );
        }
     else
	{
            write_log( "Sound Stopped...\n" );
	}
    if( lpDSB2 )
    {
        IDirectSoundBuffer_Release( lpDSB2 );
        lpDSB2 = NULL;
    }
    if( lpDSBprimary2 )
    {
        IDirectSoundBuffer_Release( lpDSBprimary2 );
        lpDSBprimary2 = NULL;
    }
    if( lpDS2 )
    {
        IDirectSound_Release( lpDS2 );
        lpDS2 = NULL;
    }
  
	 if( lpDSB2r )
    {
        IDirectSoundCaptureBuffer_Release( lpDSB2r );
        lpDSB2r = NULL;
    }
	if (dsound_tmpw)
	{
		DestroyWindow(dsound_tmpw);dsound_tmpw=0;
	}
	if (ahisndbuffer) free(ahisndbuffer);
    

    }

void ahi_updatesound( int force )
{
  HRESULT hr;
	int i;


  
		if( sound_flushes2 == 1 )
	{
		oldpos=0;
	    INTREQ(0xa000);
		intcount=1;
		/* Lock the entire buffer */
	   	hr = IDirectSoundBuffer_Lock( lpDSB2, 0, ahisndbufsize, &lpData, &dwBytes,&dwData2,&dwBytes2,0 );
		if( hr == DSERR_BUFFERLOST )
		{
		    IDirectSoundBuffer_Restore( lpDSB2 );
		    hr = IDirectSoundBuffer_Lock( lpDSB2, 0, 0, &lpData, &dwBytes,&dwData2,&dwBytes2, DSBLOCK_ENTIREBUFFER );
			
		}
	  
	    /* Get the big looping IDirectSoundBuffer_Play() rolling here, but only once at startup */
	    hr = IDirectSoundBuffer_Play( lpDSB2, 0, 0, DSBPLAY_LOOPING );
		hr=IDirectSoundBuffer_Unlock(lpDSB2,lpData,dwBytes,dwData2,dwBytes2); 
        if (!norec)hr = IDirectSoundCaptureBuffer_Start( lpDSB2r,DSBPLAY_LOOPING );

        //memset( lpData, 0x80,4 );
        
	}
/*
		{
long dwEvt=1;

  dwEvt = MsgWaitForMultipleObjects(
            2,      // How many possible events
            rghEvent,       // Location of handles
            FALSE,          // Wait for all?
            INFINITE,       // How long to wait
            QS_ALLINPUT);   // Any message is an event

			
calcsound=1;
if (dwEvt==0)freeblock=0;
if (dwEvt==1)freeblock=1;


if (dwEvt>1 ){calcsound=0;return;}
		}
*/

   hr = IDirectSoundBuffer_GetCurrentPosition(lpDSB2, &i, 0);
  if( hr != DSERR_BUFFERLOST )
	{
	i -= ahitweak;
	if (i < 0)i = i + ahisndbufsize;                  
	if (i >= ahisndbufsize)i = i - ahisndbufsize;
	i = ( i / (amigablksize * 4 )) * ( amigablksize * 4 );
		if (force == 1)
		{
			if (( oldpos != i) )
			{
			INTREQ( 0xa000 );
			intcount = 1;
			return; //to generate amiga ints every amigablksize
			       
			}
			else
			{
			return;
			}
		}
  
   }

hr = IDirectSoundBuffer_Lock( lpDSB2, oldpos, amigablksize * 4, &dwData1, &dwBytes1, &dwData2, &dwBytes2 ,0);


		if( hr == DSERR_BUFFERLOST )
			{
			write_log("lostbuf%d  %x\n",i,amigablksize);
		    IDirectSoundBuffer_Restore( lpDSB2 );
		    hr = IDirectSoundBuffer_Lock( lpDSB2, 0, 0, &lpData, &dwBytes, NULL, NULL, DSBLOCK_ENTIREBUFFER );
		  	dwData1=lpData;dwBytes1=dwBytes;dwBytes2=0;dwData2=0;
		}
		
	
				//write_log("%d  %x\n",freeblock,blksize);
			
                
	memcpy(dwData1,ahisndbuffer,dwBytes1);            			  		     		      
    sndptrmax = ahisndbuffer+ahisndbufsize;
	ahisndbufpt = (int*)ahisndbuffer;
 
IDirectSoundBuffer_Unlock(lpDSB2, dwData1,dwBytes1,dwData2, dwBytes2); 
  oldpos = i;
  //oldpos=oldpos+(amigablksize*4);
  //if (oldpos >= ahisndbufsize)oldpos=0; 
}


/* Use this to pause or stop Win32 sound output */


void ahi_finish_sound_buffer( void )
{
    sound_flushes2++;

    ahi_updatesound(2);
	
}

extern GUID sound_device_guid[];

static int ahi_init_sound_win32 (void)
{
    HRESULT hr;
    DSBUFFERDESC sound_buffer;
    DSCAPS DSCaps;
    DSCBUFFERDESC sound_buffer_rec;
    
    if (lpDS2 )
        return 0;

    enumerate_sound_devices (0);
    wavfmt.wFormatTag = WAVE_FORMAT_PCM;
    wavfmt.nChannels = 2;
    wavfmt.nSamplesPerSec = sound_freq_ahi;
    wavfmt.wBitsPerSample = 16;
    wavfmt.nBlockAlign = 16 / 8 * 2;
    wavfmt.nAvgBytesPerSec = wavfmt.nBlockAlign * sound_freq_ahi;

    soundneutral = 0;
    ahisndbufsize = (amigablksize*4)*NATIVBUFFNUM;  // use 4 native buffer
    ahisndbuffer=malloc(ahisndbufsize+32);
    if (!ahisndbuffer)return 0;
    hr = DirectSoundCreate( &sound_device_guid[currprefs.win32_soundcard], &lpDS2, NULL );
    if (hr != DS_OK) 
    {
        write_log( "DirectSoundCreate() failure: %s\n", DSError2 (hr));
        return 0;
    }
	memset (&sound_buffer, 0, sizeof( DSBUFFERDESC ));
    sound_buffer.dwSize = sizeof( DSBUFFERDESC );
    sound_buffer.dwFlags = DSBCAPS_PRIMARYBUFFER;
    sound_buffer.dwBufferBytes = 0;
    sound_buffer.lpwfxFormat = NULL;
    
    dsound_tmpw = CreateWindowEx( WS_EX_ACCEPTFILES,
                                  "PCsuxRox",
                                  "Argh",
                                  WS_CAPTION,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  10, 10,
                                  NULL,
                                  NULL,
                                  0,
                                  NULL);

    

    DSCaps.dwSize = sizeof( DSCAPS );
    hr = IDirectSound_GetCaps( lpDS2, &DSCaps );
    if( hr == DS_OK )
    {
	if( DSCaps.dwFlags & DSCAPS_EMULDRIVER )
	    write_log( "Your DirectSound Driver is emulated via WaveOut - yuck!\n" );
    }
	if FAILED(IDirectSound_SetCooperativeLevel(
            lpDS2,dsound_tmpw, DSSCL_PRIORITY))
        return 0;
    hr = IDirectSound_CreateSoundBuffer (lpDS2, &sound_buffer, &lpDSBprimary2, NULL);
    if( hr != DS_OK ) 
    {
        write_log( "CreateSoundBuffer() failure: %s\n", DSError2( hr ) );
        return 0;
    }
    hr = IDirectSoundBuffer_SetFormat (lpDSBprimary2, &wavfmt);
    if( hr != DS_OK ) 
    {
        write_log( "SetFormat() failure: %s\n", DSError2 (hr));
        return 0;
    }
    sound_buffer.dwBufferBytes = ahisndbufsize;
    sound_buffer.lpwfxFormat = &wavfmt;
    sound_buffer.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME /*| DSBCAPS_CTRLPOSITIONNOTIFY */| DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_GLOBALFOCUS |DSBCAPS_STATIC ;
    hr = IDirectSound_CreateSoundBuffer( lpDS2, &sound_buffer, &lpDSB2, NULL );
    if (hr != DS_OK) 
    {
        write_log ("CreateSoundBuffer() failure: %s\n", DSError2 (hr));
        return 0;
    }
/*  //used for PositionNotify 
    for ( i = 0; i < 2; i++)
    {
        rghEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (NULL == rghEvent[i]) return FALSE;
    }
    rgdsbpn[0].dwOffset = 0;
    rgdsbpn[0].hEventNotify = rghEvent[0];
    rgdsbpn[1].dwOffset = (soundbufsize/2)*1;
    rgdsbpn[1].hEventNotify = rghEvent[1];

   
    if FAILED(IDirectSoundBuffer_QueryInterface(lpDSB, 
            &IID_IDirectSoundNotify, (VOID **)&lpdsNotify))
        return FALSE; 
 
    if FAILED(IDirectSoundNotify_SetNotificationPositions(
             lpdsNotify, 2,rgdsbpn))
    {
        IDirectSoundNotify_Release(lpdsNotify);
        return FALSE;
    }

*/

    hr = IDirectSoundBuffer_SetVolume (lpDSB2, 0);
    if (hr != DS_OK) 
    {
        write_log( "SetVolume() 2 failure: %s\n", DSError2 (hr));
        return 0;
    }

     hr = IDirectSoundBuffer_GetFormat (lpDSBprimary2,&wavfmt,500,0);
    if( hr != DS_OK ) 
    {
        write_log( "GetFormat() failure: %s\n", DSError2 (hr));
        return 0;
    }
    // Record begin
	hr = DirectSoundCaptureCreate( NULL, &lpDS2r, NULL );
    if (hr != DS_OK) 
    {
        write_log( "DirectSoundCaptureCreate() failure: %s\n", DSError2 (hr));
        norec = 1;
    }
    memset (&sound_buffer_rec, 0, sizeof( DSCBUFFERDESC ));
    sound_buffer_rec.dwSize = sizeof( DSCBUFFERDESC );
	sound_buffer_rec.dwBufferBytes = amigablksize*4*RECORDBUFFER;
    sound_buffer_rec.lpwfxFormat = &wavfmt;
    sound_buffer_rec.dwFlags = 0 ;
  
	if (!norec)
	{	
		hr = IDirectSoundCapture_CreateCaptureBuffer( lpDS2r, &sound_buffer_rec, &lpDSB2r, NULL );
        	}
		if (hr != DS_OK) 
		{
			write_log ("CreateCaptureSoundBuffer() failure: %s\n", DSError2 (hr));
			norec = 1;
		}
		else
		

    
    
    if(ahisndbuffer==0)return 0;
    ahisndbufpt =(int*) ahisndbuffer;
    sndptrmax = ahisndbuffer + ahisndbufsize;
    samplecount = 0;
    memset( ahisndbuffer, soundneutral,amigablksize*8 );
    write_log("Init AHI Sound Rate %d Buffsize %d\n",sound_freq_ahi,amigablksize); 
	if (!norec)write_log("Init AHI Audio Recording \n");
    ahi_on = 1;
    return sound_freq_ahi;
}

static int rate;

int ahi_open_sound (void)
{  
    uaevar.changenum++;
    if (!sound_freq_ahi)
	return 0;
    if (ahi_on) {
	ahi_close_sound();
    }
    sound_flushes2 = 1;
    if ((rate = ahi_init_sound_win32 ()) )
	return rate;
    return 0;
} 

/*******************************************************************/

/* TODO: This should be moved into a separate file as it has nothing to
 * do with ahi */

static int enforcer_installed = 0;
static int enforcer_hit = 0; /* set to 1 if displaying the hit */

uae_u32 REGPARAM2	(*saved_chipmem_lget) (uaecptr addr);
uae_u32 REGPARAM2	(*saved_chipmem_wget) (uaecptr addr);
uae_u32 REGPARAM2	(*saved_chipmem_bget) (uaecptr addr);
void REGPARAM2		(*saved_chipmem_lput) (uaecptr addr, uae_u32 l);
void REGPARAM2		(*saved_chipmem_wput) (uaecptr addr, uae_u32 w);
void REGPARAM2		(*saved_chipmem_bput) (uaecptr addr, uae_u32 b);
int REGPARAM2		(*saved_chipmem_check) (uaecptr addr, uae_u32 size);
uae_u8 * REGPARAM2	(*saved_chipmem_xlate) (uaecptr addr);
uae_u32 REGPARAM2	(*saved_dummy_lget) (uaecptr addr);
uae_u32 REGPARAM2	(*saved_dummy_wget) (uaecptr addr);
uae_u32 REGPARAM2	(*saved_dummy_bget) (uaecptr addr);
void REGPARAM2		(*saved_dummy_lput) (uaecptr addr, uae_u32 l);
void REGPARAM2		(*saved_dummy_wput) (uaecptr addr, uae_u32 w);
void REGPARAM2		(*saved_dummy_bput) (uaecptr addr, uae_u32 b);
int REGPARAM2		(*saved_dummy_check) (uaecptr addr, uae_u32 size);

/*************************************************************
 Returns the first node entry of an exec list or 0 if
 empty
*************************************************************/
static uae_u32 amiga_list_first(uae_u32 list)
{
	uae_u32 node = get_long(list);      /* lh_Head */
	if (!node) return 0;
	if (!get_long(node)) return 0;   /* ln_Succ */
	return node;
}

/*************************************************************
 Returns the next node of an exec node or 0 if it was the
 last element
*************************************************************/
static uae_u32 amiga_node_next(uae_u32 node)
{
	uae_u32 next = get_long(node);    /* ln_Succ */
	if (!next) return 0;
	if (!get_long(next)) return 0; /* ln_Succ */
	return next;
}

/*************************************************************
 Converts an amiga address to a native one or NULL if this
 is not possible, Size specified the number of bytes you
 want to access
*************************************************************/
static uae_u8 *amiga2native(uae_u32 aptr, int size)
{
	addrbank bank = get_mem_bank(aptr);

	/* Check if the address can be translated to native */
	if (bank.check(aptr,size))
	{
		return bank.xlateaddr(aptr);
	}
	return NULL;
}

/*************************************************************
 Display the Hunk and Offset of the given Address
*************************************************************/
static void enforcer_display_hunk_and_offset(uae_u32 pc)
{
	uae_u32 sysbase = get_long(4);
	uae_u32 semaphore_list = sysbase + 532;

	/* First step is searching for the SegTracker semaphore */
	uae_u32 node = amiga_list_first(semaphore_list);
	while (node)
	{
		uae_u32 string = get_long(node+10); /* ln_Name */
		uae_u8 *native_string = amiga2native(string,100);

		if (native_string)
		{
			if (!strcmp(native_string,"SegTracker"))
				break;
		}
		node = amiga_node_next(node);
	}

	if (node)
	{
		/* We have found the segtracker semaphore. Soon after the
		 * public documented semaphore structure Segtracker holds
		 * an own list of all segements. We will use this list to
		 * find out the hunk and offset (simliar to segtracker).
		 *
		 * Source of segtracker can be found at:
		 *    http://www.sinz.org/Michael.Sinz/Enforcer/SegTracker.c.html
		 */
		
		uae_u32 seg_list = node + 46 + 4; /* sizeof(struct SignalSemaphore) + seg find */

		node = amiga_list_first(seg_list);
		while (node)
		{
			uae_u32 seg_entry = node + 12;
			uae_u32 address, size;
			int hunk = 0;

			/* Go through all entries until an address is 0
			 * or the segment has been found */
			while ((address = get_long(seg_entry)))
			{
				size = get_long(seg_entry+4);

				if (pc >= address && pc < address + size)
				{
					uae_u32 name,offset;
					uae_u8 *native_name;

					offset = pc - address - 4;
					name = get_long(node + 8); /* ln_Name */
					if (name)
					{
						native_name = amiga2native(name,100);
						if (!native_name) native_name = "Unknown";
					} else native_name = "Unknown";

					console_out("----> %08lx - \"%s\" Hunk %04lx Offset %08lx\n",pc,native_name,hunk,offset);
					write_log("----> %08lx - \"%s\" Hunk %04lx Offset %08lx\n",pc,native_name,hunk,offset);
					break;
				}

				seg_entry += 8;
				hunk++;
			}
			node = amiga_node_next(node);
		}
	}
}

/*************************************************************
 Display the enforcer hit
*************************************************************/
static void enforcer_display_hit(const char *addressmode, uae_u32 pc, uaecptr addr)
{
	uae_u32 a7;
	uae_u32 sysbase;
	uae_u32 this_task;
	uae_u32 task_name;
	uae_u8 *native_task_name;
	int i;
	char buf[256];

	if (enforcer_hit) return; /* our function itself generated a hit ;), avoid endless loop */
	enforcer_hit = 1;

	if (!(sysbase = get_long(4))) return;
	if (!(this_task = get_long(sysbase + 276))) return;

	task_name = get_long(this_task + 10); /* ln_Name */
	native_task_name = amiga2native(task_name,100);

	console_out("Enforcer Hit! Bad program\n");
	write_log("Enforcer Hit! Bad program\n");

	sprintf(buf,"Illegal %s: %08lx",addressmode, addr);

	console_out("%-48sPC: %0lx\n",buf, pc);
	write_log("%-48sPC: %0lx\n",buf, pc);

	console_out("Data: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
		m68k_dreg (regs, 0), m68k_dreg (regs, 1), m68k_dreg (regs, 2), m68k_dreg (regs, 3),
		m68k_dreg (regs, 4), m68k_dreg (regs, 5), m68k_dreg (regs, 6), m68k_dreg (regs, 7));
	write_log("Data: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
		m68k_dreg (regs, 0), m68k_dreg (regs, 1), m68k_dreg (regs, 2), m68k_dreg (regs, 3),
		m68k_dreg (regs, 4), m68k_dreg (regs, 5), m68k_dreg (regs, 6), m68k_dreg (regs, 7));

	console_out("Addr: %0lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
		m68k_areg (regs, 0), m68k_areg (regs, 1), m68k_areg (regs, 2), m68k_areg (regs, 3),
		m68k_areg (regs, 4), m68k_areg (regs, 5), m68k_areg (regs, 6), m68k_areg (regs, 7));
	write_log("Addr: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
		m68k_areg (regs, 0), m68k_areg (regs, 1), m68k_areg (regs, 2), m68k_areg (regs, 3),
		m68k_areg (regs, 4), m68k_areg (regs, 5), m68k_areg (regs, 6), m68k_areg (regs, 7));

	a7 = m68k_areg(regs,7);

	for (i = 0; i < 8 * 5; i++)
	{
		a7 -= 4;
		if (!(i % 8))
		{
			console_out("Stck:");
			write_log("Stck:");
		}
		console_out(" %08lx",get_long(a7));
		write_log(" %08lx",get_long(a7));

		if (i%8 == 7)
		{
			console_out("\n");
			write_log("\n");
		}
	}

	a7 = m68k_areg(regs,7);
	if (get_long(a7-4) != pc) enforcer_display_hunk_and_offset(pc);
	for (i = 0; i < 8 * 5; i++)
	{
		a7 -= 4;
		enforcer_display_hunk_and_offset(get_long(a7));
	}


	if (!native_task_name) native_task_name = "Unknown";

	console_out("Name: \"%s\"\n",native_task_name);
	write_log("Name: \"%s\"\n",native_task_name);

	console_out("\n");
	write_log("\n");

	enforcer_hit = 0;
	flashscreen = 30;
}

uae_u32 REGPARAM2 chipmem_lget2 (uaecptr addr)
{
    uae_u32 *m;

    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    m = (uae_u32 *)(chipmemory + addr);
	
	if (addr==0 || (addr>4 && addr<ENFORCESIZE))
	{	
		enforcer_display_hit("LONG READ from",regs.pc_p - NATMEM_OFFSET,addr);
	}
    return do_get_mem_long (m);
}

uae_u32 REGPARAM2 chipmem_wget2(uaecptr addr)
{
    uae_u16 *m;

    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    m = (uae_u16 *)(chipmemory + addr);
	if (addr==0 || (addr>4 && addr<ENFORCESIZE))
	{	
		enforcer_display_hit("WORD READ from",regs.pc_p - NATMEM_OFFSET,addr);
    }
    return do_get_mem_word (m);
}

uae_u32 REGPARAM2 chipmem_bget2 (uaecptr addr)
{
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
	if (addr==0 || (addr>4 && addr<ENFORCESIZE))
	{
		enforcer_display_hit("BYTE READ from",regs.pc_p - NATMEM_OFFSET,addr);
	}
    return chipmemory[addr];
}

void REGPARAM2 chipmem_lput2 (uaecptr addr, uae_u32 l)
{
    uae_u32 *m;
    
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    m = (uae_u32 *)(chipmemory + addr);
	if (addr==0 || (addr>4 && addr<ENFORCESIZE))
	{	
		enforcer_display_hit("LONG WRITE to",regs.pc_p - NATMEM_OFFSET,addr);
	}
    do_put_mem_long (m, l);
}

void REGPARAM2 chipmem_wput2 (uaecptr addr, uae_u32 w)
{
    uae_u16 *m;

    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    m = (uae_u16 *)(chipmemory + addr);

	if (addr==0 || (addr>4 && addr<ENFORCESIZE))
	{	
		enforcer_display_hit("WORD WRITE to",regs.pc_p - NATMEM_OFFSET,addr);
	}
   
    do_put_mem_word (m, w);
}

void REGPARAM2 chipmem_bput2 (uaecptr addr, uae_u32 b)
{
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
	if (addr==0 || (addr>4 && addr<ENFORCESIZE))
	{	
		enforcer_display_hit("BYTE WRITE to",regs.pc_p - NATMEM_OFFSET,addr);
	}
    chipmemory[addr] = b;
}

int REGPARAM2 chipmem_check2 (uaecptr addr, uae_u32 size)
{
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    return (addr + size) <= allocated_chipmem;
}

uae_u8 * REGPARAM2 chipmem_xlate2 (uaecptr addr)
{
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    return chipmemory + addr;
}

uae_u32 REGPARAM2 dummy_lget2 (uaecptr addr)
{
    special_mem |= S_READ;
	enforcer_display_hit("LONG READ from",regs.pc_p - NATMEM_OFFSET, addr);
	return 0xbadedeef;
}

#ifdef JIT
static int warned_JIT_0xF10000 = 0;
#endif

uae_u32 REGPARAM2 dummy_wget2 (uaecptr addr)
{

    special_mem |= S_READ;

#ifdef JIT
	if( addr >= 0x00F10000 && addr <= 0x00F7FFFF )
	{
	    if( !warned_JIT_0xF10000 )
	    {
			warned_JIT_0xF10000 = 1;
			enforcer_display_hit("LONG READ from",regs.pc_p - NATMEM_OFFSET,addr);
	    }
	    return 0;
	}
#endif
	enforcer_display_hit("WORD READ from",regs.pc_p - NATMEM_OFFSET,addr);
	return 0xbadf;
}

uae_u32 REGPARAM2 dummy_bget2 (uaecptr addr)
{
    special_mem |= S_READ;
	enforcer_display_hit("BYTE READ from",regs.pc_p - NATMEM_OFFSET,addr);
	return 0xbadedeef;
}

void REGPARAM2 dummy_lput2 (uaecptr addr, uae_u32 l)
{
    special_mem |= S_WRITE;
	enforcer_display_hit("LONG WRITE to",regs.pc_p - NATMEM_OFFSET,addr);
}

void REGPARAM2 dummy_wput2 (uaecptr addr, uae_u32 w)
{
    special_mem |= S_WRITE;
	enforcer_display_hit("WORD WRITE to",regs.pc_p - NATMEM_OFFSET,addr);
}

void REGPARAM2 dummy_bput2 (uaecptr addr, uae_u32 b)
{
    special_mem |= S_WRITE;
	enforcer_display_hit("BYTE WRITE to",regs.pc_p - NATMEM_OFFSET,addr);
}

int REGPARAM2 dummy_check2 (uaecptr addr, uae_u32 size)
{
    special_mem |= S_READ;
	enforcer_display_hit("CHECK from ",regs.pc_p - NATMEM_OFFSET,addr);
    return 0;
}


/*************************************************************
 enable the enforcer like support, maybe later this make MMU
 exceptions so enforcer can use it. Returns 1 if enforcer
 is enabled
*************************************************************/
int enforcer_enable(void)
{
	extern addrbank chipmem_bank,dummy_bank;

	if (!enforcer_installed)
	{
		saved_dummy_lget = dummy_bank.lget;
		saved_dummy_wget = dummy_bank.wget;
		saved_dummy_bget = dummy_bank.bget;
		saved_dummy_lput = dummy_bank.lput;
		saved_dummy_wput = dummy_bank.wput;
		saved_dummy_bput = dummy_bank.bput;
		saved_chipmem_lget = chipmem_bank.lget;
		saved_chipmem_wget = chipmem_bank.wget;
		saved_chipmem_bget = chipmem_bank.bget;
		saved_chipmem_lput = chipmem_bank.lput;
		saved_chipmem_wput = chipmem_bank.wput;
		saved_chipmem_bput = chipmem_bank.bput;
		saved_chipmem_xlate = chipmem_bank.xlateaddr;
		saved_chipmem_check = chipmem_bank.check;

		dummy_bank.lget = dummy_lget2;
	    dummy_bank.wget = dummy_wget2;
		dummy_bank.bget = dummy_bget2;
		dummy_bank.lput = dummy_lput2;
	    dummy_bank.wput = dummy_wput2;
		dummy_bank.bput = dummy_bput2;
	    chipmem_bank.lget = chipmem_lget2;
	    chipmem_bank.wget = chipmem_wget2;
	    chipmem_bank.bget = chipmem_bget2;
	    chipmem_bank.lput = chipmem_lput2;
	    chipmem_bank.wput = chipmem_wput2;
	    chipmem_bank.bput = chipmem_bput2;
		chipmem_bank.xlateaddr = chipmem_xlate2;
		chipmem_bank.check = chipmem_check2;

		enforcer_installed = 1;
	}
	return 1;
}

/*************************************************************
 Disable Enforcer like support
*************************************************************/
int enforcer_disable(void)
{
	if (enforcer_installed)
	{
		dummy_bank.lget = saved_dummy_lget;
	    dummy_bank.wget = saved_dummy_wget;
		dummy_bank.bget = saved_dummy_bget;
		dummy_bank.lput = saved_dummy_lput;
	    dummy_bank.wput = saved_dummy_wput;
		dummy_bank.bput = saved_dummy_bput;
	    chipmem_bank.lget = saved_chipmem_lget;
	    chipmem_bank.wget = saved_chipmem_wget;
	    chipmem_bank.bget = saved_chipmem_bget;
	    chipmem_bank.lput = saved_chipmem_lput;
	    chipmem_bank.wput = saved_chipmem_wput;
	    chipmem_bank.bput = saved_chipmem_bput;
		chipmem_bank.xlateaddr = saved_chipmem_xlate;
		chipmem_bank.check = saved_chipmem_check;

		enforcer_installed = 0;
	}
	return 1;
}

/*******************************************************************/

static char *addr;
static void *bswap_buffer = NULL;
static uae_u32 bswap_buffer_size = 0;

uae_u32 ahi_demux (void)
{
//use the extern int (6 #13)  
// d0 0=opensound      d1=unit d2=samplerate d3=blksize ret: sound frequency
// d0 1=closesound     d1=unit
// d0 2=writesamples   d1=unit a0=addr      write blksize samples to card     
// d0 3=readsamples    d1=unit a0=addr      read samples from card ret: d0=samples read
      // make sure you have from amigaside blksize*4 mem alloced 
      // d0=-1 no data available d0=-2 no recording open
	  // d0 > 0 there are more blksize Data in the que
	  // do the loop until d0 get 0
	  // if d0 is greater than 200 bring a message
	  // that show the user that data is lost
	  // maximum blocksbuffered are 250 (8,5 sec)
// d0 4=writeinterrupt d1=unit  d0=0 no interrupt happen for this unit
	  // d0=-2 no playing open

	//note units for now not support use only unit 0
	
// d0=10 get clipboard size      d0=size in bytes  
// d0=11 get clipboard data      a0=clipboarddata
                                  //Note: a get clipboard size must do before
// d0=12 write clipboard data    a0=clipboarddata
// d0=13 setp96mouserate         d1=hz value
// d0=100 open dll               d1=dll name in windows name conventions
// d0=101 get dll function addr  d1=dllhandle a0 function/var name 
// d0=102 exec dllcode           a0=addr of function (see 101)
// d0=103 close dll
// d0=104 screenlost
// d0=105 mem offset
// d0=106 16Bit byteswap    
// d0=107 32Bit byteswap
// d0=108 free swap array
// d0=200 ahitweak               d1=offset for dsound position pointer

int opcode = m68k_dreg (regs, 0);
switch (opcode) {
  	int i,slen,t,todo,byte1,byte2;	
	LPTSTR p,p2,pos1,pos2;
	uae_u32 src, num_vars;
	static int cap_pos,clipsize;
	static	LPTSTR clipdat;
	int cur_pos;
	
    case 0:
		cap_pos=0;
   		sound_freq_ahi=m68k_dreg (regs, 2);
  		amigablksize=m68k_dreg (regs, 3);
		sound_freq_ahi=ahi_open_sound();
		uaevar.changenum--;
		return sound_freq_ahi;
    case 1:
		ahi_close_sound();
		sound_freq_ahi = 0;
		return 0;
   	case 2:
		addr=(char *)m68k_areg (regs, 0);
		for (i=0;i<(amigablksize*4);i+=4)
		{ 
	        
			ahisndbufpt[0]=get_long((unsigned int)addr+i);
			ahisndbufpt+=1;
			/*ahisndbufpt[0]=chipmem_bget((unsigned int)addr+i+2);
			ahisndbufpt+=1;
			ahisndbufpt[0]=chipmem_bget((unsigned int)addr+i+1);
			ahisndbufpt+=1;
			ahisndbufpt[0]=chipmem_bget((unsigned int)addr+i);
			ahisndbufpt+=1;*/
		}
			ahi_finish_sound_buffer();
	        return amigablksize;
	case 3:
		if (norec)return -1;
		if (!ahi_on)return -2;
        i = IDirectSoundCaptureBuffer_GetCurrentPosition(lpDSB2r,&t,&cur_pos);
        t =  amigablksize*4;
		
		if (cap_pos<=cur_pos)todo=cur_pos-cap_pos;
		else
			todo=cur_pos+(RECORDBUFFER*t)-cap_pos;
		if (todo<t)    
		{                //if no complete buffer ready exit
			return -1;
		}
		i = IDirectSoundCaptureBuffer_Lock(lpDSB2r,cap_pos,t,&pos1,&byte1,&pos2,&byte2,0);

		if ((cap_pos+t)< (t*RECORDBUFFER))
		{
			cap_pos=cap_pos+t;
		}
		else
		{
			cap_pos = 0;

		}  
	 	addr=(char *)m68k_areg (regs, 0);
		sndbufrecpt=(unsigned int*)pos1;
		t=t/4;
  		for (i=0;i<t;i++)
		{
 			put_long((uae_u32)addr,sndbufrecpt[0]);
			addr+=4;
			sndbufrecpt+=1;
		}
		t=t*4;
		i=IDirectSoundCaptureBuffer_Unlock(lpDSB2r,pos1,byte1,pos2,byte2);
        return (todo-t)/t;
    case 4:
		if (!ahi_on)return -2;
		i=intcount;
		intcount=0;
        return i;
	
	case 10:	
       i=OpenClipboard(0);
 	   clipdat=GetClipboardData(CF_TEXT);
	   if (clipdat)
	   {
	       clipsize=strlen(clipdat);
	       clipsize++;
	       return clipsize;
	   }
	   else 
	   {
		   return 0;
	   }
	case 11:
		addr=(char *)m68k_areg (regs, 0);
  		for (i=0;i<clipsize;i++)
		{
 			put_byte((uae_u32)addr,clipdat[0]);
			addr++;
			clipdat++;
		}
		CloseClipboard();
		return 0;
	case 12:	   
	   addr = (char *)m68k_areg (regs, 0);
 	   addr = (char *)get_real_address ((uae_u32)addr);
  	   i = OpenClipboard (0);
	   EmptyClipboard();
 	   slen = strlen(addr);
	   p = GlobalAlloc (GMEM_DDESHARE,slen+2);
	   p2 = GlobalLock (p);
       memcpy (p2,addr,slen);
	   p2[slen]=0;
	   GlobalUnlock (p);
	   i = (int)SetClipboardData (CF_TEXT,p2);
	   CloseClipboard ();
	   GlobalFree (p);
	   return 0;
	case 13:
      /* HACK */
        { //for higher P96 mouse draw rate
       extern int p96hack_vpos2,hack_vpos,p96refresh_active;
       extern uae_u16 vtotal;
       extern unsigned int new_beamcon0;
       p96hack_vpos2=15625/m68k_dreg (regs, 1);
       p96refresh_active=1;
       if (!picasso_on)return 0;
       vtotal=p96hack_vpos2;           // only do below if P96 screen is visible
       new_beamcon0 |= 0x80;             
       hack_vpos=vtotal;                  
        } //end for higher P96 mouse draw rate
      return 0;

	case 20:
		return enforcer_enable();

	case 21:
		return enforcer_disable();

	case 25:
        flushprinter ();
	return 0;
	
	case 100: // open dll
		{  
		char *dllname;
		uae_u32 result;
        dllname = ( char *) m68k_areg (regs, 0);
        dllname = (char *)get_real_address ((uae_u32)dllname);
        result=(uae_u32) LoadLibrary(dllname);
		write_log("%s windows dll/alib loaded at %d (0 mean failure)\n",dllname,result); 
		return result;
		}
	case 101:      //get dll label
		{
		HMODULE m;
        char *funcname;
		m = (HMODULE) m68k_dreg (regs, 1);
		funcname = (char *)m68k_areg (regs, 0);
        funcname = (char *)get_real_address ((uae_u32)funcname);
		return (uae_u32) GetProcAddress(m,funcname);
		}
	case 102:      //execute native code
		return emulib_ExecuteNativeCode2 ();
		
	case 103:      //close dll
		{
		HMODULE libaddr;
		libaddr= (HMODULE) m68k_dreg (regs, 1);
		FreeLibrary(libaddr);
		return 0;
		}
	case 104:        //screenlost
	{
		static int oldnum=0;
		if (uaevar.changenum == oldnum)return 0;
		oldnum=uaevar.changenum;return 1;
	}
	case 105:		//returns memory offset
	    return (uae_u32) get_real_address(0);
	case 106:		//byteswap 16bit vars
					//a0 = start address
					//d1 = number of 16bit vars
					//returns address of new array
		src = m68k_areg(regs, 0);
		num_vars = m68k_dreg(regs, 1);
		
		if (bswap_buffer_size < num_vars * 2) {
			bswap_buffer_size = (num_vars + 1024) * 2;
			free(bswap_buffer);
			bswap_buffer = (void*)malloc(bswap_buffer_size);
		}
			__asm {
				mov esi, dword ptr [src]
				mov edi, dword ptr [bswap_buffer]
				mov ecx, num_vars
				
				mov ebx, ecx
				and ecx, 3
				je BSWAP_WORD_4X
				
			BSWAP_WORD_LOOP:
				mov ax, [esi]
				mov dl, al
				mov al, ah
				mov ah, dl
				mov [edi], ax
				add esi, 2
				add edi, 2
				loopne BSWAP_WORD_LOOP
				
			BSWAP_WORD_4X:
				mov ecx, ebx
				shr ecx, 2
				je BSWAP_WORD_END
			BSWAP_WORD_4X_LOOP:
				mov ax, [esi]
				mov dl, al
				mov al, ah
				mov ah, dl
				mov [edi], ax
				mov ax, [esi+2]
				mov dl, al
				mov al, ah
				mov ah, dl
				mov [edi+2], ax
				mov ax, [esi+4]
				mov dl, al
				mov al, ah
				mov ah, dl
				mov [edi+4], ax
				mov ax, [esi+6]
				mov dl, al
				mov al, ah
				mov ah, dl
				mov [edi+6], ax
				add esi, 8
				add edi, 8
				loopne BSWAP_WORD_4X_LOOP
				
			BSWAP_WORD_END:
			}
		return (uae_u32) bswap_buffer;
	case 107:	//byteswap 32bit vars - see case 106
				//a0 = start address
				//d1 = number of 32bit vars
				//returns address of new array
		src = m68k_areg(regs, 0);
		num_vars = m68k_dreg(regs, 1);
		if (bswap_buffer_size < num_vars * 4) {
			bswap_buffer_size = (num_vars + 16384) * 4;
			free(bswap_buffer);
			bswap_buffer = (void*)malloc(bswap_buffer_size);
		}
			__asm {
				mov esi, dword ptr [src]
				mov edi, dword ptr [bswap_buffer]
				mov ecx, num_vars
				
				mov ebx, ecx
				and ecx, 3
				je BSWAP_DWORD_4X
				
			BSWAP_DWORD_LOOP:
				mov eax, [esi]
				bswap eax
				mov [edi], eax
				add esi, 4
				add edi, 4
				loopne BSWAP_DWORD_LOOP
				
			BSWAP_DWORD_4X:
				mov ecx, ebx
				shr ecx, 2
				je BSWAP_DWORD_END
			BSWAP_DWORD_4X_LOOP:
				mov eax, [esi]
				bswap eax
				mov [edi], eax
				mov eax, [esi+4]
				bswap eax
				mov [edi+4], eax
				mov eax, [esi+8]
				bswap eax
				mov [edi+8], eax
				mov eax, [esi+12]
				bswap eax
				mov [edi+12], eax
				add esi, 16
				add edi, 16
				loopne BSWAP_DWORD_4X_LOOP
				
			BSWAP_DWORD_END:
			}
		return (uae_u32) bswap_buffer;
	case 108:		//frees swap array
		bswap_buffer_size = 0;
		free(bswap_buffer);
		bswap_buffer = NULL;
		return 0; 	
	case 200:
     ahitweak = m68k_dreg (regs, 1);
	 return 1;
	default:
	return 0x12345678;     // Code for not supportet function
	}
} 
