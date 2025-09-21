package main

import (
	"encoding/binary"
	"log"
	"net"
	"time"
)

const (
	ntpEpochOffset = 2208988800 // Seconds between 1900 and 1970
	port           = ":123"
)

func main() {
	addr, err := net.ResolveUDPAddr("udp", port)
	if err != nil {
		log.Fatalf("Failed to resolve UDP addr: %v", err)
	}
	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		log.Fatalf("Failed to listen on UDP: %v", err)
	}
	defer conn.Close()
	log.Printf("NTP server listening on %s", port)

	buf := make([]byte, 48)
	for {
		n, clientAddr, err := conn.ReadFromUDP(buf)
		if err != nil || n < 48 {
			continue
		}

		// Calcola il tempo attuale + 9 anni + 4 ore
		now := time.Now().AddDate(9, 0, 0).Add(4 * time.Hour)
		secs := uint32(now.Unix() + ntpEpochOffset)
		frac := uint32((float64(now.Nanosecond()) / 1e9) * float64(1<<32))

		resp := make([]byte, 48)
		resp[0] = 0x1C // LI=0, Version=3, Mode=4 (server)
		resp[1] = 2    // Stratum 2 (valore tipico per un server NTP non root)
		// Root Delay, Root Dispersion, Reference ID lasciati a zero
		binary.BigEndian.PutUint32(resp[4:], 1<<16) // Root Delay = 1.0
		binary.BigEndian.PutUint32(resp[8:], 1<<16) // Root Dispersion = 1.0
		copy(resp[12:16], []byte{0, 0, 0, 1})       // Reference ID = 0.0.0.1

		// Reference Timestamp
		binary.BigEndian.PutUint32(resp[16:], secs)
		binary.BigEndian.PutUint32(resp[20:], frac)
		// Originate Timestamp (copiato dal client)
		copy(resp[24:32], buf[40:48])
		// Receive Timestamp
		binary.BigEndian.PutUint32(resp[32:], secs)
		binary.BigEndian.PutUint32(resp[36:], frac)
		// Transmit Timestamp
		binary.BigEndian.PutUint32(resp[40:], secs)
		binary.BigEndian.PutUint32(resp[44:], frac)
		ts := (uint64(secs) << 32) | uint64(frac)
		log.Printf("Transmit Timestamp: %d", ts)

		_, err = conn.WriteToUDP(resp, clientAddr)
		if err != nil {
			log.Printf("Failed to send response: %v", err)
		}
	}
}
