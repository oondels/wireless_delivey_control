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


def parse_bool(value, default=False):
    if value is None:
        return default
    normalized = value.strip().lower()
    if normalized in ("1", "true", "yes", "on", "sim"):
        return True
    if normalized in ("0", "false", "no", "off", "nao", "não"):
        return False
    raise ValueError(f"Valor booleano invalido no .env: {value}")


def parse_channel(value, default=1):
    if value is None:
        return default
    try:
        channel = int(value)
    except ValueError:
        raise ValueError(f"Canal ESP-NOW invalido no .env: {value}")
    if channel < 1 or channel > 13:
        raise ValueError("ESPNOW_CHANNEL deve estar entre 1 e 13")
    return channel


if not os.path.exists(ENV_PATH):
    raise RuntimeError(
        f"Arquivo .env nao encontrado em {ENV_PATH}. "
        "Crie-o a partir de .env.example antes de compilar."
    )

config = parse_env_file(ENV_PATH)
project_name = os.path.basename(PROJECT_DIR)
is_repeater_project = project_name == "repeater"

missing = [key for key in REQUIRED_KEYS if key not in config]
if missing:
    raise RuntimeError(f"Campos obrigatorios ausentes no .env: {', '.join(missing)}")

try:
    force_repeater_route = parse_bool(config.get("FORCE_REPEATER_ROUTE"), False)
    enable_repeater_route = (
        parse_bool(config.get("ENABLE_REPEATER_ROUTE"), False)
        or force_repeater_route
        or is_repeater_project
    )
    prefer_direct_route = parse_bool(config.get("PREFER_DIRECT_ROUTE"), False)
    espnow_channel = parse_channel(config.get("ESPNOW_CHANNEL"), 1)
except ValueError as exc:
    raise RuntimeError(str(exc))

if not validate_mac(config["PRINCIPAL_MAC"]):
    raise RuntimeError("PRINCIPAL_MAC invalido. Use formato AA:BB:CC:DD:EE:FF")
if not validate_mac(config["REMOTE_MAC"]):
    raise RuntimeError("REMOTE_MAC invalido. Use formato AA:BB:CC:DD:EE:FF")
if enable_repeater_route and "REPEATER_MAC" not in config:
    raise RuntimeError(
        "REPEATER_MAC ausente no .env. Configure quando ENABLE_REPEATER_ROUTE=true "
        "ou ao compilar o modulo repeater."
    )
if enable_repeater_route and not validate_mac(config["REPEATER_MAC"]):
    raise RuntimeError("REPEATER_MAC invalido. Use formato AA:BB:CC:DD:EE:FF")
if not validate_hex_16(config["ESPNOW_PMK"]):
    raise RuntimeError("ESPNOW_PMK invalido. Use 32 hex chars (16 bytes)")
if not validate_hex_16(config["ESPNOW_LMK"]):
    raise RuntimeError("ESPNOW_LMK invalido. Use 32 hex chars (16 bytes)")

defines = [
    ("SEC_PRINCIPAL_MAC_STR", f'\\\"{config["PRINCIPAL_MAC"]}\\\"'),
    ("SEC_REMOTE_MAC_STR", f'\\\"{config["REMOTE_MAC"]}\\\"'),
    ("SEC_ESPNOW_PMK_STR", f'\\\"{config["ESPNOW_PMK"]}\\\"'),
    ("SEC_ESPNOW_LMK_STR", f'\\\"{config["ESPNOW_LMK"]}\\\"'),
    ("SEC_ENABLE_REPEATER_ROUTE", 1 if enable_repeater_route else 0),
    ("SEC_PREFER_DIRECT_ROUTE", 1 if prefer_direct_route else 0),
    ("SEC_FORCE_REPEATER_ROUTE", 1 if force_repeater_route else 0),
    ("SEC_ESPNOW_CHANNEL", espnow_channel),
]

if enable_repeater_route:
    defines.append(("SEC_REPEATER_MAC_STR", f'\\\"{config["REPEATER_MAC"]}\\\"'))

env.Append(CPPDEFINES=defines)
