FROM golang:1.14.3-alpine3.11 AS build

COPY main.go .

ENV CGO_ENABLED=0

RUN go build -trimpath -o /server main.go

FROM scratch

COPY --from=build /server /

VOLUME ["/var/run/postgresql"]

ENTRYPOINT ["/server"]
