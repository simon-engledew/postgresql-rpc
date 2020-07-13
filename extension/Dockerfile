FROM alpine:3.12.0 AS build

RUN apk add --no-cache make pkgconf libc-dev musl-dev gcc postgresql-dev \
  ;

WORKDIR /src

COPY . .

RUN DESTDIR=/out make install

FROM alpine:3.12.0

RUN apk add --no-cache su-exec curl libpq postgresql-client postgresql postgresql-contrib

ENV LANG=en_US.utf8 PGDATA=/var/lib/postgresql/data
VOLUME ["/var/run/postgresql", "/var/lib/postgresql/data"]

COPY root/ /

COPY --from=build /out/ /

ENTRYPOINT ["/docker-entrypoint.sh"]

EXPOSE 5432
CMD ["postgres", "-c", "config_file=/etc/postgresql.conf"]
