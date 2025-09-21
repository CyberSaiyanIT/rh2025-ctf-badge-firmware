package main

import (
	"fmt"
	"log"
	"os"
	"strings"
	"time"

	"github.com/gliderlabs/ssh"
	"github.com/golang-jwt/jwt/v5"
	"golang.org/x/term"
)

var redRibbonArt = `
[49m [38;5;9;49m▄▄▄[38;5;167;49m▄[49m                                                                       [38;5;9;49m▄▄▄[49m [m
[49m [48;5;9m      [38;5;9;48;5;9m▄[48;5;9m [38;5;9;49m▄▄▄▄[38;5;224;49m▄[49m                                                     [38;5;9;49m▄▄▄▄[38;5;9;48;5;160m▄[48;5;9m       [49m [m
[49m [48;5;9m      [38;5;9;48;5;9m▄[48;5;9m    [38;5;9;48;5;9m▄[48;5;9m    [38;5;9;48;5;160m▄[38;5;9;49m▄▄▄▄[49m                                    [38;5;9;49m▄▄▄▄[38;5;9;48;5;167m▄[48;5;9m      [38;5;9;48;5;9m▄[48;5;9m         [49m [m
[49m [48;5;9m   [38;5;9;48;5;9m▄[48;5;9m      [38;5;9;48;5;9m▄[48;5;9m [38;5;9;48;5;9m▄[48;5;9m            [38;5;9;48;5;174m▄[38;5;9;49m▄▄▄▄[49m                  [38;5;9;49m▄▄▄▄[38;5;9;48;5;167m▄[48;5;9m              [38;5;9;48;5;9m▄[48;5;9m      [38;5;9;48;5;9m▄[48;5;9m   [49m [m
[49m [48;5;9m [38;5;9;48;5;9m▄[48;5;9m         [38;5;15;48;5;9m▄▄▄▄▄▄▄▄▄▄[38;5;255;48;5;9m▄[38;5;160;48;5;9m▄[48;5;9m          [38;5;9;48;5;9m▄[38;5;9;49m▄▄▄▄[38;5;167;49m▄[38;5;161;49m▄[38;5;9;49m▄▄▄[38;5;9;48;5;254m▄[38;5;9;48;5;9m▄[48;5;9m   [38;5;217;48;5;9m▄▄▄▄▄▄▄▄▄▄[38;5;204;48;5;9m▄[38;5;9;48;5;9m▄[48;5;9m        [38;5;9;48;5;160m▄[48;5;9m      [38;5;9;48;5;9m▄[48;5;9m  [49m [m
[49m [48;5;9m    [38;5;9;48;5;9m▄[48;5;9m     [38;5;9;48;5;9m▄[48;5;15m        [38;5;15;48;5;15m▄[48;5;15m     [38;5;15;48;5;9m▄[38;5;9;48;5;9m▄[48;5;9m                     [48;5;15m            [38;5;15;48;5;15m▄▄▄[38;5;15;48;5;9m▄[48;5;9m     [38;5;9;48;5;9m▄[48;5;9m        [49m [m
[49m [48;5;9m        [38;5;9;48;5;9m▄[48;5;9m  [48;5;15m    [38;5;15;48;5;15m▄[38;5;9;48;5;15m▄▄▄▄▄[48;5;15m  [38;5;15;48;5;15m▄[48;5;15m   [48;5;9m       [38;5;9;48;5;9m▄[48;5;9m    [38;5;9;48;5;9m▄[48;5;9m [38;5;9;48;5;9m▄[48;5;9m   [38;5;9;48;5;9m▄[48;5;9m  [48;5;15m  [38;5;15;48;5;15m▄[48;5;15m   [38;5;9;48;5;15m▄▄▄▄[38;5;217;48;5;15m▄[48;5;15m     [38;5;15;48;5;9m▄[48;5;9m  [38;5;9;48;5;9m▄[48;5;9m          [49m [m
[49m [48;5;9m     [38;5;9;48;5;9m▄▄[48;5;9m    [48;5;15m     [38;5;9;48;5;9m▄[48;5;9m    [38;5;15;48;5;224m▄[48;5;15m [38;5;15;48;5;15m▄[48;5;15m   [48;5;9m    [38;5;9;48;5;9m▄[48;5;9m  [38;5;9;48;5;9m▄[48;5;9m             [38;5;15;48;5;15m▄[48;5;15m     [48;5;9m     [48;5;15m  [38;5;15;48;5;15m▄[48;5;15m  [38;5;167;48;5;15m▄[38;5;9;48;5;9m▄[48;5;9m            [49m [m
[49m [48;5;9m     [38;5;9;48;5;9m▄[48;5;9m [38;5;9;48;5;9m▄[48;5;9m   [48;5;15m         [38;5;15;48;5;15m▄[48;5;15m     [38;5;9;48;5;15m▄[48;5;9m                 [38;5;9;48;5;9m▄[48;5;9m   [48;5;15m      [38;5;15;48;5;224m▄▄[38;5;15;48;5;15m▄[48;5;15m      [38;5;9;48;5;15m▄[48;5;9m [38;5;9;48;5;9m▄[48;5;9m            [49m [m
[49m [38;5;9;48;5;9m▄[48;5;9m     [38;5;9;48;5;9m▄[48;5;9m [38;5;9;48;5;9m▄[48;5;9m  [48;5;15m     [38;5;15;48;5;15m▄[48;5;15m      [38;5;15;48;5;15m▄[38;5;9;48;5;167m▄[48;5;9m                     [38;5;9;48;5;9m▄[48;5;9m [48;5;15m             [38;5;9;48;5;15m▄[38;5;9;48;5;9m▄[48;5;9m           [38;5;9;48;5;9m▄[48;5;9m   [49m [m
[49m [48;5;9m         [38;5;9;48;5;9m▄▄[48;5;15m    [38;5;15;48;5;15m▄[38;5;9;48;5;9m▄[48;5;9m [38;5;9;48;5;9m▄[38;5;15;48;5;15m▄[48;5;15m   [38;5;15;48;5;15m▄[48;5;15m [38;5;15;48;5;9m▄[48;5;9m    [38;5;9;48;5;9m▄[48;5;9m         [38;5;9;48;5;9m▄[48;5;9m      [38;5;9;48;5;9m▄[48;5;15m      [48;5;9m  [38;5;9;48;5;15m▄[48;5;15m     [38;5;15;48;5;160m▄[48;5;9m    [38;5;9;48;5;9m▄[48;5;9m       [38;5;9;48;5;9m▄[48;5;9m  [49m [m
[49m [48;5;9m [38;5;9;48;5;9m▄[48;5;9m         [48;5;15m   [38;5;15;48;5;15m▄[48;5;15m [48;5;9m    [38;5;15;48;5;15m▄[48;5;15m     [38;5;15;48;5;9m▄[48;5;9m                 [38;5;9;48;5;9m▄[48;5;9m   [38;5;15;48;5;15m▄[48;5;15m     [48;5;9m   [38;5;9;48;5;15m▄[48;5;15m      [48;5;9m [38;5;9;48;5;9m▄[48;5;9m [38;5;9;48;5;9m▄[48;5;9m         [38;5;9;48;5;9m▄[49m [m
[49m [48;5;9m           [48;5;15m     [38;5;9;48;5;9m▄[48;5;9m    [38;5;9;48;5;15m▄[48;5;15m     [38;5;15;48;5;9m▄[48;5;9m                [38;5;9;48;5;9m▄[48;5;9m   [48;5;15m  [38;5;15;48;5;15m▄[48;5;15m   [48;5;9m    [38;5;9;48;5;254m▄[48;5;15m    [38;5;15;48;5;15m▄[48;5;15m [38;5;9;48;5;9m▄▄[48;5;9m         [38;5;9;48;5;9m▄[48;5;9m [49m [m
[49m [48;5;9m     [38;5;9;48;5;9m▄[48;5;9m     [38;5;9;48;5;15m▄▄▄▄▄[48;5;9m      [38;5;9;48;5;15m▄▄▄▄▄▄[38;5;9;48;5;217m▄[48;5;9m         [49;38;5;9m▀▀[38;5;181;48;5;9m▄[48;5;9m [38;5;9;48;5;9m▄[48;5;9m     [38;5;9;48;5;15m▄▄▄▄▄▄[48;5;9m     [38;5;9;48;5;9m▄[38;5;9;48;5;15m▄▄▄▄▄▄[48;5;9m           [38;5;9;48;5;9m▄[49m [m
[49m [48;5;9m      [38;5;9;48;5;9m▄[48;5;9m   [38;5;9;48;5;9m▄[48;5;9m        [38;5;9;48;5;9m▄[48;5;9m    [38;5;9;48;5;9m▄[48;5;9m    [38;5;167;48;5;9m▄[49;38;5;9m▀▀▀▀[49m           [49;38;5;9m▀▀▀▀[38;5;167;48;5;9m▄[48;5;9m     [38;5;9;48;5;9m▄[48;5;9m           [38;5;9;48;5;9m▄[48;5;9m          [49m [m
[49m [48;5;9m            [38;5;9;48;5;9m▄[48;5;9m      [38;5;9;48;5;9m▄▄[49;38;5;9m▀▀▀▀[49m                             [49;38;5;9m▀▀▀▀[38;5;9;48;5;9m▄[48;5;9m     [38;5;9;48;5;9m▄[48;5;9m  [38;5;9;48;5;9m▄[48;5;9m     [38;5;9;48;5;9m▄[48;5;9m    [49m [m
[49m [48;5;9m       [38;5;9;48;5;9m▄[48;5;9m    [49;38;5;9m▀▀▀▀[49;38;5;210m▀[49m                                             [49;38;5;181m▀[49;38;5;9m▀▀▀▀[38;5;9;48;5;9m▄[48;5;9m          [49m [m
[49m [48;5;9m   [38;5;224;48;5;9m▄[49;38;5;9m▀▀▀[49;38;5;160m▀[49m                                                               [49;38;5;211m▀[49;38;5;9m▀▀▀▀[48;5;9m  [49m [m
[49m                                                                                [m
`

func loadAllowedKey(path string) (ssh.PublicKey, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	key, _, _, _, err := ssh.ParseAuthorizedKey(data)
	return key, err
}

func main() {
	allowedKey, err := loadAllowedKey("drgero.pub")
	if err != nil {
		log.Fatalf("Could not load public key: %v", err)
	}

	ssh.Handle(func(s ssh.Session) {
		fmt.Fprintln(s, redRibbonArt)
		fmt.Fprintln(s, "Welcome DrGero! You have successfully authenticated with your SSH key.")
		fmt.Fprintln(s, "Press D to deactivate the Androids, or any other key to exit.")

		terminal := term.NewTerminal(s, "")

		input, err := terminal.ReadLine()
		if err != nil {
			fmt.Fprintln(s, "Error reading input")
			return
		}
		if strings.ToLower(input) != "d" {
			fmt.Fprintln(s, "Exiting. Goodbye!")
			return
		}
		fmt.Fprintln(s, "Deactivating the Androids...")
		time.Sleep(2 * time.Second)
		fmt.Fprintln(s, "The Androids have been deactivated.")
		fmt.Fprintln(s, "🔍 Time for the final step! Enter your Badge MAC Address (you’ll find it in the web interface under badge_stats or in the debug mode of the TOTP) to unlock the ultimate flag 🚩:")

		macAddress, err := terminal.ReadLine()
		if err != nil {
			fmt.Fprintln(s, "Error reading MAC address")
			return
		}

		fmt.Fprintf(s, "The flag is: \033[1m%s\033[0m\n", flag)
		fmt.Fprint(s, "\033[1m🎉 Well done, hacker! You successfully completed the RomHack 2025 Capture The Flag! 🚩\033[0m\n\n")
		fmt.Fprint(s, "\033[1m⚡ Be among the fastest! Send this token to ctf@cybersaiyan.it and, if you're one of the first three, you'll win a FREE entry to RomHack Camp 2026! 🏕️🔥\033[0m\n\n")

		token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
			"flag": flag,
			"iat":  time.Now().Unix(),
			"sub":  macAddress,
		})
		signed, err := token.SignedString([]byte(secretKey))
		if err != nil {
			fmt.Fprintln(s, "Error generating JWT")
			return
		}
		fmt.Fprintln(s, signed)
		fmt.Fprint(s, "\n\n")
	})
	log.Println("SSH server listening on :22")
	err = ssh.ListenAndServe(":22", nil,
		ssh.HostKeyFile("/etc/server.key"),
		ssh.PublicKeyAuth(func(ctx ssh.Context, key ssh.PublicKey) bool {
			// ONLY me can log in to this server !!!
			return strings.TrimSpace(ctx.User()) == "DrGero" &&
				ssh.KeysEqual(key, allowedKey)
		}),
	)
	if err != nil {
		log.Fatal(err)
	}
}
