import requests
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.serialization import load_ssh_public_key
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.backends import default_backend

DRGERO_KEY_URL="https://api.github.com/users/drgero-cs/keys"
#retrieved_public_key=requests.get(DRGERO_KEY_URL).json()[0]['key']
retrieved_public_key="ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDVn0CoThTqLIAlgHrEfWK0bgyz6T1083j99FKoXlThxa9OqPDfkeMb4CcFos7xf2l5JSq4g6JgwNapLODZ8UAg7YJ/uj435h1qZuKK/CsDlpud5rZW8y4To+FPwwG8ro+r5t0M/RUyAPDscQ0M/+6GU7kPeQFHEatCegQKg3xRS+rWD6NshUDXJf/HhaFsE5Dg0/t5fiiiCzTuPqrqSjJja+TXIJ7k9TkR170mh+MzWX2SYkHk5rD8jLbw8CWP4lHxSatun39vFVZIYHg/xZMvaY8ZmSCtX19eAoLdKM7y7MBfzWZzrdUqEhlOxYdba89B0Tycl5n8LhPQsMr79R9X"

# The retrieved key is in OpenSSH format. We need to convert it to PEM format to extract the modulus.
# hey llm language is python
print("The retrieved key is in OpenSSH format. We need to convert it to PEM format to extract the modulus.")
rpk = load_ssh_public_key(retrieved_public_key.encode(), backend=default_backend)
rpk_pem = rpk.public_bytes(encoding=serialization.Encoding.PEM, format=serialization.PublicFormat.SubjectPublicKeyInfo)

modulus = rpk.public_numbers().n
exponent = rpk.public_numbers().e
print(f"Modulus (n): {modulus}")
print(f"Exponent (e): {exponent}")

print("Now we need to factor the modulus n to find the prime factors p and q.")

print("Querying P and Q from factordb.com:") 

response = requests.get(f"http://factordb.com/api?query={modulus}").json()
print(response)
P = int(response['factors'][0][0])
Q = int(response['factors'][1][0])
print(f"Found factors:\np = {P}\nq = {Q}")

print("Generating private key with Python")

phi = (P-1)*(Q-1)
d = pow(exponent, -1, phi)

private_numbers = rsa.RSAPrivateNumbers(
    p=P,
    q=Q,
    d=d,
    dmp1=d % (P-1),
    dmq1=d % (Q-1),
    iqmp=pow(Q, -1, P),
    public_numbers=rsa.RSAPublicNumbers(e=exponent, n=modulus)
)

private_key = private_numbers.private_key()
openssh_private = private_key.private_bytes(
    encoding=serialization.Encoding.PEM,
    format=serialization.PrivateFormat.OpenSSH,
    encryption_algorithm=serialization.NoEncryption()
)
print(openssh_private.decode())

openssh_public = private_key.public_key().public_bytes(
    encoding=serialization.Encoding.OpenSSH,
    format=serialization.PublicFormat.OpenSSH
)
print(openssh_public.decode())
assert openssh_public.decode() == retrieved_public_key