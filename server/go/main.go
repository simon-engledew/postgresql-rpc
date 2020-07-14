package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net"
	"os"
)

func accept(conn net.Conn) {
	defer func() {
		if err := conn.Close(); err != nil {
			log.Print(fmt.Errorf("could not close: %w", err))
		}
	}()

	log.Printf("API connection from %s", conn.RemoteAddr())

	len := make([]byte, 4)

	if _, err := conn.Read(len); err != nil {
		log.Print(fmt.Errorf("could not read len: %w", err))
		return
	}

	size := int64(binary.BigEndian.Uint32(len))

	log.Printf("reading %d bytes", size)

	data, err := ioutil.ReadAll(io.LimitReader(conn, size))
	if err != nil {
		log.Print(fmt.Errorf("could not read data: %w", err))
		return
	}

	conn.Write(len)

	io.Copy(conn, bytes.NewReader(data))

	fmt.Print(string(data))
}

func main() {
	path := os.Args[1]

	if err := os.RemoveAll(path); err != nil {
		log.Fatal(err)
	}

	internal, err := net.Listen("unix", path)
	if err != nil {
		panic(err)
	}

	for {
		conn, err := internal.Accept()
		if err != nil {
			fmt.Print(err)
			continue
		}

		go accept(conn)
	}
}
