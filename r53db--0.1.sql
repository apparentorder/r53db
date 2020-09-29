CREATE FUNCTION r53db_fdw_handler()
RETURNS fdw_handler
LANGUAGE C
AS 'MODULE_PATHNAME', 'r53db_fdw_handler';

CREATE FOREIGN DATA WRAPPER r53db
HANDLER r53db_fdw_handler;

