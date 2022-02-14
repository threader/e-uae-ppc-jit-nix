
#include "sysconfig.h"
#include "sysdeps.h"

#include <windows.h>

#define SHOW_CONSOLE 0

static int consoleopen = 0;
static HANDLE stdinput,stdoutput;

FILE *debugfile = NULL;
int console_logging;

#define WRITE_LOG_BUF_SIZE 4096

/* console functions for debugger */

static void openconsole(void)
{
    if(consoleopen) return;
    AllocConsole();
    stdinput=GetStdHandle(STD_INPUT_HANDLE);
    stdoutput=GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleMode(stdinput,ENABLE_PROCESSED_INPUT|ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_OUTPUT);
    consoleopen = 1;
}

void console_out (const char *format,...)
{
    va_list parms;
    char buffer[WRITE_LOG_BUF_SIZE];
    DWORD temp;

    va_start (parms, format);
    _vsnprintf( buffer, WRITE_LOG_BUF_SIZE-1, format, parms );
    va_end (parms);
    openconsole();
    WriteConsole(stdoutput,buffer,strlen(buffer),&temp,0);
}

int console_get (char *out, int maxlen)
{
    DWORD len,totallen;

    totallen=0;
    while(maxlen>0) 
    {
	    ReadConsole(stdinput,out,1,&len,0);
	    if(*out == 13) break;
	    out++;
	    maxlen--;
	    totallen++;
    }
    *out=0;
    return totallen;
}

void console_flush (void)
{
}

#ifdef __GNUC_

/* GCC/EGCS wants this write_log in order to work from socket-land and to do traces */
void write_log (const char *format, ...)
{
    int result = 0;
    DWORD numwritten;
    char buffer[12];
    va_list parms;
    int count = 0;
    int *blah = NULL;

    if( debugfile )
    {
#if defined HAVE_GETTICKCOUNT && defined TIMESTAMP_LOGS
        {
            sprintf( buffer, "%7d - ", GetTickCount() );
            fprintf( debugfile, buffer );
        }
#endif
        va_start (parms, format);
        count = vfprintf( debugfile, format, parms );
        fflush( debugfile );
        if( count >= WRITE_LOG_BUF_SIZE-1 )
        {
            fprintf( debugfile, "SHIT in write_log()\n" );
            fflush( debugfile );
            *blah = 0; /* Access Violation here! */
            abort();
        }
	else
	    result = count;
        va_end (parms);
    }
}

#else

void write_log( const char *format, ... )
{
    int count;
    DWORD numwritten;
    char buffer[ WRITE_LOG_BUF_SIZE ];

    va_list parms;
    va_start (parms, format);
    count = _vsnprintf( buffer, WRITE_LOG_BUF_SIZE-1, format, parms );
    if (SHOW_CONSOLE || console_logging) {
	openconsole();
	WriteConsole(stdoutput,buffer,strlen(buffer),&numwritten,0);
    }
    if( debugfile ) {
	fprintf( debugfile, buffer );
	fflush (debugfile);
    }
    va_end (parms);
}

#endif

void f_out (void *f, const char *format, ...)
{
    int count;
    DWORD numwritten;
    char buffer[ WRITE_LOG_BUF_SIZE ];

    va_list parms;
    va_start (parms, format);
    count = _vsnprintf( buffer, WRITE_LOG_BUF_SIZE-1, format, parms );
    openconsole();
    WriteConsole(stdoutput,buffer,strlen(buffer),&numwritten,0);
    va_end (parms);
}
