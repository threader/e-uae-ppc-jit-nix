/*
 * PUAE - The *nix Amiga Emulator
 *
 * QT GUI for PUAE
 *
 * Copyright 2010 Mustafa 'GnoStiC' TUFAN
 * (GUI layout cloned from WinUAE/Toni Wilen)
 *
 */

#include "puae_mainwindow.h"
#include "ui_puae_mainwindow.h"

#include <QMessageBox>
#include <QDir>
#include <QFileDialog>

extern "C" {
#include "include/options.h"
struct uae_prefs workprefs;
}


// Paths Tab
QString PATHS_ROM, PATHS_CONFIG, PATHS_SCREENSHOT, PATHS_SAVESTATE, PATHS_AVIOUTPUT, PATHS_SAVEIMAGE, PATHS_RIP;

// mem
static const char *memsize_names[] = {
        /* 0 */ "none",
        /* 1 */ "256 K",
        /* 2 */ "512 K",
        /* 3 */ "1 MB",
        /* 4 */ "2 MB",
        /* 5 */ "4 MB",
        /* 6 */ "8 MB",
        /* 7 */ "16 MB",
        /* 8 */ "32 MB",
        /* 9 */ "64 MB",
        /* 10*/ "128 MB",
        /* 11*/ "256 MB",
        /* 12*/ "512 MB",
        /* 13*/ "1 GB",
        /* 14*/ "1.5MB",
        /* 15*/ "1.8MB",
        /* 16*/ "2 GB",
        /* 17*/ "384 MB",
        /* 18*/ "768 MB",
        /* 19*/ "1.5 GB",
        /* 20*/ "2.5 GB",
        /* 21*/ "3 GB"
};

static unsigned long memsizes[] = {
        /* 0 */ 0,
        /* 1 */ 0x00040000, /* 256K */
        /* 2 */ 0x00080000, /* 512K */
        /* 3 */ 0x00100000, /* 1M */
        /* 4 */ 0x00200000, /* 2M */
        /* 5 */ 0x00400000, /* 4M */
        /* 6 */ 0x00800000, /* 8M */
        /* 7 */ 0x01000000, /* 16M */
        /* 8 */ 0x02000000, /* 32M */
        /* 9 */ 0x04000000, /* 64M */
        /* 10*/ 0x08000000, //128M
        /* 11*/ 0x10000000, //256M
        /* 12*/ 0x20000000, //512M
        /* 13*/ 0x40000000, //1GB
        /* 14*/ 0x00180000, //1.5MB
        /* 15*/ 0x001C0000, //1.8MB
        /* 16*/ 0x80000000, //2GB
        /* 17*/ 0x18000000, //384M
        /* 18*/ 0x30000000, //768M
        /* 19*/ 0x60000000, //1.5GB
        /* 20*/ 0xA8000000, //2.5GB
        /* 21*/ 0xC0000000, //3GB
};

static int msi_chip[] = { 1, 2, 3, 14, 4, 5, 6 };
static int msi_bogo[] = { 0, 2, 3, 14, 15 };
static int msi_fast[] = { 0, 3, 4, 5, 6 };
static int msi_z3fast[] = { 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 17, 12, 18, 13, 19, 16, 20, 21 };
static int msi_z3chip[] = { 0, 7, 8, 9, 10, 11, 12, 13 };
static int msi_gfx[] = { 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };

puae_MainWindow::puae_MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::puae_MainWindow)
{
    ui->setupUi(this);

    QString myPath;
    myPath = QDir::currentPath();

    // Paths Tab
    PATHS_ROM = myPath;
    PATHS_CONFIG = myPath;
    PATHS_SCREENSHOT = myPath;
    PATHS_SAVESTATE = myPath;
    PATHS_AVIOUTPUT = myPath;
    PATHS_SAVEIMAGE = myPath;
    PATHS_RIP = myPath;
}

puae_MainWindow::~puae_MainWindow()
{
    delete ui;
}

//
//
//
QString puae_MainWindow::GetPath(QWidget *who, QString what, QString where)
{
    QString path = QFileDialog::getExistingDirectory(who, what, where, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if ( path.isNull() ) {
        path = "./";
    }

    return path;
}

QString puae_MainWindow::GetFile(QWidget *who, QString what, QString extensions)
{
    QString file = QFileDialog::getOpenFileName(who, what, PATHS_ROM, extensions);

//    if ( !file.isNull() ) {
    return file;
}

//*************************************************
// PATHS TAB
//*************************************************

/* Choose System ROMS Path */
void puae_MainWindow::on_IDC_PATHS_ROMS_clicked()
{
    PATHS_ROM = GetPath(this, "Select System ROMS Path", PATHS_ROM);
    ui->IDC_PATHS_ROM->setText (PATHS_ROM);
}

/* Choose Configuration Files Path */
void puae_MainWindow::on_IDC_PATHS_CONFIGS_clicked()
{
    PATHS_CONFIG = GetPath(this, "Select Config Path", PATHS_CONFIG);
    ui->IDC_PATHS_CONFIG->setText (PATHS_CONFIG);
}

/* Choose Screenshot Path */
void puae_MainWindow::on_IDC_PATHS_SCREENSHOTS_clicked()
{
    PATHS_SCREENSHOT = GetPath(this, "Select Screenshot Path", PATHS_SCREENSHOT);
    ui->IDC_PATHS_SCREENSHOT->setText (PATHS_SCREENSHOT);
}

/* Choose Savestate Path */
void puae_MainWindow::on_IDC_PATHS_SAVESTATES_clicked()
{
    PATHS_SAVESTATE = GetPath(this, "Select Savestate Path", PATHS_SAVESTATE);
    ui->IDC_PATHS_SAVESTATE->setText (PATHS_SAVESTATE);
}

/* Choose AVI Output Path */
void puae_MainWindow::on_IDC_PATHS_AVIOUTPUTS_clicked()
{
    PATHS_AVIOUTPUT = GetPath(this, "Select AVI Output Path", PATHS_AVIOUTPUT);
    ui->IDC_PATHS_AVIOUTPUT->setText (PATHS_AVIOUTPUT);
}

/* Choose Save Images Path */
void puae_MainWindow::on_IDC_PATHS_SAVEIMAGES_clicked()
{
    PATHS_SAVEIMAGE = GetPath(this, "Select Save Image Path", PATHS_SAVEIMAGE);
    ui->IDC_PATHS_SAVEIMAGE->setText (PATHS_SAVEIMAGE);
}

/* Choose RIP Path */
void puae_MainWindow::on_IDC_PATHS_RIPS_clicked()
{
    PATHS_RIP = GetPath(this, "Select RIP Path", PATHS_RIP);
    ui->IDC_PATHS_RIP->setText (PATHS_RIP);
}

//*************************************************
// ROMS TAB
//*************************************************

/* Choose Main ROM File */
void puae_MainWindow::on_IDC_KICKCHOOSER_clicked()
{
    QString fileName = GetFile (this, "Select Main ROM", "ROM Files (*.rom)");
    //ui->IDC_ROMFILE->setText(fileName);

}

/* Choose Extended ROM File */
void puae_MainWindow::on_IDC_ROMCHOOSER2_clicked()
{
    QString fileName = GetFile (this, "Select Extended ROM", "ROM Files (*.rom)");

}

/* Choose Cartridge ROM File */
void puae_MainWindow::on_IDC_CARTCHOOSER_clicked()
{
    QString fileName = GetFile (this, "Select Cartridge ROM File", "ROM Files (*.rom)");

}

/* Choose Flash RAM File */
void puae_MainWindow::on_IDC_FLASHCHOOSER_clicked()
{
    QString fileName = GetFile (this, "Select Flash RAM File", "RAM Files (*.ram)");

}

/* Map ROM Emulation */
void puae_MainWindow::on_IDC_MAPROM_toggled(bool ischecked)
{
    workprefs.maprom = ischecked ? 0x0f000000 : 0;
}

/* Shapeshifter support */
void puae_MainWindow::on_IDC_KICKSHIFTER_toggled(bool ischecked)
{
    workprefs.kickshifter = ischecked;
}

//*************************************************
// HW-CPU
//*************************************************

/* CPU 68000 */
void puae_MainWindow::on_IDC_CPU0_clicked()
{
    workprefs.cpu_model = 0;
}

/* CPU 68010 */
void puae_MainWindow::on_IDC_CPU1_clicked()
{
    workprefs.cpu_model = 1;
}

/* CPU 68020 */
void puae_MainWindow::on_IDC_CPU2_clicked()
{
    workprefs.cpu_model = 2;
}

/* CPU 68030 */
void puae_MainWindow::on_IDC_CPU3_clicked()
{
    workprefs.cpu_model = 3;
}

/* CPU 68040 */
void puae_MainWindow::on_IDC_CPU4_clicked()
{
    workprefs.cpu_model = 4;
}

/* CPU 68060 */
void puae_MainWindow::on_IDC_CPU5_clicked()
{
    workprefs.cpu_model = 5;
}

/* 24-bit Addressing */
void puae_MainWindow::on_IDC_COMPATIBLE24_toggled(bool ischecked)
{
    workprefs.address_space_24 = ischecked;
}

/* More Compatible */
void puae_MainWindow::on_IDC_COMPATIBLE_toggled(bool ischecked)
{
    workprefs.cpu_compatible = ischecked;
}

/* JIT Enable */
void puae_MainWindow::on_IDC_JITENABLE_toggled(bool ischecked)
{
//    cachesize_prev = workprefs.cachesize;
//    trust_prev = workprefs.comptrustbyte;

    workprefs.cachesize = 0;
}

/*68040 MMU Enable */
void puae_MainWindow::on_IDC_MMUENABLE_toggled(bool ischecked)
{
    //workprefs
}

/* FPU None */
void puae_MainWindow::on_IDC_FPU0_clicked()
{
    workprefs.mmu_model = 0;
}

/* FPU 68881 */
void puae_MainWindow::on_IDC_FPU1_clicked()
{
    workprefs.mmu_model = 1;
}

/* FPU 68882 */
void puae_MainWindow::on_IDC_FPU2_clicked()
{
    workprefs.mmu_model = 2;
}

/* FPU Internal */
void puae_MainWindow::on_IDC_FPU3_clicked()
{
    workprefs.mmu_model = 3;
}

/* FPU More Compatible */
void puae_MainWindow::on_IDC_COMPATIBLE_FPU_toggled(bool ischecked)
{
    //workprefs.fpu_strict = ischecked;
}

/* CPU Fastest Possible */
void puae_MainWindow::on_IDC_CS_HOST_clicked()
{
    workprefs.m68k_speed = -1;
}

/* CPU Cycle-Exact */
void puae_MainWindow::on_IDC_CS_68000_clicked()
{
    workprefs.m68k_speed = 0;
}

/* CPU Adjustable */
void puae_MainWindow::on_IDC_CS_ADJUSTABLE_clicked()
{
    workprefs.m68k_speed = 1;
}

/* CPU CPU<-->Chipset */
void puae_MainWindow::on_IDC_SPEED_valueChanged(int value)
{

}

/* CPU CPU Idle*/
void puae_MainWindow::on_IDC_CPUIDLE_sliderMoved(int position)
{

}

/* CPU CE */
void puae_MainWindow::on_IDC_CPU_FREQUENCY_currentIndexChanged(int index)
{

}

//*************************************************
// HW-Chipset
//*************************************************

/* OCS */
void puae_MainWindow::on_IDC_OCS_clicked()
{
    workprefs.chipset_mask = 0;
}

/* ECS AGNUS */
void puae_MainWindow::on_IDC_ECS_AGNUS_clicked()
{
    workprefs.chipset_mask = 1;
}

/* ECS DENISE */
void puae_MainWindow::on_IDC_ECS_DENISE_clicked()
{
    workprefs.chipset_mask = 2;
}

/* ECS FULL = AGNUS + DENISE */
void puae_MainWindow::on_IDC_ECS_clicked()
{
    workprefs.chipset_mask = 3;
}

/* AGA */
void puae_MainWindow::on_IDC_AGA_clicked()
{
    workprefs.chipset_mask = 4;
}

/* NTSC */
void puae_MainWindow::on_IDC_NTSC_toggled(bool ischecked)
{
    workprefs.ntscmode = ischecked;
}

/* Chipset Extra */
void puae_MainWindow::on_IDC_CS_EXT_currentIndexChanged(int index)
{

}

/* Blitter Immediate */
void puae_MainWindow::on_IDC_BLITIMM_toggled(bool ischecked)
{
    workprefs.immediate_blits = ischecked;
}

/* Cycle Exact */
void puae_MainWindow::on_IDC_CYCLEEXACT_toggled(bool ischecked)
{
    workprefs.cpu_cycle_exact = ischecked;
}

/* Genlock Enabled */
void puae_MainWindow::on_IDC_GENLOCK_toggled(bool ischecked)
{
    workprefs.genlock = ischecked;
}

/* Sprite Collision None */
void puae_MainWindow::on_IDC_COLLISION0_clicked()
{
    workprefs.collision_level = 0;
}

/* Sprites Only */
void puae_MainWindow::on_IDC_COLLISION1_clicked()
{
    workprefs.collision_level = 1;
}

/* Sprites and Sprites vs Playfield */
void puae_MainWindow::on_IDC_COLLISION2_clicked()
{
    workprefs.collision_level = 2;
}

/* Collision Full */
void puae_MainWindow::on_IDC_COLLISION3_clicked()
{
    workprefs.collision_level = 3;
}

/* Sound Disabled */
void puae_MainWindow::on_IDC_CS_SOUND0_clicked()
{
    workprefs.produce_sound = 0;
}

/* Sound Emulated */
void puae_MainWindow::on_IDC_CS_SOUND1_clicked()
{
    workprefs.produce_sound = 1;
}

/* Sound Emulated %100 */
void puae_MainWindow::on_IDC_CS_SOUND2_clicked()
{
    workprefs.produce_sound = 2;
}

//*************************************************
// Advanced Chipset
//*************************************************

/* Compatible Settings */
void puae_MainWindow::on_IDC_CS_COMPATIBLE_toggled(bool ischecked)
{
    workprefs.cs_compatible = ischecked;
}

/* Battery Backed Real Time Clock None */
void puae_MainWindow::on_IDC_CS_RTC1_clicked()
{
    workprefs.cs_rtc = 0;
}

/* Battery Backed Real Time Clock None */
void puae_MainWindow::on_IDC_CS_RTC2_clicked()
{
    workprefs.cs_rtc = 1;
}

/* Battery Backed Real Time Clock None */
void puae_MainWindow::on_IDC_CS_RTC3_clicked()
{
    workprefs.cs_rtc = 2;
}

/* CIA-A TOD Clock Source */
void puae_MainWindow::on_IDC_CS_CIAA_TOD1_clicked()
{
    workprefs.cs_ciaatod = 0;
}

/* CIA-A TOD Clock Source */
void puae_MainWindow::on_IDC_CS_CIAA_TOD2_clicked()
{
    workprefs.cs_ciaatod = 1;
}

/* CIA-A TOD Clock Source */
void puae_MainWindow::on_IDC_CS_CIAA_TOD3_clicked()
{
    workprefs.cs_ciaatod = 2;
}

/* CIA ROM Overlay */
void puae_MainWindow::on_IDC_CS_CIAOVERLAY_toggled(bool ischecked)
{
    workprefs.cs_ciaoverlay = ischecked;
}

/* CD32 CD */
void puae_MainWindow::on_IDC_CS_CD32CD_toggled(bool ischecked)
{
    workprefs.cs_cd32cd = ischecked;
}


void puae_MainWindow::on_IDC_CS_CD32C2P_toggled(bool ischecked)
{
    workprefs.cs_cd32c2p = ischecked;
}

void puae_MainWindow::on_IDC_CS_CD32NVRAM_toggled(bool ischecked)
{
    workprefs.cs_cd32nvram = ischecked;
}

void puae_MainWindow::on_IDC_CS_CDTVCD_toggled(bool ischecked)
{
    workprefs.cs_cdtvcd = ischecked;
}

void puae_MainWindow::on_IDC_CS_CDTVRAM_toggled(bool ischecked)
{
    workprefs.cs_cdtvram = ischecked;
}

void puae_MainWindow::on_IDC_CS_IDE1_toggled(bool ischecked)
{
    workprefs.cs_ide = (ui->IDC_CS_IDE1->checkState()) ? 1 : ((ui->IDC_CS_IDE2->checkState()) ? 2 : 0);
}

void puae_MainWindow::on_IDC_CS_IDE2_toggled(bool ischecked)
{
    workprefs.cs_ide = (ui->IDC_CS_IDE1->checkState()) ? 1 : ((ui->IDC_CS_IDE2->checkState()) ? 2 : 0);
}

/* ROM Mirror E0 */
void puae_MainWindow::on_IDC_CS_KSMIRROR_E0_toggled(bool ischecked)
{
    workprefs.cs_ksmirror_e0 = ischecked;
}

/* ROM Mirror A8 */
void puae_MainWindow::on_IDC_CS_KSMIRROR_A8_toggled(bool ischecked)
{
    workprefs.cs_ksmirror_a8 = ischecked;
}

/* C00000 is Fast RAM */
void puae_MainWindow::on_IDC_CS_SLOWISFAST_toggled(bool ischecked)
{
    workprefs.cs_slowmemisfast = ischecked;
}

/* A1000 Boot ROM/RAM */
void puae_MainWindow::on_IDC_CS_A1000RAM_toggled(bool ischecked)
{
    workprefs.cs_a1000ram = ischecked;
}

/* DF0: ID Hardware */
void puae_MainWindow::on_IDC_CS_DF0IDHW_toggled(bool ischecked)
{
    workprefs.cs_df0idhw = ischecked;
}

/* CDTV SRAM Expansion */
void puae_MainWindow::on_IDC_CS_CDTVRAMEXP_toggled(bool ischecked)
{
    workprefs.cs_cdtvram = ischecked;
}

/* PCMCIA */
void puae_MainWindow::on_IDC_CS_PCMCIA_toggled(bool ischecked)
{
    workprefs.cs_pcmcia = ischecked;
}

/* KB Reset Warning */
void puae_MainWindow::on_IDC_CS_RESETWARNING_toggled(bool ischecked)
{
    workprefs.cs_resetwarning = ischecked;
}

/* No-EHB DENISE */
void puae_MainWindow::on_IDC_CS_NOEHB_toggled(bool ischecked)
{
    workprefs.cs_denisenoehb = ischecked;
}

/* A1000 Agnus */
void puae_MainWindow::on_IDC_CS_DIPAGNUS_toggled(bool ischecked)
{
    workprefs.cs_dipagnus = ischecked;
}

/* A590/A2091 SCSI */
void puae_MainWindow::on_IDC_CS_A2091_toggled(bool ischecked)
{
    workprefs.cs_a2091 = ischecked;
}

/* A3000 SCSI */
void puae_MainWindow::on_IDC_CS_DMAC_toggled(bool ischecked)
{
    workprefs.cs_mbdmac = (ui->IDC_CS_DMAC->checkState()) ? 1 : ((ui->IDC_CS_DMAC2->checkState()) ? 2 : 0);
}

/* A4000T SCSI */
void puae_MainWindow::on_IDC_CS_DMAC2_toggled(bool ischecked)
{
    workprefs.cs_mbdmac = (ui->IDC_CS_DMAC->checkState()) ? 1 : ((ui->IDC_CS_DMAC2->checkState()) ? 2 : 0);
}

/* A4901 SCSI */
void puae_MainWindow::on_IDC_CS_A4091_toggled(bool ischecked)
{
    workprefs.cs_a4091 = ischecked;
}

/* CDTV SCSI */
void puae_MainWindow::on_IDC_CS_CDTVSCSI_toggled(bool ischecked)
{
    workprefs.cs_cdtvscsi = ischecked;
}

/* Ramsey Revision */
void puae_MainWindow::on_IDC_CS_RAMSEY_toggled(bool ischecked)
{
    workprefs.cs_ramseyrev = ischecked ? 0x0f : -1;
    //ui->IDC_CS_RAMSEYREV->setText(workprefs.cs_ramseyrev);
}

/* Fat Gary Revision */
void puae_MainWindow::on_IDC_CS_FATGARY_toggled(bool ischecked)
{
    workprefs.cs_fatgaryrev = ischecked ? 0x00 : -1;
}

/* Agnus/Alice Revision */
void puae_MainWindow::on_IDC_CS_AGNUS_toggled(bool checked)
{

}

/* Denise/Lisa Revision */
void puae_MainWindow::on_IDC_CS_DENISE_toggled(bool checked)
{

}

//*************************************************
// RAM
//*************************************************

/* Chip RAM */
void puae_MainWindow::on_IDC_CHIPMEM_valueChanged(int value)
{
    workprefs.chipmem_size = memsizes[msi_chip[value]];
    ui->IDC_CHIPRAM->setText(memsize_names[msi_chip[value]]);
}

/* Fast RAM */
void puae_MainWindow::on_IDC_FASTMEM_valueChanged(int value)
{
    workprefs.fastmem_size = memsizes[msi_fast[value]];
    ui->IDC_FASTRAM->setText(memsize_names[msi_fast[value]]);
}

/* Slow RAM */
void puae_MainWindow::on_IDC_SLOWMEM_valueChanged(int value)
{
    workprefs.bogomem_size = memsizes[msi_bogo[value]];
    ui->IDC_SLOWRAM->setText(memsize_names[msi_bogo[value]]);
}

/* Z3 Fast RAM */
void puae_MainWindow::on_IDC_Z3FASTMEM_valueChanged(int value)
{
    workprefs.z3chipmem_size = memsizes[msi_z3fast[value]];
    ui->IDC_Z3FASTRAM->setText(memsize_names[msi_z3fast[value]]);
}

/* Motherboard Fast RAM */
void puae_MainWindow::on_IDC_MBMEM1_valueChanged(int value)
{
    workprefs.mbresmem_low_size = memsizes[msi_gfx[value]];
    ui->IDC_MBRAM1->setText(memsize_names[msi_gfx[value]]);
}

/* Processor Slot RAM */
void puae_MainWindow::on_IDC_MBMEM2_valueChanged(int value)
{
    workprefs.mbresmem_high_size = memsizes[msi_gfx[value]];
    ui->IDC_MBRAM2->setText(memsize_names[msi_gfx[value]]);
}

void puae_MainWindow::on_IDC_FLOPPYSPD_valueChanged(int value)
{
    workprefs.floppy_speed = value;
    if (workprefs.floppy_speed > 0) {
            workprefs.floppy_speed--;
            workprefs.floppy_speed = 1 << workprefs.floppy_speed;
            workprefs.floppy_speed *= 100;
    }

    out_floppyspeed();
}


//
// Santa's Little Helpers
//
void puae_MainWindow::fix_values_memorydlg()
{
    if (workprefs.chipmem_size > 0x200000)
            workprefs.fastmem_size = 0;
}

void puae_MainWindow::updatez3 (unsigned int *size1p, unsigned int *size2p)
{
        int i;
        unsigned int s1, s2;

        // no 2GB Z3 size so we need 2x1G
        if (*size1p >= 0x80000000) {
                *size2p = *size1p - 0x40000000;
                *size1p = 0x40000000;
                return;
        }
        s1 = *size1p;
        *size1p = 0;
        *size2p = 0;
        s2 = 0;
        for (i = 32; i >= 0; i--) {
                if (s1 & (1 << i))
                        break;
        }
        if (i < 20)
                return;
        if (s1 == (1 << i)) {
                *size1p = s1;
                return;
        }
        s2 = s1 & ((1 << i) - 1);
        s1 = 1 << i;
        i--;
        while (i >= 0) {
                if (s2 & (1 << i)) {
                        s2 = 1 << i;
                        break;
                }
                i--;
        }
        if (i < 19)
                s2 = 0;
        *size1p = s1;
        *size2p = s2;
}

void puae_MainWindow::out_floppyspeed()
{
    char spe[30];

    if (workprefs.floppy_speed)
            sprintf (spe, "%d%%%s", workprefs.floppy_speed, workprefs.floppy_speed == 100 ? " (compatible)" : "");
    else
            strcpy (spe, "Turbo");
    ui->IDC_FLOPPYSPDTEXT->setText(spe);
}

void puae_MainWindow::values_to_memorydlg()
{
    unsigned int mem_size = 0;
    unsigned long v;

    switch (workprefs.chipmem_size) {
    case 0x00040000: mem_size = 0; break;
    case 0x00080000: mem_size = 1; break;
    case 0x00100000: mem_size = 2; break;
    case 0x00180000: mem_size = 3; break;
    case 0x00200000: mem_size = 4; break;
    case 0x00400000: mem_size = 5; break;
    case 0x00800000: mem_size = 6; break;
    }
    ui->IDC_CHIPMEM->setValue(mem_size);

    mem_size = 0;
    switch (workprefs.fastmem_size) {
    case 0x00000000: mem_size = 0; break;
    case 0x00100000: mem_size = 1; break;
    case 0x00200000: mem_size = 2; break;
    case 0x00400000: mem_size = 3; break;
    case 0x00800000: mem_size = 4; break;
    case 0x01000000: mem_size = 5; break;
    }
    ui->IDC_FASTMEM->setValue(mem_size);

    mem_size = 0;
    switch (workprefs.bogomem_size) {
    case 0x00000000: mem_size = 0; break;
    case 0x00080000: mem_size = 1; break;
    case 0x00100000: mem_size = 2; break;
    case 0x00180000: mem_size = 3; break;
    case 0x001C0000: mem_size = 4; break;
    }
    ui->IDC_SLOWMEM->setValue(mem_size);

    mem_size = 0;
    v = workprefs.z3fastmem_size + workprefs.z3fastmem2_size;
    if      (v < 0x00100000)
            mem_size = 0;
    else if (v < 0x00200000)
            mem_size = 1;
    else if (v < 0x00400000)
            mem_size = 2;
    else if (v < 0x00800000)
            mem_size = 3;
    else if (v < 0x01000000)
            mem_size = 4;
    else if (v < 0x02000000)
            mem_size = 5;
    else if (v < 0x04000000)
            mem_size = 6;
    else if (v < 0x08000000)
            mem_size = 7;
    else if (v < 0x10000000)
            mem_size = 8;
    else if (v < 0x18000000)
            mem_size = 9;
    else if (v < 0x20000000)
            mem_size = 10;
    else if (v < 0x30000000)
            mem_size = 11;
    else if (v < 0x40000000) // 1GB
            mem_size = 12;
    else if (v < 0x60000000) // 1.5GB
            mem_size = 13;
    else if (v < 0x80000000) // 2GB
            mem_size = 14;
    else if (v < 0xA8000000) // 2.5GB
            mem_size = 15;
    else if (v < 0xC0000000) // 3GB
            mem_size = 16;
    else
            mem_size = 17;
    ui->IDC_Z3FASTMEM->setValue(mem_size);

    mem_size = 0;
    switch (workprefs.gfxmem_size) {
    case 0x00000000: mem_size = 0; break;
    case 0x00100000: mem_size = 1; break;
    case 0x00200000: mem_size = 2; break;
    case 0x00400000: mem_size = 3; break;
    case 0x00800000: mem_size = 4; break;
    case 0x01000000: mem_size = 5; break;
    case 0x02000000: mem_size = 6; break;
    case 0x04000000: mem_size = 7; break;
    case 0x08000000: mem_size = 8; break;
    case 0x10000000: mem_size = 9; break;
    case 0x20000000: mem_size = 10; break;
    case 0x40000000: mem_size = 11; break;
    }
    //ui->IDC_P96MEM->setValue(mem_size);
    //ui->IDC_P96RAM->setValue(memsize_names[msi_gfx[mem_size]]);
}

void puae_MainWindow::enable_for_memorydlg ()
{
    int z3 = ! workprefs.address_space_24;
    int fast = workprefs.chipmem_size <= 0x200000;
    int rtg = workprefs.gfxmem_size; //&& full_property_sheet;
    int rtg2 = workprefs.gfxmem_size;

#ifndef AUTOCONFIG
    z3 = FALSE;
    fast = FALSE;
#endif

    ui->IDC_Z3FASTRAM->setEnabled(z3);
    ui->IDC_Z3FASTMEM->setEnabled(z3);
    ui->IDC_FASTMEM->setEnabled(fast);
    ui->IDC_FASTRAM->setEnabled(fast);
//    ui->IDC_FASTTEXT->setEnabled(fast);
//    ui->IDC_GFXCARDTEXT->setEnabled(z3);
    ui->IDC_P96RAM->setEnabled(z3);
    ui->IDC_P96MEM->setEnabled(z3);
    ui->IDC_MBRAM1->setEnabled(z3);
    ui->IDC_MBMEM1->setEnabled(z3);
    ui->IDC_MBRAM2->setEnabled(z3);
    ui->IDC_MBMEM2->setEnabled(z3);

    ui->IDC_RTG_8BIT->setEnabled(rtg);
    ui->IDC_RTG_16BIT->setEnabled(rtg);
    ui->IDC_RTG_24BIT->setEnabled(rtg);
    ui->IDC_RTG_32BIT->setEnabled(rtg);
    ui->IDC_RTG_MATCH_DEPTH->setEnabled(rtg2);
    ui->IDC_RTG_SCALE->setEnabled(rtg2);
    ui->IDC_RTG_SCALE_ALLOW->setEnabled(rtg2);
    ui->IDC_RTG_SCALE_ASPECTRATIO->setEnabled(rtg2);
    ui->IDC_RTG_VBLANKRATE->setEnabled(rtg2);
}
