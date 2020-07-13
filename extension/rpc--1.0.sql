\echo Use "CREATE EXTENSION rpc" to load this file. \quit

CREATE OR REPLACE FUNCTION rpc(request JSON) RETURNS JSON AS 'MODULE_PATHNAME', 'rpc_request' LANGUAGE 'c';
