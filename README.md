# About

An extremely simple JSON <-> JSON rpc mechanism for Postgres

Send/receives a big endian uint32 prefixed JSON document to and from a domain socket connection:

```
POSTGRES              SERVER
    |                    |
    |------->JSON-------->
    |                    |
    <--------JSON--------|
```

This compliments pg_notify, allowing triggers and validation functions to hit a backend during a database update.
