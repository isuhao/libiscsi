/* 
   Copyright (C) 2014 by Ronnie Sahlberg <sahlberg@gmail.com>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_read10_dpofua(void)
{ 
	int ret, dpofua, usage_data_dpofua;
	struct scsi_task *ms_task = NULL;
	struct scsi_mode_sense *ms;
	struct scsi_task *rso_task = NULL;
	struct scsi_report_supported_op_codes_one_command *rsoc;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test READ10 DPO/FUA flags");

	CHECK_FOR_SBC;

	logging(LOG_VERBOSE, "Read the DPOFUA flag from mode sense data");
	ret = modesense6(sd, &ms_task, 0, SCSI_MODESENSE_PC_CURRENT,
			 SCSI_MODEPAGE_RETURN_ALL_PAGES, 0, 255,
			 EXPECT_STATUS_GOOD);
	CU_ASSERT_EQUAL(ret, 0);
	logging(LOG_VERBOSE, "[SUCCESS] Mode sense returned status GOOD");
	ms = scsi_datain_unmarshall(ms_task);
	dpofua = ms && (ms->device_specific_parameter & 0x10);
	scsi_free_scsi_task(ms_task);

	if (dpofua) {
		logging(LOG_VERBOSE, "DPOFUA flag is set. Device should allow "
			"DPO/FUA flags in CDBs");
	} else {
		logging(LOG_VERBOSE, "DPOFUA flag is clear. Device should fail "
			"CDBs with DPO/FUA set");
	}

	logging(LOG_VERBOSE, "Test READ10 with DPO==1");
	if (dpofua) {
		ret = read10(sd, NULL, 0,
			     block_size, block_size, 0, 1, 0, 0, 0, NULL,
			     EXPECT_STATUS_GOOD);
		CU_ASSERT_EQUAL(ret, 0);
	} else {
		ret = read10(sd, NULL, 0,
			     block_size, block_size, 0, 1, 0, 0, 0, NULL,
			     EXPECT_INVALID_FIELD_IN_CDB);
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test READ10 with FUA==1");
	if (dpofua) {
		ret = read10(sd, NULL, 0,
			     block_size, block_size, 0, 0, 1, 0, 0, NULL,
			     EXPECT_STATUS_GOOD);
		CU_ASSERT_EQUAL(ret, 0);
	} else {
		ret = read10(sd, NULL, 0,
			     block_size, block_size, 0, 0, 1, 0, 0, NULL,
			     EXPECT_INVALID_FIELD_IN_CDB);
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test READ10 with DPO==1 FUA==1");
	if (dpofua) {
		ret = read10(sd, NULL, 0,
			     block_size, block_size, 0, 1, 1, 0, 0, NULL,
			     EXPECT_STATUS_GOOD);
		CU_ASSERT_EQUAL(ret, 0);
	} else {
		ret = read10(sd, NULL, 0,
			     block_size, block_size, 0, 1, 1, 0, 0, NULL,
			     EXPECT_INVALID_FIELD_IN_CDB);
		CU_ASSERT_EQUAL(ret, 0);
	}


	logging(LOG_VERBOSE, "Try fetching REPORT_SUPPORTED_OPCODES "
		"for READ10");
	ret = report_supported_opcodes(sd, &rso_task,
				       0, SCSI_REPORT_SUPPORTING_OPCODE,
				       SCSI_OPCODE_READ10,
				       0,
				       65535,
				       EXPECT_STATUS_GOOD);
	if (ret == -2) {
		logging(LOG_NORMAL, "REPORT_SUPPORTED_OPCODES not implemented. "
			"Skipping this part of the test");
		return;
	}
	logging(LOG_VERBOSE, "Unmarshall the DATA-IN buffer");
	rsoc = scsi_datain_unmarshall(rso_task);
	if (rsoc) {
		usage_data_dpofua = rsoc->cdb_usage_data[1] & 0x18;
		if (dpofua) {
			logging(LOG_VERBOSE, "DPOFUA is set. Verify the "
				"DPO/FUA flags are set in the CDB_USAGE_DATA");
			if (!usage_data_dpofua) {
				logging(LOG_NORMAL, "[FAILED] DpoFua not set "
					"in CDB_USAGE_DATE");
				CU_FAIL("DpoFua not set in CDB_USAGE_DATE");
			}
		} else {
			logging(LOG_VERBOSE, "DPOFUA is clear. Verify the "
				"DPO/FUA flags are clear in the CDB_USAGE_DATA");
			if (usage_data_dpofua) {
				logging(LOG_NORMAL, "[FAILED] DpoFua not clear "
					"in CDB_USAGE_DATE");
				CU_FAIL("DpoFua not clear in CDB_USAGE_DATE");
			}
		}
	} else {
		logging(LOG_NORMAL, "[FAILED] Target did not return any data "
			"for ReportSupportedOpcodes\n");
		CU_FAIL("Target did not return any data for "
			"ReportSupportedOpcodes");
	}
	scsi_free_scsi_task(rso_task);
}
