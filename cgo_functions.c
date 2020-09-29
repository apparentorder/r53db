#include <stdio.h>

#include <postgres.h>
#include <fmgr.h>

#include <foreign/foreign.h>

#include "dns.h"
#include "misc.h"
#include "fdw.h"

void r53dbDebug(const char *s) {
	elog(DEBUG1, "%s", s);
}

void r53dbError(const char *s) {
	elog(ERROR, "%s", s);
}

void r53dbNotice(const char *s) {
	elog(NOTICE, "%s", s);
}

r53dbZone *r53dbNewZone() {
	return (r53dbZone *) palloc0(sizeof(r53dbZone));
}


r53dbDNSRR *r53dbNewDNSRR() {
	return palloc0(sizeof(r53dbDNSRR));
}

char *r53dbStoreZone(char *zoneList_void, r53dbZone *zone) {
	elog(DEBUG2, "r53dbStoreZone() zoneList@%p zone@%p", zoneList_void, zone);

	palloc_string(&zone->id);
	palloc_string(&zone->name);

	zone->table_name = pstrdup(zone->name);
	make_dns_identifier(zone->table_name);

	return (char *) lappend((List *) zoneList_void, zone);
}

void r53dbStoreResult(char *scanState_void, r53dbDNSRR *rr) {
	elog(DEBUG2, "r53dbStoreResult() scanState@%p rr@%p", scanState_void, rr);
	elog(
		DEBUG2,
		"... for name=%s type=%s data=%s",
		rr->name,
		rr->type,
		rr->data != NULL ? rr->data : "(NULL)"
	);

	r53dbScanState *scanState = (r53dbScanState *) scanState_void;

	palloc_string(&rr->name);
	palloc_string(&rr->type);
	palloc_string(&rr->data);
	palloc_string(&rr->at_dns_name);
	palloc_string(&rr->at_hosted_zone_id);

	scanState->results = lappend(scanState->results, rr);
}

