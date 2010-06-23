#ifndef __gsm_backupics_h
#define __gsm_backupics_h

#include "backgen.h"

#ifdef GSM_ENABLE_BACKUP
GSM_Error SaveICS(char *FileName, GSM_Backup *backup);
GSM_Error LoadICS(char *FileName, GSM_Backup *backup);
#endif

#endif

/* How should editor hadle tabs in this file? Add editor commands here.
 * vim: noexpandtab sw=8 ts=8 sts=8:
 */