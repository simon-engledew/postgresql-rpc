.PHONY: docker-build
docker-build:
	docker build extension -t postgres-rpc


.PHONY: docker-run
docker-run: docker-build
	docker run --name postgres-rpc --rm -it -v "pgrpc-sock:/var/run/postgresql" -v "pgrpc-data:/var/lib/postgresql/data" postgres-rpc

.PHONY: docker-test
docker-test:
	echo "SELECT rpc('[1, 2, 3]')" | docker exec -i postgres-rpc psql -U  admin test

.PHONY: build-server
build-server:
#	go run server/go/main.go $(abspath rpc.sock)
	docker build server/go -t postgres-rpc-server

.PHONY: run-server
run-server: build-server
	docker run -it -v "pgrpc-sock:/var/run/postgresql" postgres-rpc-server /var/run/postgresql/rpc.sock
