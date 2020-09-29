#include <stdio.h>

#include <postgres.h>
#include <fmgr.h>

#include <foreign/foreign.h>
#include <catalog/pg_type.h>
#include <utils/builtins.h>

#include "dns.h"
#include "misc.h"
#include "fdw.h"

char *get_relation_hosted_zone_id(Oid relation_id) {
	ForeignTable *ft = GetForeignTable(relation_id);

	ListCell *lcopt;
	foreach(lcopt, ft->options) {
		DefElem *opt = (DefElem *) lfirst(lcopt);
		Value *optval = (Value *) opt->arg;

		if (strcmp(opt->defname, "hosted_zone_id") == 0) {
			return optval->val.str;
		}
	}

	elog(ERROR, "Missing hosted_zone_id option in foreign table defintion");
}

List *get_column_positions(TupleDesc td) {
	List *res = NIL;

	for (int attnum = 0; attnum < td->natts; attnum++) {
		r53dbColumnPosition *cpos = (r53dbColumnPosition *) palloc0(sizeof(r53dbColumnPosition));
		int expected_atttypid = -1;

		Form_pg_attribute attr = TupleDescAttr(td, attnum);
		if (attr->attisdropped) continue;

		char *attname = NameStr(attr->attname);

		if (strcmp(attname, "name") == 0) {
			cpos->column = name;
			cpos->position = attnum;
			expected_atttypid = TEXTOID;
		} else if (strcmp(attname, "type") == 0) {
			cpos->column = type;
			cpos->position = attnum;
			expected_atttypid = TEXTOID;
		} else if (strcmp(attname, "ttl") == 0) {
			cpos->column = ttl;
			cpos->position = attnum;
			expected_atttypid = INT4OID;
		} else if (strcmp(attname, "data") == 0) {
			cpos->column = data;
			cpos->position = attnum;
			expected_atttypid = TEXTOID;
		} else if (strcmp(attname, "at_dns_name") == 0) {
			cpos->column = at_dns_name;
			cpos->position = attnum;
			expected_atttypid = TEXTOID;
		} else if (strcmp(attname, "at_hosted_zone_id") == 0) {
			cpos->column = at_hosted_zone_id;
			cpos->position = attnum;
			expected_atttypid = TEXTOID;
		} else if (strcmp(attname, "at_evaluate_target_health") == 0) {
			cpos->column = at_evaluate_target_health;
			cpos->position = attnum;
			expected_atttypid = BOOLOID;
		} else {
			elog(ERROR, "invalid column name %s in table definition", attname);
			return NIL;
		}

		if (attr->atttypid != expected_atttypid) {
			elog(FATAL, "invalid data type for column %s", attname);
			return NIL;
		}

		res = lappend(res, cpos);
	}

	return res;
}

r53dbDNSRR *get_rr_from_values(Datum *values, bool *isnulls, List *column_positions) {
	r53dbDNSRR *rr = palloc0(sizeof(r53dbDNSRR));

	ListCell *lc;
	foreach(lc, column_positions) {
		r53dbColumnPosition *cp = (r53dbColumnPosition *) lfirst(lc);
		if (isnulls[cp->position]) continue;

		Datum value = values[cp->position];

		switch (cp->column) {
		case name: rr->name = TextDatumGetCString(value); break;
		case type: rr->type = TextDatumGetCString(value); break;
		case ttl: rr->ttl = DatumGetInt32(value); break;
		case data: rr->data = TextDatumGetCString(value); break;
		case at_dns_name: rr->at_dns_name = TextDatumGetCString(value); break;
		case at_hosted_zone_id: rr->at_hosted_zone_id = TextDatumGetCString(value); break;
		case at_evaluate_target_health: rr->at_evaluate_target_health = DatumGetBool(value); break;
		default:
			elog(ERROR, "Internal error: Invalid column %d in column list", cp->column);
		}
	}

	return rr;
}

/*
 * The C Strings within our structs will have been allocated by Go's
 * C.CString(), which malloc()s memory in C land. That needs to
 * be freed at some point.
 *
 * Use palloc_string() to re-map all those malloc()ed C Strings to
 * palloc()ed C Strings and to free the malloc()s.
 *
 * It would be nice to find a less wasteful way to allocate with palloc()
 * in the first place.
 */
void palloc_string(char **s) {
	if (*s == NULL) return;

	char *new = (char *) palloc(strlen(*s) + 1);
	strcpy(new, *s);
	free(*s);
	*s = new;
}

void make_dns_identifier(char *s) {
	while (*s != '\0') {
		if (s[0] == '.' && s[1] == '\0') {
			// remove final '.'
			s[0] = '\0';
		} else if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9'))) {
			// make every non-[a-z0-9] into a _
			*s = '_';
		}

		s++;
	}
}

Datum CStringGetTextDatumOrNULL(char *s) {
	char *t = (s != NULL) ? s : "(NULL)";
	return CStringGetTextDatum(t);
}

PG_FUNCTION_INFO_V1(r53db_hi);
Datum r53db_hi(PG_FUNCTION_ARGS) {
	PG_RETURN_TEXT_P(cstring_to_text("ho"));
}

PG_FUNCTION_INFO_V1(r53db_42);
Datum r53db_42(PG_FUNCTION_ARGS) {
	PG_RETURN_INT32(42);
}

