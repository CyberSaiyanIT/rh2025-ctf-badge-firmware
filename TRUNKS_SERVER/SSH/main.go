package main

import (
	"fmt"
	"io"
	"log"
	"os"
	"strings"
	"time"

	"os/exec"

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
			//username := "trunks"
			/*usr, err := user.Lookup(username)
			if err != nil {
				fmt.Fprintln(s, "User not found.")
				return
			}*/
			//uid, _ := strconv.Atoi(usr.Uid)
			//gid, _ := strconv.Atoi(usr.Gid)

			sessionEnvironment := s.Environ()
			// convert env vars to map
			envMap := make(map[string]string)
			envMap["TERM"] = "xterm-256color" // default
			for _, envVar := range sessionEnvironment {
				parts := strings.SplitN(envVar, "=", 2)
				if len(parts) == 2 {
					key := parts[0]
					value := parts[1]
					envMap[key] = value
				}
			}
			now := time.Now().UnixNano()
			cidFile := fmt.Sprintf("/tmp/ctf_%d.cid", now)
			cmd := exec.Command("docker", "run", "--rm", "-it",
				"--cidfile="+cidFile,                         // File per salvare il container ID
				"--name=ctf_session_"+fmt.Sprintf("%d", now), // Nome unico per il container
				"--hostname=trunks_"+fmt.Sprintf("%d", now),  // Imposta l'hostname del container
				"--cap-drop=ALL",                             // Rimuove tutte le capabilities
				"--security-opt", "no-new-privileges",        // Impedisce l'elevazione dei privilegi
				//"--read-only",                        // Monta il filesystem del container in modalità read-only
				"--tmpfs", "/tmp", // Fornisce una directory temporanea in RAM
				"--tmpfs", "/run", // Fornisce una directory temporanea in RAM
				"--network", "none",
				"--security-opt", "apparmor=docker-default", // Usa un profilo AppArmor predefinito
				"ghcr.io/patriziotufarolo/trunks-filesystem:latest", "/bin/bash")
			/*cmd.SysProcAttr = &syscall.SysProcAttr{
				Setpgid: true, // Crea un nuovo gruppo di processi
			}*/
			/*
					cmd := exec.Command("/bin/bash")
					cmd.Dir = usr.HomeDir
					cmd.Env = []string{
						"TERM=" + envMap["TERM"],
						"LANG=" + envMap["LANG"],
						"LANGUAGE=" + envMap["LANGUAGE"],
						"LC_ALL=" + envMap["LC_ALL"],
						"PS1=\\[\\e[1;36m\\]\\u@\\h:\\w\\$ \\[\\e[0m\\]",
						"HOME=" + usr.HomeDir,
						"USER=" + usr.Username,
						"HISTFILE=",
						"LOGNAME=" + usr.Username,
						"SHELL=/bin/sh",
						"PATH=/usr/local/bin:/usr/bin:/bin",
					}
				cmd.SysProcAttr = &syscall.SysProcAttr{
					Credential: &syscall.Credential{
						Uid: uint32(uid),
						Gid: uint32(gid),
					},
				}*/
			f, _ := pty.Start(cmd)
			go func() {
				for win := range winCh {
					pty.Setsize(f, &pty.Winsize{Cols: uint16(win.Width), Rows: uint16(win.Height)})
				}
			}()
			go func() { io.Copy(f, s) }()
			go func() { io.Copy(s, f) }()
			go func() {
				<-s.Context().Done()
				log.Println("Session context canceled")
				data, e := os.ReadFile(cidFile)
				if e == nil {
					id := strings.TrimSpace(string(data))
					exec.Command("docker", "kill", id).Run()
				}
				_ = os.Remove(cidFile)
				if cmd.Process != nil {
					_ = cmd.Process.Kill()
				}
			}()
			cmd.Wait()
		} else {
			fmt.Fprintln(s, "No PTY requested.")
		}
	})

	log.Println("SSH server listening on :22")

	testmode := os.Getenv("TESTMODE") == "1"

	var err error
	if testmode {
		err = ssh.ListenAndServe(":2222", nil, ssh.HostKeyFile("server.key")) // ssh.PasswordAuth(func(ctx ssh.Context, pass string) bool { return true }), // accetta qualsiasi password
	} else {
		err = ssh.ListenAndServe(":22", nil, ssh.HostKeyFile("/etc/server.key")) // ssh.PasswordAuth(func(ctx ssh.Context, pass string) bool { return true }), // accetta qualsiasi password
	}
	if err != nil {
		log.Fatal(err)
	}
}
