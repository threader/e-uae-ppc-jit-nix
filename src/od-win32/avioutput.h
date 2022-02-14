/*
  UAE - The Ultimate Amiga Emulator
  
  avioutput.h
  
  Copyright(c) 2001 - 2002 �ane
*/

extern int avioutput_video, avioutput_audio;

extern int avioutput_width, avioutput_height, avioutput_bits;
extern int avioutput_fps;

extern char avioutput_filename[MAX_PATH];

extern void AVIOutput_WriteAudio(uae_u8 *sndbuffer, int sndbufsize);
extern void AVIOutput_WriteVideo(void);
extern LPSTR AVIOutput_ChooseAudioCodec(HWND hwnd);
extern LPSTR AVIOutput_ChooseVideoCodec(HWND hwnd);
extern void AVIOutput_End(void);
extern void AVIOutput_Begin(void);
extern void AVIOutput_Release(void);
extern void AVIOutput_Initialize(void);

#define AVIAUDIO_AVI 1
#define AVIAUDIO_WAV 2


/*
extern int avioutput_pause;

extern int avioutput_bits;

extern int avioutput_fps;

extern int avioutput_width, avioutput_height;

extern int avioutput_video, avioutput_audio;

extern int avioutput_init;

extern int frame_count;

extern char avioutput_filename[MAX_PATH];

extern void AviOutputClearAudioCodec(HWND hwnd);
extern void AviOutputClearVideoCodec(HWND hwnd);

extern void avioutput_screenshot(void);

extern void AVIWriteAudio(uae_u16* sndbuffer, int sndbufsize);
extern void AVIWriteVideo(void);
extern LPSTR AVIChooseAudioCodec(HWND hwnd);
extern LPSTR AVIChooseVideoCodec(HWND hwnd);
extern void AVIUninitialize(void);
extern void AVIInitialize(void);
*/