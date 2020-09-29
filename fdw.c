#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#include <postgres.h>
#include <fmgr.h>
#include <access/htup_details.h>
#include <access/xact.h>
#include <catalog/pg_type.h>
#include <executor/executor.h>
#include <foreign/fdwapi.h>
#include <foreign/foreign.h>
#include <nodes/makefuncs.h>
#include <nodes/pg_list.h>
#include <optimizer/pathnode.h>
#include <optimizer/planmain.h>
#include <optimizer/restrictinfo.h>
#include <utils/builtins.h> // for TextDatumGetCString()
#include <utils/rel.h>
#include <utils/typcache.h>

#include "go_functions.h"
#include "dns.h"
#include "misc.h"
#include "fdw.h"

PG_MODULE_MAGIC;

/*----------------
 * for all following functions, refer to
 * https://www.postgresql.org/docs/12/fdw-callbacks.html
 *----------------
 */

void r53dbGetForeignRelSize(
	PlannerInfo *root,
	RelOptInfo *baserel,
	Oid foreigntableid
) {
	// OPTIONAL: set estimates: baserel->rows, ->width, ->tuples
	// This needs to exist, but doesn't really need to do anything for now.
	elog(DEBUG1, "r53db GetForeignRelSize(): NOP");
}

void r53dbGetForeignPaths(
	PlannerInfo *root,
	RelOptInfo *baserel,
	Oid foreigntableid
) {
	elog(DEBUG1, "r53db GetForeignPaths()");

	ForeignPath *fp = create_foreignscan_path(
		root,
		baserel,
		NULL, // baserel->reltarget, // target
		baserel->rows, // rows
		(Cost) 53, // startup_cost,
		(Cost) 53, // total_cost,
		NULL, // pathkeys
		baserel->lateral_relids, // required_outer,
		NULL, // fdw_outerpath,
		NULL // fdw_private
	);

	add_path(baserel, (Path *) fp);
}

ForeignScan *r53dbGetForeignPlan(
	PlannerInfo *root,
	RelOptInfo *baserel,
	Oid foreigntableid,
	ForeignPath *best_path,
	List *tlist,
	List *scan_clauses,
	Plan *outer_plan
) {
	elog(DEBUG1, "r53db GetForeignPlan()");

	return make_foreignscan(
		tlist, // qptlist
		extract_actual_clauses(scan_clauses, false), // qpqual
		baserel->relid, // scanrelid
		NULL, // fdw_exprs
		best_path->fdw_private, // fdw_private
		NULL, // fdw_scan_tlist
		NULL, // fdw_recheck_quals
		outer_plan // outer_plan
	);
}

void r53dbBeginForeignScan(ForeignScanState *node, int eflags) {
	elog(DEBUG1, "r53db BeginForeignScan()");

	r53dbScanState *scanState = (r53dbScanState *) palloc0(sizeof(r53dbScanState));
	scanState->column_positions = NIL;
	scanState->results = NIL;
	node->fdw_state = (void *) scanState;

	TupleTableSlot *tts = (TupleTableSlot *) node->ss.ss_ScanTupleSlot;

	char *hosted_zone_id = get_relation_hosted_zone_id(node->ss.ss_currentRelation->rd_id);
	char *relname = NameStr(node->ss.ss_currentRelation->rd_rel->relname);

	elog(DEBUG1, "BeginForeignScan of foreign table %s (hosted_zone_id %s)", relname, hosted_zone_id);

	scanState->column_positions = get_column_positions(tts->tts_tupleDescriptor);

	r53dbGoBeginScan((void *) scanState, hosted_zone_id);
}

TupleTableSlot *r53dbIterateForeignScan(ForeignScanState *node) {
	elog(DEBUG1, "r53db IterateForeignScan()");

	TupleTableSlot *tts = node->ss.ss_ScanTupleSlot;
	Datum *values = tts->tts_values;
	bool *isnull = tts->tts_isnull;

	// init all results to NULL
	// (precaution only, as every result should be set below)
	for (int i = 0; i < tts->tts_tupleDescriptor->natts; i++) {
		values[i] = PointerGetDatum(NULL);
		isnull[i] = true;
	}

	r53dbScanState *scanState = (r53dbScanState *) node->fdw_state;

	if (scanState->result_index == list_length(scanState->results)) {
		// done
		return NULL;
	}

	r53dbDNSRR *rr = (r53dbDNSRR *) list_nth(scanState->results, scanState->result_index);
	scanState->result_index++;

	elog(
		DEBUG2,
		"scanState@%p tts_values@%p tts_isnull=%p rname=%s rdata=%s",
		scanState, values, isnull,
		rr->name, rr->data != NULL ? rr->data : "(NULL)"
	);

	ExecClearTuple(tts);

	ListCell *lc;
	foreach(lc, scanState->column_positions) {
		r53dbColumnPosition *cp = (r53dbColumnPosition *) lfirst(lc);

		switch (cp->column) {
		case name:
			values[cp->position] = CStringGetTextDatum(rr->name);
			isnull[cp->position] = false;
			break;
		case type:
			values[cp->position] = CStringGetTextDatum(rr->type);
			isnull[cp->position] = false;
			break;
		case ttl:
			values[cp->position] = Int32GetDatum(rr->ttl);
			isnull[cp->position] = (rr->at_dns_name != NULL);
			break;
		case data:
			values[cp->position] = CStringGetTextDatumOrNULL(rr->data);
			isnull[cp->position] = (rr->at_dns_name != NULL);
			break;
		case at_dns_name:
			values[cp->position] = CStringGetTextDatumOrNULL(rr->at_dns_name);
			isnull[cp->position] = (rr->at_dns_name == NULL);
			break;
		case at_hosted_zone_id:
			values[cp->position] = CStringGetTextDatumOrNULL(rr->at_hosted_zone_id);
			isnull[cp->position] = (rr->at_dns_name == NULL);
			break;
		case at_evaluate_target_health:
			values[cp->position] = BoolGetDatum(rr->at_evaluate_target_health);
			isnull[cp->position] = (rr->at_dns_name == NULL);
			break;
		default:
			elog(ERROR, "Internal error: Invalid column %d in column list", cp->column);
		}
	}

	ExecStoreVirtualTuple(tts);

	return tts;
}

void r53dbReScanForeignScan(ForeignScanState *node) {
	elog(ERROR, "r53db: ReScanForeignScan not yet implemented");
}

void r53dbEndForeignScan(ForeignScanState *node) {
	elog(DEBUG1, "r53db EndForeignScan()");
	r53dbGoEndScan();
}

List *r53dbImportForeignSchema(ImportForeignSchemaStmt *stmt, Oid serverOid) {
	elog(DEBUG1, "r53db ImportForeignSchema()");

	ForeignServer *fs = GetForeignServer(serverOid);
	List *zones = (List *) r53dbGoGetZones(NULL);
	elog(DEBUG2, "... zoneList@%p", zones);
	elog(DEBUG2, "... serverName@%p", fs->servername);

	List *statements = NIL;
	ListCell *lc;
	foreach(lc, zones) {
		r53dbZone *zone = (r53dbZone *) lfirst(lc);
		elog(DEBUG2, "... zoneName@%p zoneId@%p", zone->name, zone->id);

		char *statement = psprintf(
			"CREATE FOREIGN TABLE IF NOT EXISTS %s ("
				"name text not null, "
				"type text not null, "
				"ttl int default 300, " // 300s is the default in the AWS Console
				"data text, "
				"at_dns_name text, "
				"at_hosted_zone_id text, "
				"at_evaluate_target_health bool"
			") "
			"SERVER %s "
			"OPTIONS (dns_name '%s', hosted_zone_id '%s')",
			zone->table_name,
			fs->servername,
			zone->name,
			zone->id
		);
		statements = lappend(statements, statement);
		elog(DEBUG2, "%s", statement);
	}

	return statements;
}

void r53dbAddForeignUpdateTargets(Query *parsetree, RangeTblEntry *target_rte, Relation target_relation) {
	elog(DEBUG1, "r53db AddForeignUpdateTargets()");

	Var *var = makeWholeRowVar(target_rte, parsetree->resultRelation, 0, false);
	TargetEntry *tle;

	tle = makeTargetEntry((Expr *) var, list_length(parsetree->targetList) + 1, "r53wholerow", true);

	parsetree->targetList = lappend(parsetree->targetList, tle);
}

void r53dbBeginForeignModify(
	ModifyTableState *mtstate,
	ResultRelInfo *rinfo,
	List *fdw_private,
	int subplan_index,
	int eflags
) {
	if (IsTransactionBlock()) {
		char *table = NameStr(rinfo->ri_RelationDesc->rd_rel->relname);
	       	elog(ERROR, "Route53 data cannot be modified in a transaction (foreign table %s)", table);
		return;
	}

	// Pg expects a List node as fdw_private, so let's wrap our state struct
	// in a single-element List.
	r53dbModifyState *modifyState = palloc0(sizeof(r53dbModifyState));

	modifyState->operation = mtstate->operation;
	modifyState->hosted_zone_id = get_relation_hosted_zone_id(rinfo->ri_RelationDesc->rd_id);
	modifyState->column_positions = get_column_positions(rinfo->ri_RelationDesc->rd_att);

	if (mtstate->operation != CMD_INSERT) {
		// For UPDATE/DELETE, figure out at which position our junk row is
		modifyState->junk_row_resno = InvalidAttrNumber;
		ListCell *lc;
		foreach(lc, mtstate->mt_plans[subplan_index]->plan->targetlist) {
			TargetEntry *tle = lfirst(lc);

			if (tle->resjunk) {
				elog(DEBUG2, "... junk column %s at resno %d", tle->resname, tle->resno);
				modifyState->junk_row_resno = tle->resno;
			}
		}

		if (modifyState->junk_row_resno == InvalidAttrNumber) {
			elog(ERROR, "r53dbBeginForeignModify(): Cannot find resjunk target?!");
			return;
		}
	}

	rinfo->ri_FdwState = lappend(NIL, modifyState);
}

TupleTableSlot *r53dbExecForeignModify(
	EState *estate,
	ResultRelInfo *rinfo,
	TupleTableSlot *slot,
	TupleTableSlot *planSlot
) {
	elog(DEBUG1, "r53db ExecForeignModify()");

	Datum *values = NULL;
	bool *isnulls = NULL;
	TupleDesc tupDesc = NULL;

	r53dbDNSRR *oldRR = NULL;
	r53dbDNSRR *newRR = NULL;

	r53dbModifyState *modifyState = (r53dbModifyState *) linitial(rinfo->ri_FdwState);

	elog(DEBUG1, "... zone %s", modifyState->hosted_zone_id);

	/*
	 * INSERT and UPDATE will have a filled tuple with the new value ready-to-go in 'slot'.
	 *
	 * For UPDATE and DELETE, we need to identify the old row with the junk column added
	 * in r53dbAddForeignUpdateTargets() (via 'planSlot').
	 */
	if (modifyState->operation != CMD_DELETE) {
		// for INSERT, UPDATE: Get the new RR
		values = slot->tts_values;
		isnulls = slot->tts_isnull;
		tupDesc = slot->tts_tupleDescriptor;
		slot_getallattrs(slot);

		newRR = get_rr_from_values(values, isnulls, modifyState->column_positions);
	}

	if (modifyState->operation != CMD_INSERT) {
		// for UPDATE, DELETE: Get the old RR
		elog(DEBUG2, "r53db ExecForeignModify: Retrieve junk column");

		slot_getallattrs(planSlot);

		// Now, having our whole-row junk column, build a HeapTuple and point
		// values and isnulls there.
		// code stolen from execUtils.c, GetAttributeByName()
		// https://doxygen.postgresql.org/execUtils_8c.html#ac7de9629c5bfbf1c4734824f03a5e281
		HeapTupleHeader ht_header = DatumGetHeapTupleHeader(planSlot->tts_values[modifyState->junk_row_resno - 1]);

		Oid tupType = HeapTupleHeaderGetTypeId(ht_header);
		int32 tupTypmod = HeapTupleHeaderGetTypMod(ht_header);
		tupDesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

		HeapTupleData ht_data;
		ht_data.t_len = HeapTupleHeaderGetDatumLength(ht_header);
		ItemPointerSetInvalid(&ht_data.t_self);
		ht_data.t_tableOid = InvalidOid;
		ht_data.t_data = ht_header;

		values = palloc0(sizeof(Datum) * tupDesc->natts);
		isnulls = palloc0(sizeof(bool) * tupDesc->natts);
		heap_deform_tuple((HeapTuple) &ht_data, tupDesc, values, isnulls);

		oldRR = get_rr_from_values(values, isnulls, modifyState->column_positions);
		ReleaseTupleDesc(tupDesc);
	}

	bool is_success = false;
	switch (modifyState->operation) {
	case CMD_INSERT:
		is_success = r53dbGoModifyDNSRR(modifyState->hosted_zone_id, newRR, oldRR, DML_INSERT);
		break;
	case CMD_UPDATE:
		is_success = r53dbGoModifyDNSRR(modifyState->hosted_zone_id, newRR, oldRR, DML_UPDATE);
		break;
	case CMD_DELETE:
		is_success = r53dbGoModifyDNSRR(modifyState->hosted_zone_id, newRR, oldRR, DML_DELETE);
		if (is_success) {
			// fill the passed-in 'slot' with the tuple to be returned
			ExecClearTuple(slot);
			slot->tts_values = values;
			slot->tts_isnull = isnulls;
			ExecStoreVirtualTuple(slot);
		}
		break;
	default:
		elog(ERROR, "unexpected operation commandType %d", modifyState->operation);
		return NULL;
	}

	elog(DEBUG1, "r53db ExecForeignModify() *DONE*");
	return is_success ? slot : NULL;
}

PG_FUNCTION_INFO_V1(r53db_fdw_handler);
Datum r53db_fdw_handler(PG_FUNCTION_ARGS) {
	FdwRoutine *fdw = makeNode(FdwRoutine);

	elog(DEBUG1, "r53db says hi!");
	r53dbGoOnLoad();

	fdw->GetForeignRelSize = r53dbGetForeignRelSize;
	fdw->GetForeignPaths = r53dbGetForeignPaths;
	fdw->GetForeignPlan = r53dbGetForeignPlan;
	fdw->BeginForeignScan = r53dbBeginForeignScan;
	fdw->IterateForeignScan = r53dbIterateForeignScan;
	fdw->ReScanForeignScan = r53dbReScanForeignScan;
	fdw->EndForeignScan = r53dbEndForeignScan;
	fdw->ImportForeignSchema = r53dbImportForeignSchema;
	fdw->BeginForeignModify = r53dbBeginForeignModify;
	fdw->ExecForeignInsert = r53dbExecForeignModify;
	fdw->ExecForeignUpdate = r53dbExecForeignModify;
	fdw->ExecForeignDelete = r53dbExecForeignModify;
	fdw->AddForeignUpdateTargets = r53dbAddForeignUpdateTargets;

	PG_RETURN_POINTER(fdw);
}

