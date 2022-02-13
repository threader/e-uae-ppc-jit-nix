/***********************************************************/
//  BeUAE - The Be Un*x Amiga Emulator
//
//  BeOS port specific stuff
//
//  (c) 2004 Richard Drummond
//  (c) 2000-2001 Axel D�fler
//  (c) 1999 Be/R4 Sound - Raphael Moll
//  (c) 1998-1999 David Sowsy
//  (c) 1996-1998 Christian Bauer
//  (c) 1996 Patrick Hanevold
//
/***********************************************************/

#include "be-UAE.h"
//#include "be-Settings.h"
#include "be-Window.h"

extern "C" {
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "xwin.h"
#include "custom.h"
#include "picasso96.h"
#include "uae.h"
#include "inputdevice.h"
};

// some support c functions we use
static uint32 get_screen_space(int width,int height,int depth);
static void get_color_bitshifts(int mode,int *redBits,int *greenBits,int *blueBits,int *redShift,int *greenShift,int *blueShift);

#ifdef PICASSO96
static void update_picasso_vidinfo();
#endif
static void update_gfxvidinfo();

// Speed control hacks by David Sowsy
extern int mouse_speed_rate;

BPoint	gOrigin(0,0);
uint8	gWindowMode;

static class UAEWindow *gWin;
uint32	gWidth,gHeight,gBytesPerRow;
uint8	gBytesPerPixel;
uint8	*gBuffer;
color_space gColorSpace;
bool	gIsPicasso;
int32	gLockBalance = 0;


UAEWindow::UAEWindow(BRect frame,bool useBitmap)
	: BDirectWindow(frame, PACKAGE_NAME, B_TITLED_WINDOW,B_NOT_RESIZABLE | B_NOT_ZOOMABLE),
	fBitmap(NULL),
	fReset(false),
	fIsConnected(false)
{
	// Move window to right position
	MoveTo(80, 80);

	// if (gBePrefs.dynamic_resize)
	// SetFlags(Flags() & ~B_NOT_RESIZABLE);

	gWidth = currprefs.gfx_width_win;
	gHeight = currprefs.gfx_height_win;

	if (useBitmap)
	{
		switch(currprefs.color_mode)
		{
			case 0:
			case 3:
			case 4:
				gColorSpace = B_CMAP8;
				gBytesPerPixel = 1;
				break;
			case 1:
				gColorSpace = B_RGB15;
				gBytesPerPixel = 2;
				break;
			case 2:
				gColorSpace = B_RGB16;
				gBytesPerPixel = 2;
				break;
			case 5:
			default:
				gColorSpace = B_RGB32;
				gBytesPerPixel = 4;
				break;
		}
		InitColors();
		fBitmap = new BBitmap(BRect(0,0,currprefs.gfx_width_win-1,currprefs.gfx_height_win-1),gColorSpace);

		gBytesPerRow = fBitmap->BytesPerRow();
		gBuffer = (uint8 *)fBitmap->Bits();

		memset(gBuffer,0,fBitmap->BitsLength());
		gWindowMode = kWindowBitmap;
	}
	// Create bitmap view
	frame.OffsetTo(0.0,0.0);
	fBitmapView = new BitmapView(frame,fBitmap);
	AddChild(fBitmapView);

	update_gfxvidinfo();
#ifdef PICASSO96
	update_picasso_vidinfo();
#endif

	if (currprefs.gfx_afullscreen)
 		SetFullScreenMode(true);

	// AddShortcut('f',B_COMMAND_KEY,new BMessage(kMsgToggleFullScreen));
	// AddShortcut('1',B_COMMAND_KEY,new BMessage(kMsgFullSizeWindow));
	// AddShortcut('2',B_COMMAND_KEY,new BMessage(kMsgHalfSizeWindow));
	// AddShortcut('3',B_COMMAND_KEY,new BMessage(kMsgThirdSizeWindow));

    gWin = this;

	fDrawingLock = create_sem(0,"UAE drawing lock");
}


void UAEWindow::FrameResized(float width,float height)
{
	float x = width / gfxvidinfo.width;
	float y = height / gfxvidinfo.height;

	fBitmapView->SetScale(min(x,y));

	BMessage msg(kMsgRedraw);
	PostMessage(&msg);
}


void UAEWindow::InitColors()
{
	if (gBytesPerPixel == 1)	// clut color modes
	{
		BScreen screen(this);
		int i = 0;

		for (register int r = 0;r < 16;r++)
		{
			for (register int g = 0;g < 16;g++)
			{
				for (register int b = 0;b < 16;b++)
					xcolors[i++] = screen.IndexForColor(r << 4 | r, g << 4 | g, b << 4 | b);
			}
		}
	}
	else	// high and true color modes
	{
		int redBits,greenBits,blueBits;
		int redShift,greenShift,blueShift;

		get_color_bitshifts(currprefs.color_mode,&redBits,&greenBits,&blueBits,&redShift,&greenShift,&blueShift);
		alloc_colors64k(redBits,greenBits,blueBits,redShift,greenShift,blueShift,0,0,0);
	}
}


void UAEWindow::UpdateBufferInfo(direct_buffer_info *info)
{
	fWindowBounds = info->window_bounds;

	if (gWindowMode == kWindowBitmap)
	{
		if (IsFullScreen())
			gOrigin.Set((fWindowBounds.right - fWindowBounds.left - currprefs.gfx_width_win) >> 1,
						(fWindowBounds.bottom - fWindowBounds.top - currprefs.gfx_height) >> 1);
		else
			gOrigin.Set(0,0);

		BMessage msg(kMsgRedraw);
		PostMessage(&msg);
		return;
	}
	if (!IsFullScreen())
	{
		/** Copy the current clip list **/

		fClipListCount = info->clip_list_count;
		if (fClipListCount > MAX_CLIP_LIST_COUNT)
			fClipListCount = MAX_CLIP_LIST_COUNT;

		memcpy(fClipList,info->clip_list,fClipListCount * sizeof(clipping_rect));

		if ((info->buffer_state & ~B_DIRECT_MODE_MASK) == B_CLIPPING_MODIFIED)
			return;
	}
	/** update gfxvidinfo **/

	int pixbytes = (info->bits_per_pixel + 1) >> 3;
	if (gBytesPerPixel != pixbytes)
	{
		switch(info->bits_per_pixel)
		{
			case 8:
				currprefs.color_mode = 0;
				break;
			case 15:
				currprefs.color_mode = 1;
				break;
			case 16:
				currprefs.color_mode = 2;
				break;
			case 32:
				currprefs.color_mode = 5;
				break;
		}
		gBytesPerPixel = pixbytes;
		InitColors();
	}

	gWidth = fWindowBounds.right - fWindowBounds.left + 1;
	gHeight = fWindowBounds.bottom - fWindowBounds.top + 1;
	gBytesPerRow = info->bytes_per_row;
	gBuffer = (uint8 *)info->bits;

#ifdef PICASOO96
	if (gIsPicasso)
		update_picasso_vidinfo();
	else
#endif
		update_gfxvidinfo();

	if (IsFullScreen())
	{
		if (!gIsPicasso)
		{
			gfxvidinfo.emergmem = (uae_u8 *)fScreenLine;
			gfxvidinfo.maxblocklines = gfxvidinfo.height;

			// black screen
			memset(gfxvidinfo.bufmem,0,gfxvidinfo.rowbytes * gfxvidinfo.height);
			init_row_map();
		}
		gWindowMode = kWindowFullScreen;
	}
	else
	{
		if (!gIsPicasso)
		{
			gfxvidinfo.linemem = (uae_u8 *)fScreenLine;
			gfxvidinfo.bufmem = NULL;
			gfxvidinfo.maxblocklines = 0;
		}
		gWindowMode = kWindowSingleLine;
	}
	switch(gBytesPerPixel)
	{
		case 1:
			fBits8 = (uint8 *)info->bits;
			break;
		case 2:
			fBits16 = (uint16 *)info->bits;
			break;
		case 4:
			fBits32 = (uint32 *)info->bits;
			break;
	}
}


void UAEWindow::DirectConnected(direct_buffer_info *info)
{
	switch (info->buffer_state & B_DIRECT_MODE_MASK)
	{
		case B_DIRECT_START:	// start a direct screen connection.
			fIsConnected = true;
			UpdateBufferInfo(info);
			release_sem(fDrawingLock);
			break;
		case B_DIRECT_STOP:		// stop a direct screen connection.
			acquire_sem(fDrawingLock);
			break;
		case B_DIRECT_MODIFY:	// modify the state of a direct screen connection.
			acquire_sem(fDrawingLock);
			UpdateBufferInfo(info);
			release_sem(fDrawingLock);
			break;
	}
}


void UAEWindow::UnlockBuffer()
{
//	if (!fAcquireFailed--)
//		release_sem(fDrawingLock);
}


uint8 *UAEWindow::LockBuffer()
{
	/*if ((gBuffer || gIsPicasso && gWindowMode == kWindowFullScreen)
		&& acquire_sem(fDrawingLock) == B_NO_ERROR)
	{*/
//	update_picasso_vidinfo();
		return gBuffer;
	/*}
	fAcquireFailed++;

	return NULL;*/
}


void UAEWindow::DrawLine(int y)
{
	if (!fIsConnected)
		return;
	if (acquire_sem(fDrawingLock) != B_NO_ERROR)
		return;

	int i = fClipListCount;
	clipping_rect *rect = fClipList;

	y += fWindowBounds.top;

	switch(gBytesPerPixel)
	{
		case 1:
		{
			uint8 *bits = fBits8 + y*gBytesPerRow;
			int   x = fWindowBounds.left;

			for(i = fClipListCount;i--;rect++)
				if (rect->top <= y && rect->bottom >= y)
					memcpy(bits + rect->left,fScreenLine + rect->left - x,1 + rect->right - rect->left);
			break;
		}
		case 2:
		{
			uint16 *bits = (uint16 *)((uint8 *)fBits16 + y*gBytesPerRow);
			int   x = (fWindowBounds.left) << 1;

			for(i = fClipListCount;i--;rect++)
				if (rect->top <= y && rect->bottom >= y)
					memcpy(bits + rect->left,fScreenLine + (rect->left << 1) - x,(1 + rect->right - rect->left) << 1);
			break;
		}
		case 4:
		{
			uint32 *bits = (uint32 *)((uint8 *)fBits32 + y*gBytesPerRow);
			int   x = (fWindowBounds.left) << 2;

			for(i = fClipListCount;i--;rect++)
				if (rect->top <= y && rect->bottom >= y)
					memcpy(bits + rect->left,fScreenLine + (rect->left << 2) - x,(1 + rect->right - rect->left) << 2);
			break;
		}
	}
	release_sem(fDrawingLock);
}


void UAEWindow::DrawBlock(int yMin,int yMax)
{
	if (Lock())
	{
		fBitmapView->DrawBitmapAsync(fBitmap,gOrigin);
		Unlock();
	}
}


void UAEWindow::SetFullScreenMode(bool full)
{
	if (gIsPicasso)	// picasso96 emulation currently supports only full screen
		return;

	if (!IsFullScreen())
	{
		int depth =  gColorSpace == B_CMAP8 ? 8 :
					(gColorSpace == B_RGB15 ? 15 :
					(gColorSpace == B_RGB16 ? 16 : 32));
		uint32 space = get_screen_space(gWidth,gHeight,depth);
		if (space != ~0L)
			set_screen_space(0,space,false);
	}
	else
		restoreWorkspaceResolution();
	be_app->HideCursor();

	SetFullScreen(!IsFullScreen());
}


bool UAEWindow::QuitRequested(void)
{
	gWindowMode = kWindowNone;

	be_app->PostMessage(B_QUIT_REQUESTED);
	return false;
}


bool UAEWindow::UpdateMouseButtons()
{
	bool proceed = false;

	if (Lock())
	{
		if (fReset)
		{
			uae_reset(0);
			fReset = false;
		}
		if (IsActive())
		{
			BPoint mousePoint;
			uint32 mouseButtons;

			fBitmapView->GetMouse(&mousePoint,&mouseButtons,true);

			if (mousePoint.x >= 0 && mousePoint.x < gWidth &&
				mousePoint.y >= 0 && mousePoint.y < gHeight)
			{
		        setmousebuttonstate (0, 0, mouseButtons & B_PRIMARY_MOUSE_BUTTON);
		        setmousebuttonstate (0, 1, mouseButtons & B_SECONDARY_MOUSE_BUTTON);
		        setmousebuttonstate (0, 2, mouseButtons & B_TERTIARY_MOUSE_BUTTON);
			}
			proceed = true;
		}
		Unlock();
	}
	return proceed;
}


void UAEWindow::MessageReceived(BMessage *msg)
{
	switch(msg->what)
	{
		case kMsgReset:
			fReset = true;
			break;
		case kMsgQuit:
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;
		case kMsgToggleFullScreen:
			SetFullScreenMode(!IsFullScreen());
			break;
		case kMsgThirdSizeWindow:
			if (!gIsPicasso)
				ResizeTo(gWidth / 3,gHeight / 3);
			break;
		case kMsgHalfSizeWindow:
			if (!gIsPicasso)
				ResizeTo(gWidth / 2,gHeight / 2);
			break;
		case kMsgFullSizeWindow:
			if (!gIsPicasso)
				ResizeTo(gWidth,gHeight);
			break;
		case kMsgRedraw:
		{
			BMessage *oldMessage;

			if (MessageQueue()->Lock())
			{
				while ((oldMessage = MessageQueue()->FindMessage(kMsgRedraw,0)) != NULL)
				{
					MessageQueue()->RemoveMessage(oldMessage);
					delete oldMessage;
				}
				MessageQueue()->Unlock();
			}
			fBitmapView->Invalidate();
			break;
		}
		default:
			BWindow::MessageReceived(msg);
			break;
	}
}


//#pragma mark -
//**************************************************
// The BitmapView is the "indirect" graphics buffer
//**************************************************


BitmapView::BitmapView(BRect frame,BBitmap *bitmap)
	: BView(frame,"",B_FOLLOW_ALL_SIDES,B_WILL_DRAW)
{
	SetViewColor(0,0,0); // B_TRANSPARENT_COLOR);
	fBitmap = bitmap;
}


BitmapView::~BitmapView()
{
	delete fBitmap;
}


void BitmapView::Pulse()
{
	/*BMessage msg(kMsgRedraw);

	gEmulationWindow->PostMessage(&msg);*/
}


void BitmapView::Draw(BRect update)
{
	DrawBitmapAsync(fBitmap, gOrigin);
}


void BitmapView::MouseMoved(BPoint point, uint32 transit, const BMessage *message)
{
	switch (transit) {
		case B_ENTERED_VIEW:
			be_app->HideCursor();

			setmousestate(0, 0, (int) point.x, 1);
			setmousestate(0, 1, (int) point.y, 1);
 			break;
		case B_EXITED_VIEW:
			be_app->ShowCursor();
			break;
		case B_INSIDE_VIEW:
			setmousestate(0, 0, (int) point.x, 1);
			setmousestate(0, 1, (int) point.y, 1);
			break;
	}
}


//#pragma mark -
//**************************************************
// c support functions
//**************************************************


static uint32 get_screen_space(int width,int height,int depth)
{
	if (width <= 640 && height <= 480)
	{
		switch(depth)
		{
			case 8:  return  B_8_BIT_640x480;
			case 15: return B_15_BIT_640x480;
			case 16: return B_16_BIT_640x480;
			case 32:
			default: return B_32_BIT_640x480;
		}
	}
	else if (width <= 800 && height <= 600)
	{
		switch(depth)
		{
			case 8:  return  B_8_BIT_800x600;
			case 15: return B_15_BIT_800x600;
			case 16: return B_16_BIT_800x600;
			case 32:
			default: return B_32_BIT_800x600;
		}
	}
	else if (width <= 1024 && height <= 768)
	{
		switch(depth)
		{
			case 8:  return  B_8_BIT_1024x768;
			case 15: return B_15_BIT_1024x768;
			case 16: return B_16_BIT_1024x768;
			case 32:
			default: return B_32_BIT_1024x768;
		}
	}
	else if (width <= 1280 && height <= 1024)
	{
		switch(depth)
		{
			case 8:  return  B_8_BIT_1280x1024;
			case 15: return B_15_BIT_1280x1024;
			case 16: return B_16_BIT_1280x1024;
			case 32:
			default: return B_32_BIT_1280x1024;
		}
	}
	write_log("unsupported screen resolution\n");
	return ~0L;
}


static void get_color_bitshifts(int mode,int *redBits,int *greenBits,int *blueBits,int *redShift,int *greenShift,int *blueShift)
{
	switch(mode)
	{
		case 1:
			*redBits = *greenBits = *blueBits = 5;
			*redShift = 10;  *greenShift = 5;  *blueShift = 0;
			break;
		case 2:
			*redBits = *blueBits = 5;  *greenBits = 6;
			*redShift = 11;  *greenShift = 5;  *blueShift = 0;
			break;
		case 5:
		default:
			*redBits = *greenBits = *blueBits = 8;
			*redShift = 16;  *greenShift = 8;  *blueShift = 0;
			break;
	}
}


static void update_gfxvidinfo()
{
	gfxvidinfo.width = gWidth;
	gfxvidinfo.height = gHeight;
	gfxvidinfo.rowbytes = gBytesPerRow;
	gfxvidinfo.pixbytes = gBytesPerPixel;

	gfxvidinfo.bufmem = (uae_u8 *)gBuffer;
	gfxvidinfo.emergmem = NULL;
	gfxvidinfo.linemem = NULL;
	gfxvidinfo.maxblocklines = gfxvidinfo.height;
}

#ifdef PICASSO96
static void update_picasso_vidinfo()
{
	switch(gBytesPerPixel)
	{
		case 1:
			picasso_vidinfo.rgbformat = RGBFB_CLUT;
			break;
		case 2:
			picasso_vidinfo.rgbformat = RGBFB_R5G6B5PC;
			break;
		case 4:
			picasso_vidinfo.rgbformat = RGBFB_R8G8B8A8;
			break;
	}
	picasso_vidinfo.width = gWidth;
	picasso_vidinfo.height = gHeight;
	picasso_vidinfo.rowbytes = gBytesPerRow;
	picasso_vidinfo.pixbytes = gBytesPerPixel;
	picasso_vidinfo.depth = 8 * gBytesPerPixel;
	//picasso_vidinfo.selected_rgbformat = picasso_vidinfo.rgbformat; //RGBFB_R5G6B5PC;
	picasso_vidinfo.extra_mem = gIsPicasso ? 1 : 0;

/*	printf("picasso: rgbformat = %d\n",picasso_vidinfo.rgbformat);
	printf("picasso: selected_rgbformat = %d\n",picasso_vidinfo.selected_rgbformat);
	printf("picasso: rowbytes = %d\n",picasso_vidinfo.rowbytes);
	printf("picasso: pixbytes = %d\n",picasso_vidinfo.pixbytes);
	printf("picasso: depth = %d\n",picasso_vidinfo.depth);
	printf("picasso: width = %d\n",picasso_vidinfo.width);
	printf("picasso: height = %d\n",picasso_vidinfo.height);*/
}
#endif

//#pragma mark -
//**************************************************
// UAE calls these functions to update the graphic display
//**************************************************


int lockscr(void)
{
	return 1;
}

void unlockscr(void)
{
}


void flush_line(int y)
{
	if (gWindowMode == kWindowSingleLine)
		gEmulationWindow->DrawLine(y);
}


void flush_block(int ystart, int ystop)
{
}


void flush_screen(int ystart, int ystop)
{
	if (gWindowMode == kWindowFullScreen)
	{
		if (gEmulationWindow->LockBuffer())
			gEmulationWindow->UnlockBuffer();
	}
	else if (gWindowMode == kWindowBitmap)
		gEmulationWindow->DrawBlock(ystart,ystop);
}

void flush_clear_screen (void)
{
}

int pause_emulation;


//#pragma mark -
//**************************************************
// Picasso96 support functions
//**************************************************

#ifdef PICASSO96

int DX_FillResolutions(uae_u16 * ppixel_format)
{
	display_mode *modes;
	BScreen screen;
    uint32 modeCount,count = 0;

	if (screen.GetModeList(&modes,&modeCount) != B_OK)
		return 0;

	for(int i = 0;i < modeCount;i++)
	{
		switch(modes[i].space)
		{
			case B_CMAP8:
			case B_GRAY8:
				DisplayModes[count].depth = 1;
				*ppixel_format |= RGBMASK_8BIT;
				break;
			case B_RGB15:
				DisplayModes[count].depth = 2;
				*ppixel_format |= RGBMASK_15BIT;
				break;
			case B_RGB16:
				DisplayModes[count].depth = 2;
				*ppixel_format |= RGBMASK_16BIT;
				break;
			case B_RGB32:
				/* I don't know why, but Picasso96 don't want to display anything in 32bit mode */
				//DisplayModes[count].depth = 4;
				//*ppixel_format |= RGBMASK_32BIT;
				//break;
			default:
				continue;
		}
		DisplayModes[count].res.width = modes[i].virtual_width;
		DisplayModes[count].res.height = modes[i].virtual_height;
		DisplayModes[count].refresh = 75;

		if (++count >= MAX_PICASSO_MODES)
			break;
	}
	free(modes);
    return count;
}

void DX_SetPalette(int start, int count)
{
    if (picasso_vidinfo.pixbytes == 1)
    {
    	/*color_map colorMap = system_colors();

		while(count-- > 0)
		{
			picasso_vidinfo.clut[start++] = colorMap.color_list
		}
    	return;*/
	}
	/* This is the case when we're emulating a 256 color display.  */
	int redBits,greenBits,blueBits;
	int redShift,greenShift,blueShift;
	int mode = 0;

	if (picasso_vidinfo.rgbformat & RGBMASK_15BIT)
		mode = 1;
	else if (picasso_vidinfo.rgbformat & RGBMASK_16BIT)
		mode = 2;
	else if (picasso_vidinfo.rgbformat & RGBMASK_32BIT)
		mode = 5;

	get_color_bitshifts(currprefs.color_mode,&redBits,&greenBits,&blueBits,&redShift,&greenShift,&blueShift);

	while (count-- > 0)
	{
	    int r = picasso96_state.CLUT[start].Red;
	    int g = picasso96_state.CLUT[start].Green;
	    int b = picasso96_state.CLUT[start].Blue;

	    picasso_vidinfo.clut[start++] = ( doMask256(r,redBits,redShift)
										| doMask256(g,greenBits,greenShift)
										| doMask256(b,blueBits,blueShift));
	}
}


void DX_Invalidate(int first, int last)
{
}


int DX_BitsPerCannon(void)
{
    return 8;
}


int DX_FillRect(uaecptr addr, uae_u16 X, uae_u16 Y, uae_u16 Width, uae_u16 Height, uae_u32 Pen, uae_u8 Bpp)
{
    return 0;
}


uae_u8 *gfx_lock_picasso(void)
{
	return (uae_u8 *)gEmulationWindow->LockBuffer();
}


void gfx_unlock_picasso(void)
{
	gEmulationWindow->UnlockBuffer();
}


void gfx_set_picasso_state(int on)
{
	printf("gfx_set_picasso_state(%s)\n",on ? "on" : "off");
	if (on)
	{
		update_picasso_vidinfo();
//		if (!gIsPicasso)
//			picasso_vidinfo.extra_mem = 0;
	}
	gIsPicasso = on ? true : false;
//    if (screen_is_picasso == on)
//	return;
//
//    screen_is_picasso = on;
//    close_windows ();
//    open_windows ();
//    DX_SetPalette (0, 256);
}


void gfx_set_picasso_modeinfo (int w, int h, int depth, int rgbfmt)
{
	printf("gfx_set_picasso_modeinfo: w = %d, h = %d, d = %d, rgb = %d\n",w,h,depth,rgbfmt);
	gIsPicasso = true;

	uint32 space = get_screen_space(w,h,depth);
	if (space == ~0L)
		return;

	picasso_vidinfo.selected_rgbformat = rgbfmt;
	set_screen_space(0,space,false);
	be_app->HideCursor();
	gWindowMode = kWindowNone;
	gEmulationWindow->SetFullScreen(true);

	printf("extra: %d\n",picasso_vidinfo.extra_mem);
//	switch(depth)
//	{
//		case 8:
//			colorSpace = B_CMAP8;
//			break;
//		case 15:
//			colorSpace = B_RGB15;
//			break;
//		case 16:
//			colorSpace = B_RGB16;
//			break;
//		case 32:
//			colorSpace = B_RGB32;
//			break;
//	}
//	gWindowMode = kWindowNone;
	/*** find possible display mode ***/
//return;
//	BScreen screen;
//	display_mode *modes;
//	uint32 count;
//
//	if (screen.GetModeList(&modes,&count) == B_OK)
//	{
//		display_mode mode,low,high;
//		bool first = true;
//
//		for(int i = count;i--;)
//		{
//			if (modes[i].virtual_width == w && modes[i].virtual_height == h
//				&& modes[i].space == colorSpace)
//			{
//				gWindowMode = kWindowNone;
//				screen.SetMode(&modes[i]);
//				gEmulationWindow->SetFullScreen(true);
//				//mode = modes[i];
//				break;
//			}
//		}
//		free(modes);
//		if (screen.GetMode(&mode) == B_OK)
//		{
//
//			mode.virtual_width = w;
//			mode.virtual_height = h;
//			mode.space = colorSpace;
//
//			if (screen.ProposeMode(&high,&low,&high) != B_ERROR)
//			{
//				gWindowMode = kWindowNone;
//				screen.SetMode(&mode);
//				gEmulationWindow->SetFullScreen(true);
//			}
//		}
//	}
//	picasso_vidinfo.depth = dep;
//	picasso_vidinfo.selected_rgbformat = RGBFB_R8G8B8A8;
//    depth >>= 3;
//    if (picasso_vidinfo.width == w
//	&& picasso_vidinfo.height == h
//	&& picasso_vidinfo.depth == depth
//	&& picasso_vidinfo.selected_rgbformat == rgbfmt)
//	return;
//
//
//    if (screen_is_picasso) {
//	close_windows ();
//	open_windows ();
//	DX_SetPalette (0, 256);
//    }
}

#endif

/*
 * Mouse inputdevice functions
 */

/* Hardwire for 3 axes and 3 buttons for now */
#define MAX_BUTTONS	3
#define MAX_AXES	3
#define FIRST_AXIS	0
#define FIRST_BUTTON	MAX_AXES

static int init_mouse (void)
{
   return 1;
}

static void close_mouse (void)
{
   return;
}

static int acquire_mouse (int num, int flags)
{
   return 1;
}

static void unacquire_mouse (int num)
{
   return;
}

static int get_mouse_num (void)
{
    return 1;
}

static char *get_mouse_name (int mouse)
{
    return 0;
}

static int get_mouse_widget_num (int mouse)
{
    return MAX_AXES + MAX_BUTTONS;
}

static int get_mouse_widget_first (int mouse, int type)
{
    switch (type) {
	case IDEV_WIDGET_BUTTON:
	    return FIRST_BUTTON;
	case IDEV_WIDGET_AXIS:
	    return FIRST_AXIS;
    }
    return -1;
}

static int get_mouse_widget_type (int mouse, int num, char *name, uae_u32 *code)
{
    if (num >= MAX_AXES && num < MAX_AXES + MAX_BUTTONS) {
	if (name)
	    sprintf (name, "Button %d", num + 1 + MAX_AXES);
	return IDEV_WIDGET_BUTTON;
    } else if (num < MAX_AXES) {
	if (name)
	    sprintf (name, "Axis %d", num + 1);
	return IDEV_WIDGET_AXIS;
    }
    return IDEV_WIDGET_NONE;
}

static void read_mouse (void)
{
    /* We handle mouse input in handle_events() */
}

struct inputdevice_functions inputdevicefunc_mouse = {
    init_mouse, close_mouse, acquire_mouse, unacquire_mouse, read_mouse,
    get_mouse_num, get_mouse_name,
    get_mouse_widget_num, get_mouse_widget_type,
    get_mouse_widget_first
};

/*
 * Default inputdevice config for BeOS mouse
 */
void input_get_default_mouse (struct uae_input_device *uid)
{
    /* Supports only one mouse */
    uid[0].eventid[ID_AXIS_OFFSET + 0][0]   = INPUTEVENT_MOUSE1_HORIZ;
    uid[0].eventid[ID_AXIS_OFFSET + 1][0]   = INPUTEVENT_MOUSE1_VERT;
    uid[0].eventid[ID_AXIS_OFFSET + 2][0]   = INPUTEVENT_MOUSE1_WHEEL;
    uid[0].eventid[ID_BUTTON_OFFSET + 0][0] = INPUTEVENT_JOY1_FIRE_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 1][0] = INPUTEVENT_JOY1_2ND_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 2][0] = INPUTEVENT_JOY1_3RD_BUTTON;
    uid[0].enabled = 1;
}

/*
 * Handle gfx specific cfgfile options
 */
void gfx_default_options (struct uae_prefs *p)
{
}

void gfx_save_options (FILE *f, struct uae_prefs *p)
{
}

int gfx_parse_option (struct uae_prefs *p, char *option, char *value)
{
	return 0;
}

/*
 * Misc functions
 */
void screenshot (int mode)
{
	write_log ("Screenshot not supported yet\n");
}

void framerate_up (void)
{
    if (currprefs.gfx_framerate < 20)
		changed_prefs.gfx_framerate = currprefs.gfx_framerate + 1;
}

void framerate_down (void)
{
    if (currprefs.gfx_framerate > 1)
	changed_prefs.gfx_framerate = currprefs.gfx_framerate - 1;
}

int debuggable (void)
{
	return true;
}

int needmousehack (void)
{
	return true;
}

void toggle_mousegrab (void)
{
}

int check_prefs_changed_gfx (void)
{
	return 0;
}

void toggle_fullscreen (void)
{
	gWin->SetFullScreenMode(0);
}

int is_fullscreen (void)
{
    return 0;
}
