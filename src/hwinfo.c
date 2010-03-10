/* * This file is part of testrunner-lite *
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Contact: Sampo Saaristo <ext-sampo.1.saaristo@nokia.com>
 *
 * This software, including documentation, is protected by copyright
 * controlled by Nokia Corporation. All rights are reserved. Copying,
 * including reproducing, storing, adapting or translating, any or all of
 * this material requires the prior written consent of Nokia Corporation.
 * This material also contains confidential information which may not be
 * disclosed to others without the prior written consent of Nokia.
 *
 */

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES */
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <libxml/xmlwriter.h>
#include "executor.h"
#include "hwinfo.h"

/* ------------------------------------------------------------------------- */
/* EXTERNAL DATA STRUCTURES */
/* None */

/* ------------------------------------------------------------------------- */
/* EXTERNAL GLOBAL VARIABLES */
/* None */

/* ------------------------------------------------------------------------- */
/* EXTERNAL FUNCTION PROTOTYPES */
/* None */

/* ------------------------------------------------------------------------- */
/* GLOBAL VARIABLES */
/* None */

/* ------------------------------------------------------------------------- */
/* CONSTANTS */
/* None */

/* ------------------------------------------------------------------------- */
/* MACROS */
/* None */

/* ------------------------------------------------------------------------- */
/* LOCAL GLOBAL VARIABLES */
/* None */
/* ------------------------------------------------------------------------- */
/* LOCAL CONSTANTS AND MACROS */
/* None */

/* ------------------------------------------------------------------------- */
/* MODULE DATA STRUCTURES */
/* None */
/* ------------------------------------------------------------------------- */
/* LOCAL FUNCTION PROTOTYPES */
/* ------------------------------------------------------------------------- */
LOCAL char *get_sysinfo (const char *);
/* ------------------------------------------------------------------------- */
/* FORWARD DECLARATIONS */
/* None */

/* ------------------------------------------------------------------------- */
/* ==================== LOCAL FUNCTIONS ==================================== */
/* ------------------------------------------------------------------------- */
/** Execute sysinfo-tool --get for given key
 *  @param key to be passed to sysinfo-tool e.g. "component/product"
 *  @return stdout output of the sysinfo-tool command
 */
LOCAL char *get_sysinfo (const char *key)
{
	char *cmd;
	exec_data edata;
	memset (&edata, 0x0, sizeof (exec_data));

	edata.soft_timeout = 5;
	edata.hard_timeout = edata.soft_timeout + 5;

	cmd = (char *)malloc (strlen ("sysinfo-tool --get ") + 
			      strlen (key) + 1);
	sprintf (cmd, "sysinfo-tool --get %s", key);
	execute (cmd, &edata);
	
	if (edata.stderr_data.buffer) {
		fprintf (stderr, "%s:%s():%s\n", PROGNAME, __FUNCTION__,
			 edata.stderr_data.buffer);
		free (edata.stderr_data.buffer);
	}

	return (char *)edata.stdout_data.buffer;
}

/* ------------------------------------------------------------------------- */
/* ======================== FUNCTIONS ====================================== */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/** Obtain hardware info and save it to argument
 * @param hi container for hardware info
 * @return 0 if basic information can be obtained 1 otherwise
 */
int read_hwinfo (hw_info *hi)
{
	memset (hi, 0x0, sizeof (hw_info));
	
	hi->product = get_sysinfo("component/product");
	hi->hw_build = get_sysinfo("component/product");
	if (!hi->product || !hi->hw_build) {
		fprintf (stderr, "%s: Failed to read basic HW "
			 "information from sysinfo.\n", PROGNAME);
		return 1;
	} 
	hi->nolo = get_sysinfo("component/nolo");
	hi->boot_mode = get_sysinfo("component/boot-mode");
	hi->production_sn = get_sysinfo("device/production-sn");
	hi->product_code = get_sysinfo("device/product-code");
	hi->basic_product_code = get_sysinfo("device/basic-product-code");

	
	return 0;
}
/* ------------------------------------------------------------------------- */
/** Print hardware info
 * @param hi hw_info data
 */
void print_hwinfo (hw_info *hi)
{
	printf ("Hardware Info:\n");
	printf ("%s %s %s %s\n", 
		(char *)(hi->product ? hi->product : "<none>"), 
		(char *)(hi->hw_build ? hi->hw_build : "<none>"), 
		(char *)(hi->nolo ? hi->nolo : "<none>"), 
		(char *)(hi->boot_mode ? hi->boot_mode : "<none>"));
	printf ("%s %s %s\n", 
		(char *)(hi->production_sn ? hi->production_sn : "<none>") , 
		(char *)(hi->product_code ? hi->product_code : "<none>"),
		(char *)(hi->basic_product_code ? hi->basic_product_code : 
			 "<none>"));
}
/* ------------------------------------------------------------------------- */
/** Free the allocated data from hw_info
 * @param hi hw_info data
 */
void clean_hwinfo (hw_info *hi)
{
	
        if (hi->product) free (hi->product); 
	if (hi->hw_build) free (hi->hw_build);
	if (hi->nolo) free (hi->nolo);
	if (hi->boot_mode) free (hi->boot_mode);
	if (hi->production_sn) free (hi->production_sn);
	if (hi->product_code) free (hi->product_code);
	if (hi->basic_product_code) free (hi->basic_product_code);
	return;
} 

/* ================= OTHER EXPORTED FUNCTIONS ============================== */
/* None */

/* ------------------------------------------------------------------------- */
/* End of file */