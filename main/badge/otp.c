#include "otp.h"
#include "esp_log.h"
// --- Base32 decode minimale ---
static int base32_decode(const char *encoded, uint8_t *out, size_t *out_len)
{
    const char *p = encoded;
    uint32_t buffer = 0;
    int bits_left = 0;
    size_t count = 0;
    while (*p)
    {
        char ch = *p++;
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '=')
            continue;
        uint8_t val;
        if (ch >= 'A' && ch <= 'Z')
            val = ch - 'A';
        else if (ch >= 'a' && ch <= 'z')
            val = ch - 'a';
        else if (ch >= '2' && ch <= '7')
            val = 26 + (ch - '2');
        else
            return -1;
        buffer = (buffer << 5) | (val & 0x1F);
        bits_left += 5;
        if (bits_left >= 8)
        {
            bits_left -= 8;
            if (out && count < *out_len)
                out[count] = (uint8_t)((buffer >> bits_left) & 0xFF);
            count++;
        }
    }
    *out_len = count;
    return 0;
}

// --- HMAC-SHA1 ---
static int hmac_sha1(const uint8_t *key, size_t key_len,
                     const uint8_t *msg, size_t msg_len,
                     uint8_t out[20])
{
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (!md_info)
        return -1;
    return mbedtls_md_hmac(md_info, key, key_len, msg, msg_len, out);
}

// --- Genera OTP ---
int generate_otp(const char *base32_secret, char *out_buf, size_t out_len)
{
    if (!base32_secret || !out_buf || out_len < TOTP_DIGITS + 1)
        return -1;

    uint8_t key[64];
    size_t key_len = sizeof(key);
    if (base32_decode(base32_secret, key, &key_len) != 0)
        return -2;

    time_t now = time(NULL);
    uint64_t counter = (now - TOTP_T0) / TOTP_STEP;

    // Pack counter in big-endian
    uint8_t msg[8];
    for (int i = 7; i >= 0; --i)
    {
        msg[i] = counter & 0xFF;
        counter >>= 8;
    }

    uint8_t hash[20];
    if (hmac_sha1(key, key_len, msg, sizeof(msg), hash) != 0)
        return -3;

    int offset = hash[19] & 0x0F;
    uint32_t binary = ((hash[offset] & 0x7F) << 24) |
                      ((hash[offset + 1] & 0xFF) << 16) |
                      ((hash[offset + 2] & 0xFF) << 8) |
                      (hash[offset + 3] & 0xFF);

    uint32_t otp_val = binary % 1000000; // 6 cifre
    snprintf(out_buf, out_len, "%06u", otp_val);
    return 0;
}