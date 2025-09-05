#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mbedtls/md.h"
#include <stdlib.h>

#include "ui.h"

#define TOTP_DIGITS 6
#define TOTP_STEP 30 // 30 secondi per step
#define TOTP_T0 0

int generate_otp(const char *base32_secret, char *out_buf, size_t out_len);