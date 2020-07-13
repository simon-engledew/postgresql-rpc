#!/bin/sh
set -e

if test "$(id -u)" -ne 0 ; then
   echo "This script must be run as root"
   exit 1
fi

chown -R postgres "${PGDATA}" /var/run/postgresql

if ! su-exec postgres test -f "${PGDATA}/PG_VERSION"; then
  su-exec postgres initdb -k -E UTF8 --locale=en_US.UTF-8

  su-exec postgres postgres --single -jE > /dev/null <<__SQL__
CREATE DATABASE test;
__SQL__
  su-exec postgres postgres --single -jE > /dev/null <<__SQL__
CREATE USER admin SUPERUSER;
__SQL__
fi

exec su-exec postgres "$@"
