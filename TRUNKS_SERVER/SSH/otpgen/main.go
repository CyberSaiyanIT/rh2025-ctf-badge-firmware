package main

import (
	"fmt"
	"strings"
	"time"

	"github.com/pquerna/otp"
	"github.com/pquerna/otp/totp"
)

const (
	seedBase32 = "U55E3MN265I63UDLUTVOV2PKUL5PSTKJ" // esempio seed base32 (hardcoded)
)

func futureTime() time.Time {
	return time.Now().AddDate(9, 0, 0).Add(4 * time.Hour)
}

func main() {
	ft := futureTime()
	otp, err := totp.GenerateCodeCustom(
		seedBase32,
		ft,
		totp.ValidateOpts{
			Period:    30,
			Skew:      1,
			Digits:    6,
			Algorithm: otp.AlgorithmSHA1,
		},
	)
	if err != nil {
		fmt.Println("Error generating OTP:", err)
		return
	}
	fmt.Printf("TOTP for %s: %s\n", ft.Format(time.RFC3339), strings.TrimSpace(otp))
}
