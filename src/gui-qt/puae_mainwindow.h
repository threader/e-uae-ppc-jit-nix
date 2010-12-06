#ifndef PUAE_MAINWINDOW_H
#define PUAE_MAINWINDOW_H

#include <stdio.h>
#include <QMainWindow>

namespace Ui {
    class puae_MainWindow;
}

class puae_MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    QString GetPath(QWidget *who, QString what, QString where);
    QString GetFile(QWidget *who, QString what, QString extensions);
    explicit puae_MainWindow(QWidget *parent = 0);
    ~puae_MainWindow();

//private:
    Ui::puae_MainWindow *ui;

private slots:
    void on_IDC_MBMEM2_valueChanged(int value);
    void on_IDC_FLOPPYSPD_valueChanged(int value);
    void on_IDC_MBMEM1_valueChanged(int value);
    void on_IDC_Z3FASTMEM_valueChanged(int value);
    void on_IDC_SLOWMEM_valueChanged(int value);
    void on_IDC_FASTMEM_valueChanged(int value);
    void on_IDC_CHIPMEM_valueChanged(int value);
    void on_IDC_CS_DENISE_toggled(bool checked);
    void on_IDC_CS_AGNUS_toggled(bool checked);
    void on_IDC_CS_FATGARY_toggled(bool checked);
    void on_IDC_CS_RAMSEY_toggled(bool checked);
    void on_IDC_CS_CDTVSCSI_toggled(bool checked);
    void on_IDC_CS_DMAC2_toggled(bool checked);
    void on_IDC_CS_A4091_toggled(bool checked);
    void on_IDC_CS_DMAC_toggled(bool checked);
    void on_IDC_CS_A2091_toggled(bool checked);
    void on_IDC_CS_DIPAGNUS_toggled(bool checked);
    void on_IDC_CS_NOEHB_toggled(bool checked);
    void on_IDC_CS_RESETWARNING_toggled(bool checked);
    void on_IDC_CS_PCMCIA_toggled(bool checked);
    void on_IDC_CS_CDTVRAMEXP_toggled(bool checked);
    void on_IDC_CS_DF0IDHW_toggled(bool checked);
    void on_IDC_CS_A1000RAM_toggled(bool checked);
    void on_IDC_CS_SLOWISFAST_toggled(bool checked);
    void on_IDC_CS_KSMIRROR_A8_toggled(bool checked);
    void on_IDC_CS_KSMIRROR_E0_toggled(bool checked);
    void on_IDC_CS_CIAA_TOD3_clicked();
    void on_IDC_CS_CIAA_TOD2_clicked();
    void on_IDC_CS_RTC3_clicked();
    void on_IDC_CS_RTC2_clicked();
    void on_IDC_CS_SOUND2_clicked();
    void on_IDC_CS_SOUND1_clicked();
    void on_IDC_COLLISION3_clicked();
    void on_IDC_COLLISION2_clicked();
    void on_IDC_COLLISION1_clicked();
    void on_IDC_FPU2_clicked();
    void on_IDC_FPU3_clicked();
    void on_IDC_FPU1_clicked();
    void on_IDC_CPU5_clicked();
    void on_IDC_CPU3_clicked();
    void on_IDC_CPU4_clicked();
    void on_IDC_CPU2_clicked();
    void on_IDC_CS_CDTVRAM_toggled(bool checked);
    void on_IDC_CS_CD32NVRAM_toggled(bool checked);
    void on_IDC_CS_CD32C2P_toggled(bool checked);
    void on_IDC_CS_IDE2_toggled(bool checked);
    void on_IDC_CS_IDE1_toggled(bool checked);
    void on_IDC_CS_CDTVCD_toggled(bool checked);
    void on_IDC_CS_CD32CD_toggled(bool checked);
    void on_IDC_CS_CIAOVERLAY_toggled(bool checked);
    void on_IDC_CS_CIAA_TOD1_clicked();
    void on_IDC_CS_RTC1_clicked();
    void on_IDC_CS_COMPATIBLE_toggled(bool checked);
    void on_IDC_CS_SOUND0_clicked();
    void on_IDC_COLLISION0_clicked();
    void on_IDC_NTSC_toggled(bool checked);
    void on_IDC_GENLOCK_toggled(bool checked);
    void on_IDC_CYCLEEXACT_toggled(bool checked);
    void on_IDC_BLITIMM_toggled(bool checked);
    void on_IDC_CS_EXT_currentIndexChanged(int index);
    void on_IDC_AGA_clicked();
    void on_IDC_ECS_clicked();
    void on_IDC_ECS_DENISE_clicked();
    void on_IDC_ECS_AGNUS_clicked();
    void on_IDC_OCS_clicked();
    void on_IDC_CPU_FREQUENCY_currentIndexChanged(int index);
    void on_IDC_CPUIDLE_sliderMoved(int position);
    void on_IDC_SPEED_valueChanged(int value);
    void on_IDC_CS_ADJUSTABLE_clicked();
    void on_IDC_CS_68000_clicked();
    void on_IDC_CS_HOST_clicked();
    void on_IDC_COMPATIBLE_FPU_toggled(bool checked);
    void on_IDC_FPU0_clicked();
    void on_IDC_MMUENABLE_toggled(bool checked);
    void on_IDC_JITENABLE_toggled(bool checked);
    void on_IDC_COMPATIBLE_toggled(bool checked);
    void on_IDC_COMPATIBLE24_toggled(bool checked);
    void on_IDC_CPU1_clicked();
    void on_IDC_CPU0_clicked();
    void on_IDC_KICKSHIFTER_toggled(bool checked);
    void on_IDC_MAPROM_toggled(bool checked);
    void on_IDC_FLASHCHOOSER_clicked();
    void on_IDC_CARTCHOOSER_clicked();
    void on_IDC_ROMCHOOSER2_clicked();
    void on_IDC_PATHS_RIPS_clicked();
    void on_IDC_PATHS_SAVEIMAGES_clicked();
    void on_IDC_PATHS_AVIOUTPUTS_clicked();
    void on_IDC_PATHS_SAVESTATES_clicked();
    void on_IDC_KICKCHOOSER_clicked();
    void on_IDC_PATHS_SCREENSHOTS_clicked();
    void on_IDC_PATHS_CONFIGS_clicked();
    void on_IDC_PATHS_ROMS_clicked();
//macros
    void values_to_memorydlg();
    void out_floppyspeed();
    void fix_values_memorydlg();
    void updatez3 (unsigned int *size1p, unsigned int *size2p);
    void enable_for_memorydlg ();
};

#define TCHAR char

typedef unsigned char  uae_u8;
typedef signed char    uae_s8;

typedef unsigned int   uae_u16;
typedef int            uae_s16;

typedef unsigned long  uae_u32;
typedef long           uae_s32;

#define MAX_DPATH	512
#define STATIC_INLINE static __inline__

#endif // PUAE_MAINWINDOW_H
