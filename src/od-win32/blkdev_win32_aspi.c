 /*
  * UAE - The Un*x Amiga Emulator
  *
  * WIN32 CDROM/HD low level access code (ASPI)
  *
  * Copyright 2002 Toni Wilen
  *
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "memory.h"

#include "threaddep/thread.h"
#include "blkdev.h"
#include "scsidev.h"
#include "gui.h"

#include <wnaspi32.h>

#include "scsidef.h"

#define ASPI_DEBUG 0

int aspi_allow_misc = 1;
int aspi_allow_all = 0;

static int busses, AspiLoaded;
static DWORD (*pfnGetASPI32SupportInfo)(void);
static DWORD (*pfnSendASPI32Command)(LPSRB);
static BOOL (*pfnGetASPI32Buffer)(PASPI32BUFF);
static BOOL (*pfnFreeASPI32Buffer)(PASPI32BUFF);
static BOOL (*pfnTranslateASPI32Address)(PDWORD, PDWORD);
static HANDLE hAspiLib;

struct scsi_info {
    int scsibus,target,lun;
    int type;
    int mediainserted;
    uae_u8 *buf;
    char label[100];
    SCSI *handle;
};
static struct scsi_info si[MAX_TOTAL_DEVICES];
static int unitcnt;

static int ha_inquiry(SCSI *scgp, int id, SRB_HAInquiry *ip)
{
    DWORD Status;
	
    ip->SRB_Cmd	= SC_HA_INQUIRY;
    ip->SRB_HaId = id;
    ip->SRB_Flags = 0;
    ip->SRB_Hdr_Rsvd = 0;

    Status = pfnSendASPI32Command((LPSRB)ip);
#if ASPI_DEBUG
    write_log ("ASPI: S=%d ha=%d, ID=%d, M='%s', Id='%s'\n",
	Status, ip->HA_Count, ip->HA_SCSI_ID, ip->HA_ManagerId, ip->HA_Identifier);
#endif
    if (ip->SRB_Status != SS_COMP)
    	return -1;
    return 0;
}

static int open_driver (SCSI *scgp)
{
    DWORD	astatus;
    BYTE	HACount;
    BYTE	ASPIStatus;
    int	i;

    /*
     * Check if ASPI library is already loaded yet
     */
    if (AspiLoaded == TRUE)
	return TRUE;
    /*
     * Load the ASPI library
     */
    hAspiLib = LoadLibrary("WNASPI32");
    /*
     * Check if ASPI library is loaded correctly
     */
    if (hAspiLib == NULL) {
	write_log ("ASPI: failed to load wnaspi32.dll\n");
    	return FALSE;
    }
    /*
     * Get a pointer to GetASPI32SupportInfo function
     * and a pointer to SendASPI32Command function
     */
    pfnGetASPI32SupportInfo = (DWORD(*)(void))GetProcAddress(hAspiLib, "GetASPI32SupportInfo");
    pfnSendASPI32Command = (DWORD(*)(LPSRB))GetProcAddress(hAspiLib, "SendASPI32Command");

    if ((pfnGetASPI32SupportInfo == NULL) || (pfnSendASPI32Command == NULL)) {
	write_log ("ASPI: obsolete wnaspi32.dll found\n");
    	return FALSE;
    }

    pfnGetASPI32Buffer = (BOOL(*)(PASPI32BUFF))GetProcAddress(hAspiLib, "GetASPI32Buffer");
    pfnFreeASPI32Buffer = (BOOL(*)(PASPI32BUFF))GetProcAddress(hAspiLib, "FreeASPI32Buffer");
    pfnTranslateASPI32Address = (BOOL(*)(PDWORD, PDWORD))GetProcAddress(hAspiLib, "TranslateASPI32Address");

    /*
     * Set AspiLoaded variable
     */
    AspiLoaded = TRUE;

    astatus = pfnGetASPI32SupportInfo();

    ASPIStatus = HIBYTE(LOWORD(astatus));
    HACount    = LOBYTE(LOWORD(astatus));

    write_log ("ASPI: open_driver %X HostASPIStatus=0x%x HACount=0x%x\n", astatus, ASPIStatus, HACount);

    if (ASPIStatus != SS_COMP && ASPIStatus != SS_NO_ADAPTERS) {
    	write_log ("ASPI: Could not find any host adapters, ASPIStatus == 0x%02X", ASPIStatus);
	return FALSE;
    }
    busses = HACount;

    write_log ("ASPI: open_driver HostASPIStatus=0x%x HACount=0x%x\n", ASPIStatus, HACount);

    for (i=0; i < busses; i++) {
    	SRB_HAInquiry	s;
	ha_inquiry(scgp, i, &s);
    }
    return TRUE;
}


static void close_driver (void)
{
    if (!AspiLoaded) return;
    AspiLoaded	= FALSE;
    pfnGetASPI32SupportInfo = NULL;
    pfnSendASPI32Command = NULL;
    pfnGetASPI32Buffer	= NULL;
    pfnFreeASPI32Buffer	= NULL;
    pfnTranslateASPI32Address = NULL;
    FreeLibrary(hAspiLib);
    hAspiLib = NULL;
}


static int scsi_bufsize (SCSI *scgp, int amt)
{
    return 63*1024;
}

static int scsi_havebus (SCSI *scgp, int busno)
{
    if (busno < 0 || busno >= busses)
	return FALSE;
    return TRUE;
}

static void *scgo_getbuf (SCSI *scgp, long amt)
{
    scgp->bufbase = malloc((size_t)(amt));
    return scgp->bufbase;
}

static void scsi_sfree (SCSI *scgp)
{
    if (scgp->cmdstart)
    	free(scgp->cmdstart);
    if (scgp->cmdstop)
    	free(scgp->cmdstop);
    if (scgp->scmd)
    	free(scgp->scmd);
    if (scgp->inq)
    	free(scgp->inq);
    if (scgp->cap)
    	free(scgp->cap);
    if (scgp->local)
    	free(scgp->local);
    if (scgp->errstr)
    	free(scgp->errstr);
    free(scgp);
}

static SCSI *scsi_smalloc(void)
{
    SCSI *scgp;

    scgp = (SCSI *)malloc(sizeof(*scgp));
    if (scgp == NULL)
        return 0;

    memset (scgp, 0, sizeof (*scgp));
    scgp->deftimeout = 20;
    scgp->running = FALSE;

    scgp->cmdstart = (struct timeval *)malloc(sizeof(struct timeval));
    if (scgp->cmdstart == NULL)
    	goto err;
    scgp->cmdstop = (struct timeval *)malloc(sizeof(struct timeval));
    if (scgp->cmdstop == NULL)
	goto err;
    scgp->scmd = (struct scg_cmd *)malloc(sizeof(struct scg_cmd));
    if (scgp->scmd == NULL)
	goto err;
    scgp->errstr = malloc(SCSI_ERRSTR_SIZE);
    if (scgp->errstr == NULL)
	goto err;
    scgp->errptr = scgp->errbeg = scgp->errstr;
    scgp->errstr[0] = '\0';
    scgp->inq = (struct scsi_inquiry *)malloc(sizeof(struct scsi_inquiry));
    if (scgp->inq == NULL)
	goto err;
    scgp->cap = (struct scsi_capacity *)malloc(sizeof(struct scsi_capacity));
    if (scgp->cap == NULL)
	goto err;
    return scgp;
err:
    scsi_sfree(scgp);
    return 0;
}

#define	MAX_SCG		16	/* Max # of SCSI controllers	*/
#define	MAX_TGT		16	/* Max # of SCSI Targets	*/
#define	MAX_LUN		8	/* Max # of SCSI LUNs		*/

struct scg_local {
    int dummy;
};
#define scglocal(p)	((struct scg_local *)((p)->local)) 

static SCSI *openscsi (int busno, int tgt, int tlun)
{
    SCSI *scgp = scsi_smalloc ();

    if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {
    	errno = EINVAL;
#if ASPI_DEBUG
	write_log ("ASPI: Illegal value for busno, target or lun '%d,%d,%d'\n", busno, tgt, tlun);
#endif
	return 0;
    }
    /*
     *  Check if variables are within the range
     */
    if (tgt >= 0 && tgt >= 0 && tlun >= 0) {
    	/*
    	 * This is the non -scanbus case.
    	 */
	;
    } else if (tgt != -1 || tgt != -1 || tlun != -1) {
    	errno = EINVAL;
    	return 0;
    }
    if (scgp->local == NULL) {
	scgp->local = malloc(sizeof(struct scg_local));
	if (scgp->local == NULL)
	    return 0;
    }
    /*
     * Try to open ASPI-Router
     */
    if (!open_driver(scgp))
	return 0;
    /*
     * More than we have ...
     */
    if (busno >= busses) {
    	close_driver ();
	return 0;
    }
    return scgp;
}

static void closescsi (SCSI *scgp)
{
    close_driver ();
}

static void scsi_debug (SCSI *scgp, SRB_ExecSCSICmd *s)
{
#if ASPI_DEBUG
    int i;

    if (scgp->scmd->cdb.g0_cdb.cmd == 0x12)
	return;
    write_log ("ASPI EXEC_SCSI: cmd=%02.2X bus=%d,target=%d,lun=%d\n",scgp->scmd->cdb.g0_cdb.cmd,s->SRB_HaId,s->SRB_Target,s->SRB_Lun);
    for (i = 0; i < scgp->scmd->cdb_len; i++) {
	write_log ("%s%02.2X", i > 0 ? "." : "", scgp->scmd->cdb.cmd_cdb[i]);
    }
    write_log ("\n");
#endif
}


static void copy_sensedata(SRB_ExecSCSICmd *cp, struct scg_cmd *sp)
{
    sp->sense_count	= cp->SRB_SenseLen;
    if (sp->sense_count > sp->sense_len)
	sp->sense_count = sp->sense_len;
    memset(&sp->u_sense.Sense, 0x00, sizeof(sp->u_sense.Sense));
    if(sp->sense_len > 0) {
	int len = sp->sense_len;
	if (len > sizeof(sp->u_sense.Sense)) len = sizeof(sp->u_sense.Sense);
	memcpy(&sp->u_sense.Sense, cp->SenseArea, len);
#if ASPI_DEBUG
	{
	    int i;
	    write_log ("ASPI SENSE:");
	    for (i = 0; i < len; i++) {
		write_log ("%s%02.2X", i > 0 ? "." : "", cp->SenseArea[i]);
	    }
	    write_log ("\n");
	}
#endif
    }
    sp->u_scb.cmd_scb[0] = cp->SRB_TargStat;
}

/*
 * Set error flags
 */
static void set_error(SRB_ExecSCSICmd *cp, struct scg_cmd *sp)
{
    switch (cp->SRB_Status) {
	case SS_COMP:			/* 0x01 SRB completed without error  */
	sp->error = SCG_NO_ERROR;
	sp->ux_errno = 0;
	break;
	case SS_PENDING:		/* 0x00 SRB being processed          */
	/*
	 * XXX Could SS_PENDING happen ???
	 */
	case SS_ABORTED:		/* 0x02 SRB aborted                  */
	case SS_ABORT_FAIL:		/* 0x03 Unable to abort SRB          */
	case SS_ERR:			/* 0x04 SRB completed with error     */
	default:
	sp->error = SCG_RETRYABLE;
	sp->ux_errno = EIO;
	break;
	case SS_INVALID_CMD:		/* 0x80 Invalid ASPI command         */
	case SS_INVALID_HA:		/* 0x81 Invalid host adapter number  */
	case SS_NO_DEVICE:		/* 0x82 SCSI device not installed    */
	case SS_INVALID_SRB:		/* 0xE0 Invalid parameter set in SRB */
	case SS_ILLEGAL_MODE:		/* 0xE2 Unsupported Windows mode     */
	case SS_NO_ASPI:		/* 0xE3 No ASPI managers             */
	case SS_FAILED_INIT:		/* 0xE4 ASPI for windows failed init */
	case SS_MISMATCHED_COMPONENTS:	/* 0xE7 The DLLs/EXEs of ASPI don't  */
					/*      version check                */
	case SS_NO_ADAPTERS:		/* 0xE8 No host adapters to manager  */
	
	case SS_ASPI_IS_SHUTDOWN:	/* 0xEA Call came to ASPI after      */
					/*      PROCESS_DETACH               */
	case SS_BAD_INSTALL:		/* 0xEB The DLL or other components  */
					/*      are installed wrong          */
	sp->error = SCG_FATAL;
	sp->ux_errno = EINVAL;
	break;
	case SS_BUFFER_ALIGN:		/* 0xE1 Buffer not aligned (replaces */
					/*      SS_OLD_MANAGER in Win32)     */
	sp->error = SCG_FATAL;
	sp->ux_errno = EFAULT;
	break;

	case SS_ASPI_IS_BUSY:		/* 0xE5 No resources available to    */
					/*      execute command              */
	sp->error = SCG_RETRYABLE;
	sp->ux_errno = EBUSY;
	break;

	case SS_BUFFER_TO_BIG:		/* 0xE6 Correct spelling of 'too'    */
	case SS_INSUFFICIENT_RESOURCES:	/* 0xE9 Couldn't allocate resources  */
					/*      needed to init               */
	sp->error = SCG_RETRYABLE;
	sp->ux_errno = ENOMEM;
	break;
    }
}
static int scsiabort(SCSI *scgp, SRB_ExecSCSICmd *sp)
{
    DWORD Status = 0;
    SRB_Abort s;

#if ASPI_DEBUG
    write_log ("ASPI: Attempting to abort SCSI command\n");
#endif
    /*
     * Set structure variables
     */
    s.SRB_Cmd	= SC_ABORT_SRB;			/* ASPI command code = SC_ABORT_SRB	*/
    s.SRB_HaId	= scg_scsibus(scgp);		/* ASPI host adapter number		*/
    s.SRB_Flags	= 0;				/* Flags				*/
    s.SRB_ToAbort	= (LPSRB)&sp;			/* sp					*/
    /*
     * Initiate SCSI abort
     */
    Status = pfnSendASPI32Command((LPSRB)&s);
    /*
     * Check condition
     */
    if (s.SRB_Status != SS_COMP) {
#if ASPI_DEBUG
	write_log ("ASPI: Abort ERROR! 0x%08X\n", s.SRB_Status);
#endif
	return FALSE;
    }
#if ASPI_DEBUG
    write_log ("ASPI: Abort SCSI command completed\n");
#endif
    /*
     * Everything went OK
     */
    return TRUE;
}

static int scsicmd(SCSI *scgp)
{
    struct scg_cmd *sp = scgp->scmd;
    DWORD Status = 0;
    DWORD EventStatus = WAIT_OBJECT_0;
    HANDLE Event = NULL;
    SRB_ExecSCSICmd s;

    /*
     * Initialize variables
     */
    sp->error = SCG_NO_ERROR;
    sp->sense_count = 0;
    sp->u_scb.cmd_scb[0] = 0;
    sp->resid = 0;

    memset(&s, 0, sizeof(s)); /* Clear SRB structure */

    /*
     * Check cbd_len > the maximum command pakket that can be handled by ASPI
     */
    if (sp->cdb_len > 16) {
    	sp->error = SCG_FATAL;
    	sp->ux_errno = EINVAL;
#if ASPI_DEBUG
    	write_log ("ASPI: sp->cdb_len > sizeof(SRB_ExecSCSICmd.CDBByte). Fatal error in scgo_send, exiting...\n");
#endif
    	return -1;
    }
    /*
     * copy command into SRB
     */
    memcpy(&(s.CDBByte), &sp->cdb, sp->cdb_len);

    Event = CreateEvent(NULL, TRUE, FALSE, NULL);

    /*
     * Fill ASPI structure
     */
    s.SRB_Cmd	= SC_EXEC_SCSI_CMD; /* SCSI Command */
    s.SRB_HaId	= scg_scsibus(scgp); /* Host adapter number */
    s.SRB_Flags	= SRB_EVENT_NOTIFY; /* Flags */
    s.SRB_Target	= scg_target(scgp); /* Target SCSI ID */
    s.SRB_Lun	= scg_lun(scgp); /* Target SCSI LUN */
    s.SRB_BufLen	= sp->size; /* # of bytes transferred */
    s.SRB_BufPointer= sp->addr;	/* pointer to data buffer */
    s.SRB_CDBLen	= sp->cdb_len; 	/* SCSI command length */
    s.SRB_PostProc	= Event; /* Post proc event */
    s.SRB_SenseLen	= SENSE_LEN; /* Lenght of sense buffer */
    
    scsi_debug (scgp,&s);

    /*
     * Do we receive data from this ASPI command?
     */
    if (sp->flags & SCG_RECV_DATA) {
	s.SRB_Flags |= SRB_DIR_IN;
    } else {
    	/*
    	 * Set direction to output
    	 */
    	if (sp->size > 0)
    	    s.SRB_Flags |= SRB_DIR_OUT;
    }

    /*
     * ------------ Send SCSI command --------------------------
     */

    ResetEvent (Event); /* Clear event handle */
    Status = pfnSendASPI32Command((LPSRB)&s);/* Initiate SCSI command  */

    if (Status == SS_PENDING) { /* If in progress */
    	/*
    	 * Wait until command completes, or times out.
    	 */
    	EventStatus = WaitForSingleObject(Event, sp->timeout * 1000);
	if (EventStatus == WAIT_OBJECT_0)
	    ResetEvent(Event);	/* Clear event, time out */
	if (s.SRB_Status == SS_PENDING) {/* Check if we got a timeout*/
	    scsiabort(scgp, &s);
	    ResetEvent(Event);	/* Clear event, time out */
	    CloseHandle(Event);	/* Close the event handle */
	    sp->error = SCG_TIMEOUT;
	    return 1;		/* Return error	*/
	}
    }
    CloseHandle (Event); /* Close the event handle  */

    /*
     * Check ASPI command status
     */
    if (s.SRB_Status != SS_COMP) {
#if ASPI_DEBUG
	if (s.SRB_Status != 0x82)
	    write_log ("ASPI: Error in scgo_send: s.SRB_Status is 0x%x\n", s.SRB_Status);
#endif
	set_error(&s, sp); /* Set error flags */
	copy_sensedata(&s, sp); /* Copy sense and status */
#if ASPI_DEBUG
	if (s.SRB_Status != 0x82)
	    write_log ("ASPI: Mapped to: error %d errno: %d\n", sp->error, sp->ux_errno);
#endif
	return 1;
    }
    /*
     * Return success
     */
    return 0;
}

static int inquiry (SCSI *scgp, void *bp, int cnt)
{
    struct scg_cmd *scmd = scgp->scmd;

    memset(bp, 0, cnt);
    memset(scmd, 0, sizeof(struct scg_cmd));
    scmd->addr = bp;
    scmd->size = cnt;
    scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
    scmd->cdb_len = SC_G0_CDBLEN;
    scmd->sense_len = CCS_SENSE_LEN;
    scmd->target = scgp->addr.target;
    scmd->cdb.g0_cdb.cmd = SC_INQUIRY;
    scmd->cdb.g0_cdb.lun = scgp->addr.lun;
    scmd->cdb.g0_cdb.count = cnt;
    scgp->scmd->timeout = 10 * 60;
    if (scsicmd(scgp))
        return -1;
    return 0;
}

static int scsierr(SCSI *scgp)
{
    register struct scg_cmd *cp = scgp->scmd;

    if(cp->error != SCG_NO_ERROR ||
       cp->ux_errno != 0 || *(u_char *)&cp->scb != 0)
        return -1;
    return 0;
}

static void scan_scsi_bus (SCSI *scgp, int flags)
{
    /* add all units we find */
    for (scgp->addr.scsibus=0; scgp->addr.scsibus < 8; scgp->addr.scsibus++) {
        if (!scsi_havebus(scgp, scgp->addr.scsibus))
            continue;
        for (scgp->addr.target=0; scgp->addr.target < 16; scgp->addr.target++) {
            struct scsi_inquiry inq;
            scgp->addr.lun = 0;
            if (inquiry (scgp, &inq, sizeof(inq)))
                continue;
            for (scgp->addr.lun=0; scgp->addr.lun < 8; scgp->addr.lun++) {
                if (!inquiry (scgp, &inq, sizeof(inq))) {
		    write_log ("ASPI: %d:%d:%d ", scgp->addr.scsibus,scgp->addr.target,scgp->addr.lun);
		    write_log ("'%.8s' ", inq.vendor_info);
		    write_log ("'%.16s' ", inq.prod_ident);
		    write_log ("'%.4s' ", inq.prod_revision);
		    if (unitcnt < MAX_TOTAL_DEVICES) {
			struct scsi_info *cis = &si[unitcnt];
			int use = 0;
			if (inq.type == INQ_ROMD) {
			    write_log (" [CDROM]");
			    use = 1;
			} else if (!flags && ((inq.type >= INQ_SEQD && inq.type < INQ_COMM && inq.type != INQ_PROCD && aspi_allow_misc) || aspi_allow_all)) {
			    write_log (" [%d]", inq.type);
			    use = 1;
			}
			if (use) {
			    unitcnt++;
			    cis->buf = malloc (DEVICE_SCSI_BUFSIZE);
			    cis->scsibus = scgp->addr.scsibus;
			    cis->target = scgp->addr.target;
			    cis->lun = scgp->addr.lun;
			    cis->type = inq.type;
			    sprintf (cis->label, "%.8s %.16s %.4s", inq.vendor_info, inq.prod_ident, inq.prod_revision);
			}
		    }
		    write_log ("\n");
		}
            }
        }
    }
}

static void aspi_led (int unitnum)
{
    int type = si[unitnum].type;

    if (type == INQ_ROMD)
	gui_cd_led (1);
    else if (type == INQ_DASD)
	gui_hd_led (1);
}

static uae_sem_t scgp_sem;

static uae_u8 *execscsicmd_out (int unitnum, uae_u8 *data, int len)
{
    SCSI *scgp = si[unitnum].handle;
    int v;

    uae_sem_wait (&scgp_sem);
    scgp->scmd->cdb_len = len;
    memcpy (scgp->scmd->cdb.cmd_cdb, data, len);
    scgp->scmd->addr = 0;
    scgp->scmd->size = 0;
    scgp->addr.scsibus = si[unitnum].scsibus;
    scgp->addr.target = si[unitnum].target;
    scgp->addr.lun = si[unitnum].lun;
    scgp->scmd->timeout = 80 * 60;
    aspi_led (unitnum);
    v = scsicmd (scgp);
    aspi_led (unitnum);
    uae_sem_post (&scgp_sem);
    if (v)
	return 0;
    return data;
}

static uae_u8 *execscsicmd_in (int unitnum, uae_u8 *data, int len, int *outlen)
{
    SCSI *scgp = si[unitnum].handle;
    int v;

    uae_sem_wait (&scgp_sem);
    scgp->scmd->cdb_len = len;
    memcpy (scgp->scmd->cdb.cmd_cdb, data, len);
    scgp->scmd->addr = si[unitnum].buf;
    scgp->scmd->size = DEVICE_SCSI_BUFSIZE;
    scgp->addr.scsibus = si[unitnum].scsibus;
    scgp->addr.target = si[unitnum].target;
    scgp->addr.lun = si[unitnum].lun;
    scgp->scmd->timeout = 80 * 60;
    aspi_led (unitnum);
    v = scsicmd (scgp);
    aspi_led (unitnum);
    uae_sem_post (&scgp_sem);
    if (v)
	return 0;
    if (outlen)
	*outlen = scgp->scmd->size;
    return si[unitnum].buf;
}

static SCSI *scsi_handle;

static int open_scsi_bus (int flags)
{
    SCSI *scgp = openscsi (-1, -1, -1);
    unitcnt = 0;
    if (scgp) {
	scan_scsi_bus (scgp, flags);
        uae_sem_init (&scgp_sem, 0, 1);
    }
    scsi_handle = scgp;
    return scgp ? 1 : 0;
}

static int mediacheck (int unitnum)
{
    uae_u8 cmd [6] = { 0,0,0,0,0,0 }; /* TEST UNIT READY */
    if (si[unitnum].handle == 0)
	return 0;
    return execscsicmd_out (unitnum, cmd, sizeof(cmd)) ? 1 : 0;
}

static int open_scsi_device (int unitnum)
{
    if (unitnum >= unitcnt)
	return 0;
#if ASPI_DEBUG
    write_log ("ASPI: opening %d:%d:%d\n", si[unitnum].scsibus, si[unitnum].target, si[unitnum].lun);
#endif
    si[unitnum].handle = openscsi (si[unitnum].scsibus, si[unitnum].target, si[unitnum].lun);
    if (si[unitnum].handle)
	si[unitnum].mediainserted = mediacheck (unitnum);
#if ASPI_DEBUG
    write_log ("unit %d: %s\n", unitnum, si[unitnum].mediainserted ? "CD inserted" : "Drive empty");
#endif
    return si[unitnum].handle ? 1 : 0;
}

static void close_scsi_device (int unitnum)
{
    if (unitnum >= unitcnt)
	return;
    if (!si[unitnum].handle)
	return;
    scsi_sfree (si[unitnum].handle);
    si[unitnum].handle = 0;
}

static void close_scsi_bus (void)
{
    closescsi (scsi_handle);
    scsi_handle = 0;
}

static int execscsicmd_direct (int unitnum, uaecptr acmd)
{
    int sactual = 0, i;
    SCSI *scgp = si[unitnum].handle;
    struct scg_cmd *scmd = scgp->scmd;
    uaecptr scsi_data = get_long (acmd + 0);
    uae_u32 scsi_len = get_long (acmd + 4);
    uaecptr scsi_cmd = get_long (acmd + 12);
    uae_u16 scsi_cmd_len = get_word (acmd + 16);
    uae_u8 scsi_flags = get_byte (acmd + 20);
    uaecptr scsi_sense = get_long (acmd + 22);
    uae_u16 scsi_sense_len = get_word (acmd + 26);

    int io_error = 0;
    addrbank *bank_data = &get_mem_bank (scsi_data);
    /* do transfer directly to and from Amiga memory */
    if (!bank_data || !bank_data->check (scsi_data, scsi_len))
        return -5; /* IOERR_BADADDRESS */

    uae_sem_wait (&scgp_sem);

    /* the Amiga does not tell us how long the timeout shall be, so make it _very_ long (specified in seconds) */
    scmd->timeout = 80 * 60;
    scmd->addr = scsi_len ? bank_data->xlateaddr (scsi_data) : 0;
    scmd->size = scsi_len;
    scmd->flags = ((scsi_flags & 1) ? SCG_RECV_DATA : 0) | SCG_DISRE_ENA;
    scmd->cdb_len = scsi_cmd_len;
    for (i = 0; i < scsi_cmd_len; i++)
	scmd->cdb.cmd_cdb[i] = get_byte (scsi_cmd + i);
    scmd->target = si[unitnum].target;
    scmd->sense_len = (scsi_flags & 4) ? 4 : /* SCSIF_OLDAUTOSENSE */
        (scsi_flags & 2) ? scsi_sense_len : /* SCSIF_AUTOSENSE */
        -1;
    scmd->sense_count = 0;
    scmd->u_scb.cmd_scb[0] = 0;
    scgp->addr.scsibus = si[unitnum].scsibus;
    scgp->addr.target  = si[unitnum].target;
    scgp->addr.lun     = si[unitnum].lun;
    scsicmd (scgp);

    put_word (acmd + 18, scmd->error == SCG_FATAL ? 0 : scsi_cmd_len); /* fake scsi_CmdActual */
    put_byte (acmd + 21, scmd->u_scb.cmd_scb[0]); /* scsi_Status */
    if (scmd->u_scb.cmd_scb[0]) {
        io_error = 45; /* HFERR_BadStatus */
        /* copy sense? */
        for (sactual = 0; scsi_sense && sactual < scsi_sense_len && sactual < scmd->sense_count; sactual++)
            put_byte (scsi_sense + sactual, scmd->u_sense.cmd_sense[sactual]);
        put_long (acmd + 8, 0); /* scsi_Actual */
    } else {
        int i;
        for (i = 0; i < scsi_sense_len; i++)
            put_byte (scsi_sense + i, 0);
        sactual = 0;
        if (scmd->error != SCG_NO_ERROR) {
	    io_error = 20; /* io_Error, but not specified */
	    put_long (acmd + 8, 0); /* scsi_Actual */
        } else {
	    io_error = 0;
            put_long (acmd + 8, scsi_len - scmd->resid); /* scsi_Actual */
        }
    }
    put_word (acmd + 28, sactual);

    uae_sem_post (&scgp_sem);
    return io_error;
}

static struct device_info *info_device (int unitnum, struct device_info *di)
{
    if (unitnum >= unitcnt)
	return 0;
    di->bus = si[unitnum].scsibus;
    di->target = si[unitnum].target;
    di->lun = si[unitnum].lun;
    di->media_inserted = mediacheck (unitnum); //si[unitnum].mediainserted;
    di->write_protected = 1;
    di->bytespersector = 2048;
    di->cylinders = 1;
    di->type = si[unitnum].type;
    di->id = unitnum + 1;
    strcpy (di->label, si[unitnum].label);
    return di;
}

void win32_aspi_media_change (char driveletter, int insert)
{
    int i, now;

    for (i = 0; i < MAX_TOTAL_DEVICES; i++) {
	now = mediacheck (i);
	if (now != si[i].mediainserted) {
	    write_log ("ASPI: media change %c %d\n", driveletter, insert);
	    si[i].mediainserted = now;
	    scsi_do_disk_change (i + 1, insert);
	}
    }
}

struct device_functions devicefunc_win32_aspi = {
    open_scsi_bus, close_scsi_bus, open_scsi_device, close_scsi_device, info_device,
    execscsicmd_out, execscsicmd_in, execscsicmd_direct,
    0, 0, 0, 0, 0, 0
};
