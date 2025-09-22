#!/bin/bash
set -e

DRGERO_KEY_URL="https://api.github.com/users/drgero-cs/keys"

#KEY=$(curl $DRGERO_KEY_URL | jq -r '.[].key')
KEY="ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDVn0CoThTqLIAlgHrEfWK0bgyz6T1083j99FKoXlThxa9OqPDfkeMb4CcFos7xf2l5JSq4g6JgwNapLODZ8UAg7YJ/uj435h1qZuKK/CsDlpud5rZW8y4To+FPwwG8ro+r5t0M/RUyAPDscQ0M/+6GU7kPeQFHEatCegQKg3xRS+rWD6NshUDXJf/HhaFsE5Dg0/t5fiiiCzTuPqrqSjJja+TXIJ7k9TkR170mh+MzWX2SYkHk5rD8jLbw8CWP4lHxSatun39vFVZIYHg/xZMvaY8ZmSCtX19eAoLdKM7y7MBfzWZzrdUqEhlOxYdba89B0Tycl5n8LhPQsMr79R9X"
echo "Retrieved key $KEY"
echo

echo "Converting to PEM with ssh-keygen -e -m pem:"
PEM_KEY=$(ssh-keygen -f /dev/stdin -e -m pem <<<"$KEY")
echo $PEM_KEY
echo

echo "Getting modulus with openssl:"
MODULUS=$(echo "$PEM_KEY" | openssl rsa -pubin -modulus -noout | sed 's/Modulus=//g')
echo $MODULUS
echo

echo "Calculating decimal value with bc:"
DECIMAL=$(echo "ibase=16; $MODULUS" | bc | tr -d '[:space:]')
echo $DECIMAL
echo

echo "Querying P and Q from factordb.com:"
FACTORDB_URL="http://factordb.com/api?query=$DECIMAL"
RESPONSE=$(curl -s $FACTORDB_URL)
P=$(echo $RESPONSE | jq -r '.factors[0][0]')
Q=$(echo $RESPONSE | jq -r '.factors[1][0]')
echo "P: $P"
echo "Q: $Q"

echo "Generating private key with Python"
RSA=$(
  python3 - <<EOF
import sys
from Crypto.PublicKey import RSA
from Crypto.Util.number import long_to_bytes
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.backends import default_backend
p = $P 
q = $Q 
n=p*q
e=65537
key = RSA.construct((n, e, pow(e, -1, (p-1)*(q-1)), p, q))
print(f"key=RSA.construct(({n}, {e}, {pow(e, -1, (p-1)*(q-1))}, {p}, {q}))", file=sys.stderr)
pem = key.export_key()
from cryptography.hazmat.primitives.serialization import load_pem_private_key
private_key = load_pem_private_key(pem, password=None, backend=default_backend())
openssh_private = private_key.private_bytes(
    encoding=serialization.Encoding.PEM,
    format=serialization.PrivateFormat.OpenSSH,
    encryption_algorithm=serialization.NoEncryption()
)
print(openssh_private.decode())
EOF
)
echo "$RSA"
echo


