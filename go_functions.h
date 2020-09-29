#ifndef R53DB_GOFUNC_H
#define R53DB_GOFUNC_H

#include "dns.h"

extern void r53dbGoOnLoad();
extern bool r53dbGoModifyDNSRR(char *hosted_zone_id, r53dbDNSRR *new_rr, r53dbDNSRR *old_rr, int op);
extern char *r53dbGoGetZones(char *zoneList);
extern void r53dbGoBeginScan(char *scanState, const char *hosted_zone_id);
extern void r53dbGoEndScan();
extern r53dbDNSRR *r53dbGoIterateScan();

#endif // R53DB_GOFUNC_H
