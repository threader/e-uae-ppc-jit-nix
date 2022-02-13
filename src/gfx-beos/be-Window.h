#ifndef BE_WINDOW_H
#define BE_WINDOW_H
/***********************************************************/
//  UAE - The Un*x Amiga Emulator
//
//  BeOS port - graphics stuff
//
//  (c) 2000-2001 Axel D�fler
//  (c) 1999 Be/R4 Sound - Raphael Moll
//  (c) 1998-1999 David Sowsy
//  (c) 1996-1998 Christian Bauer
//  (c) 1996 Patrick Hanevold
//
/***********************************************************/

#include <AppKit.h>
#include <InterfaceKit.h>
#include <KernelKit.h>
#include <DirectWindow.h>

#include <stdio.h>
#include <stdlib.h>


const uint32 kMsgRedraw = 'draw';
const uint32 kMsgToggleFullScreen = 'full';
const uint32 kMsgFullSizeWindow = 'w100';
const uint32 kMsgHalfSizeWindow = 'w050';
const uint32 kMsgThirdSizeWindow = 'w033';
const uint32 kMsgQuit = 'quit';
const uint32 kMsgAbout = 'abut';
const uint32 kMsgReset = 'rset';

const uint8 kWindowNone = 0;
const uint8 kWindowBitmap = 1;
const uint8 kWindowSingleLine = 2;
const uint8 kWindowFullScreen = 4;

const int MAX_CLIP_LIST_COUNT = 64;


// Global variables
extern UAEWindow *gWindow;
extern uint8 gWindowMode;
extern int	 inhibit_frame;


class BitmapView;

/*
 *  The window in which the Amiga graphics are displayed, handles I/O
 */

class UAEWindow : public BDirectWindow
{
	public:
		UAEWindow(BRect frame,bool useBitmap);

		bool QuitRequested(void);
		void MessageReceived(BMessage *msg);
		void FrameResized(float width,float height);

		void DirectConnected(direct_buffer_info *info);
		void UpdateBufferInfo(direct_buffer_info *info);
		void UnlockBuffer();
		uint8 *LockBuffer();

		void InitColors();
		void DrawBlock(int yMin,int yMax);
		void DrawLine(int y);
		void SetFullScreenMode(bool full);
		bool UpdateMouseButtons();

	private:
		BitmapView *fBitmapView;
		BBitmap	*fBitmap;
		bool	fReset;
		bool	fIsConnected;
		int32	fAcquireFailed;

		uint8	fScreenLine[8192];	// UAE writes in this buffer in direct window mode

		sem_id	fDrawingLock;
		uint8	*fBits8;			// the frame buffer (top left corner of the window)
		uint16	*fBits16;
		uint32	*fBits32;

		clipping_rect fWindowBounds;
		clipping_rect fClipList[MAX_CLIP_LIST_COUNT];
		uint32	fClipListCount;
};

/*
 *  A simple view class for blitting a bitmap on the screen
 */

class BitmapView : public BView {
	public:
		BitmapView(BRect frame, BBitmap *bitmap);
		~BitmapView();

		void Draw(BRect update);
		void Pulse();
		void MouseMoved(BPoint point, uint32 transit, const BMessage *message);

	private:
		BBitmap *fBitmap;
};

#endif

