#ifndef R53DB_FDW_H
#define R53DB_FDW_H

#include <postgres.h>
#include <nodes/pg_list.h>

enum r53dbDMLOp {
	DML_INSERT,
	DML_UPDATE,
	DML_DELETE
};

enum r53dbColumn {
	name,
	type,
	ttl,
	data,
	at_dns_name,
	at_hosted_zone_id,
	at_evaluate_target_health
};

typedef struct r53dbColumnPosition {
	enum r53dbColumn column;
	uint32_t position;
} r53dbColumnPosition;

typedef struct r53dbScanState {
	char *hosted_zone_id;
	List *column_positions;
	List *results;
	int result_index;
} r53dbScanState;

typedef struct r53dbModifyState {
	int junk_row_resno;
	int operation;
	char *hosted_zone_id;
	List *column_positions;
} r53dbModifyState;

#endif // R53DB_FDW_H

