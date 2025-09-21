package main

import (
	"fmt"
	"io"
	"log"
	"os/exec"
	"strconv"
	"strings"
	"syscall"
	"time"

	"os/user"

	"github.com/creack/pty"
	"github.com/gliderlabs/ssh"
	"github.com/pquerna/otp"
	"github.com/pquerna/otp/totp"
	terminal "golang.org/x/term"
)

const (
	seedBase32 = "U55E3MN265I63UDLUTVOV2PKUL5PSTKJ" // esempio seed base32 (hardcoded)
)

var digitArt = map[rune][]string{
	'0': {
		" ███ ",
		"█   █",
		"█   █",
		"█   █",
		" ███ ",
	},
	'1': {
		"  █  ",
		" ██  ",
		"  █  ",
		"  █  ",
		" ███ ",
	},
	'2': {
		" ███ ",
		"    █",
		" ███ ",
		"█    ",
		"█████",
	},
	'3': {
		"████ ",
		"    █",
		" ███ ",
		"    █",
		"████ ",
	},
	'4': {
		"█  █ ",
		"█  █ ",
		"█████",
		"   █ ",
		"   █ ",
	},
	'5': {
		"█████",
		"█    ",
		"████ ",
		"    █",
		"████ ",
	},
	'6': {
		" ███ ",
		"█    ",
		"████ ",
		"█   █",
		" ███ ",
	},
	'7': {
		"█████",
		"   █ ",
		"  █  ",
		" █   ",
		" █   ",
	},
	'8': {
		" ███ ",
		"█   █",
		" ███ ",
		"█   █",
		" ███ ",
	},
	'9': {
		" ███ ",
		"█   █",
		" ████",
		"    █",
		" ███ ",
	},
	':': {
		"     ",
		"  █  ",
		"     ",
		"  █  ",
		"     ",
	},
}

func asciiClock(t time.Time) string {
	colors := []string{"\033[31m", "\033[33m", "\033[32m", "\033[36m", "\033[34m", "\033[35m"}
	reset := "\033[0m"

	clock := t.Format("15:04")
	date := t.Format("02.01.2006") // giorno.mese.anno

	linesClock := make([]string, 5)
	for i, ch := range clock {
		color := colors[i%len(colors)]
		art := digitArt[ch]
		for l := 0; l < 5; l++ {
			linesClock[l] += color + art[l] + reset + " "
		}
	}
	linesDate := make([]string, 5)
	for i, ch := range date {
		color := colors[(i+3)%len(colors)]
		art, ok := digitArt[ch]
		if !ok {
			art = []string{"     ", "     ", "     ", "     ", "     "}
		}
		for l := 0; l < 5; l++ {
			linesDate[l] += color + art[l] + reset + " "
		}
	}
	return strings.Join(linesClock, "\n") + "\n" + strings.Join(linesDate, "\n")
}

func futureTime() time.Time {
	return time.Now().AddDate(9, 0, 0).Add(4 * time.Hour)
}

func main() {
	ssh.Handle(func(s ssh.Session) {
		ft := futureTime()
		clockArt := asciiClock(ft)
		fmt.Fprintln(s, "\n\033[1;37mtrunks ssh server - capscorp\033[0m")
		fmt.Fprintln(s, clockArt)
		fmt.Fprintln(s, "\nauthenticate with totp:")

		// Leggi TOTP
		term := terminal.NewTerminal(s, "> ")
		input, err := term.ReadLine()
		if err != nil {
			fmt.Fprintln(s, "Error.")
			return
		}

		// Calcola TOTP con orario falsato
		valid, err := totp.ValidateCustom(strings.TrimSpace(input), seedBase32, ft, totp.ValidateOpts{
			Period:    30,
			Skew:      1,
			Digits:    6,
			Algorithm: otp.AlgorithmSHA1,
		})

		if !valid || err != nil {
			fmt.Fprintln(s, "\033[1;31mTOTP invalid. Are you Dr Gero?\033[0m")
			return
		}

		fmt.Fprint(s, "\033[1;32mLogin succeeded...\033[0m\n\n")

		if _, winCh, isPty := s.Pty(); isPty {
			username := "trunks"
			usr, err := user.Lookup(username)
			if err != nil {
				fmt.Fprintln(s, "User not found.")
				return
			}
			uid, _ := strconv.Atoi(usr.Uid)
			gid, _ := strconv.Atoi(usr.Gid)

			cmd := exec.Command("/bin/sh")
			cmd.Dir = usr.HomeDir
			cmd.Env = []string{
				"HOME=" + usr.HomeDir,
				"USER=" + usr.Username,
				"LOGNAME=" + usr.Username,
				"SHELL=/bin/sh",
				"PATH=/usr/local/bin:/usr/bin:/bin",
			}
			cmd.SysProcAttr = &syscall.SysProcAttr{
				Credential: &syscall.Credential{
					Uid: uint32(uid),
					Gid: uint32(gid),
				},
			}
			f, _ := pty.Start(cmd)
			go func() {
				for win := range winCh {
					pty.Setsize(f, &pty.Winsize{Cols: uint16(win.Width), Rows: uint16(win.Height)})
				}
			}()
			go func() { io.Copy(f, s) }()
			io.Copy(s, f)
			cmd.Wait()
		} else {
			fmt.Fprintln(s, "No PTY requested.")
		}
	})

	log.Println("SSH server listening on :22")
	err := ssh.ListenAndServe(":22", nil, ssh.HostKeyFile("/etc/server.key")) // ssh.PasswordAuth(func(ctx ssh.Context, pass string) bool { return true }), // accetta qualsiasi password

	if err != nil {
		log.Fatal(err)
	}
}
