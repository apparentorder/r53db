#ifndef R53DB_MISC_H
#define R53DB_MISC_H

#include <postgres.h>
#include <access/tupdesc.h>

char *get_relation_hosted_zone_id(Oid relation_id);
List *get_column_positions(TupleDesc td);
r53dbDNSRR *get_rr_from_values(Datum *values, bool *isnulls, List *column_positions);

void palloc_string(char **s);
void make_dns_identifier(char *s);

Datum CStringGetTextDatumOrNULL(char *s);

#endif // R53DB_MISC_H

