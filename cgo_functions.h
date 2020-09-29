#ifndef R53DB_CGO_FUNCTIONS_H
#define R53DB_CGO_FUNCTIONS_H

#include <postgres.h>

#include "dns.h"

/*
 * Pass C structs as char* to keep struct details away from Go --
 * it contains Postgres-specific types that we don't want to include
 * in Go.
 * Apparently, idiomatic usage of void* doesn't play well with Go, so
 * we cheat by using char* instead.
 */
void r53dbStoreResult(char *scanState_void, r53dbDNSRR *rr);
char *r53dbStoreZone(char *zoneList_void, r53dbZone *zone);

void r53dbDebug(const char *s);
void r53dbNotice(const char *s);
void r53dbError(const char *s);
void r53dbErrorWithDetail(const char *error, const char *detail, const char *hint);

r53dbZone *r53dbNewZone();

/*
 * Using a Go C literal, like C.r53dbDNSRR{}, might seem more obvious --
 * but by having an explicit allocator function, we can use palloc() to
 * get our memory, which will automatically be freed when the query context
 * ends.
 * Also I couldn't immediately figure out if Go C literals are guaranteed to
 * not get garbage-collected.
 */
r53dbDNSRR *r53dbNewDNSRR();

#endif // R53DB_CGO_FUNCTIONS_H

