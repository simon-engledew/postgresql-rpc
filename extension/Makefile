MODULE_big = rpc
OBJS = rpc.o
EXTENSION = rpc
DATA = rpc--1.0.sql
REGRESS = rpc
EXTRA_CLEAN =

PG_CONFIG = pg_config

ifdef DEBUG
COPT			+= -O0
endif

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
