import os

Import("env")

PROJECT_DIR = env.subst("$PROJECT_DIR")
ROOT_DIR = os.path.abspath(os.path.join(PROJECT_DIR, ".."))
ENV_PATH = os.path.join(ROOT_DIR, ".env")

REQUIRED_KEYS = (
    "PRINCIPAL_MAC",
    "REMOTE_MAC",
    "ESPNOW_PMK",
    "ESPNOW_LMK",
)


def parse_env_file(path):
    values = {}
    with open(path, "r", encoding="ascii") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" not in line:
                raise ValueError(f"Linha invalida no .env: {raw_line.rstrip()}")
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def validate_mac(mac):
    parts = mac.split(":")
    if len(parts) != 6:
        return False
    try:
        return all(len(part) == 2 and 0 <= int(part, 16) <= 255 for part in parts)
    except ValueError:
        return False


def validate_hex_16(value):
    if len(value) != 32:
        return False
    try:
        int(value, 16)
        return True
    except ValueError:
        return False


if not os.path.exists(ENV_PATH):
    raise RuntimeError(
        f"Arquivo .env nao encontrado em {ENV_PATH}. "
        "Crie-o a partir de .env.example antes de compilar."
    )

config = parse_env_file(ENV_PATH)

missing = [key for key in REQUIRED_KEYS if key not in config]
if missing:
    raise RuntimeError(f"Campos obrigatorios ausentes no .env: {', '.join(missing)}")

if not validate_mac(config["PRINCIPAL_MAC"]):
    raise RuntimeError("PRINCIPAL_MAC invalido. Use formato AA:BB:CC:DD:EE:FF")
if not validate_mac(config["REMOTE_MAC"]):
    raise RuntimeError("REMOTE_MAC invalido. Use formato AA:BB:CC:DD:EE:FF")
if not validate_hex_16(config["ESPNOW_PMK"]):
    raise RuntimeError("ESPNOW_PMK invalido. Use 32 hex chars (16 bytes)")
if not validate_hex_16(config["ESPNOW_LMK"]):
    raise RuntimeError("ESPNOW_LMK invalido. Use 32 hex chars (16 bytes)")

env.Append(
    CPPDEFINES=[
        ("SEC_PRINCIPAL_MAC_STR", f'\\\"{config["PRINCIPAL_MAC"]}\\\"'),
        ("SEC_REMOTE_MAC_STR", f'\\\"{config["REMOTE_MAC"]}\\\"'),
        ("SEC_ESPNOW_PMK_STR", f'\\\"{config["ESPNOW_PMK"]}\\\"'),
        ("SEC_ESPNOW_LMK_STR", f'\\\"{config["ESPNOW_LMK"]}\\\"'),
    ]
)
