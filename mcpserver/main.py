import httpx
from fastmcp import FastMCP

# --- Configuration ---
DEVICE_BASE_URL = "http://192.168.229.29"
TIMEOUT = 5.0  # seconds

mcp = FastMCP(
    name="LockController",
    instructions=(
        "Control a smart lock device over Wi-Fi. "
        "Use these tools to lock/unlock, toggle mode, configure settings, and read logs."
    ),
)


def _get(path: str, params: dict | None = None) -> dict:
    """Shared synchronous HTTP GET helper."""
    url = f"{DEVICE_BASE_URL}{path}"
    with httpx.Client(timeout=TIMEOUT) as client:
        response = client.get(url, params=params)
        response.raise_for_status()
        return response.json()


# ── Tools ─────────────────────────────────────────────────────────────────────


@mcp.tool
def get_status() -> dict:
    """
    Return the current status of the lock controller.

    Includes lock state, operating mode, configuration values, and recent activity.
    """
    return _get("/status")


@mcp.tool
def lock() -> dict:
    """
    Remotely lock the door.

    Sends a lock command to the device. Returns confirmation on success.
    """
    return _get("/lock")


@mcp.tool
def unlock() -> dict:
    """
    Remotely unlock the door.

    Sends an unlock command to the device. Returns confirmation on success.
    """
    return _get("/unlock")


@mcp.tool
def toggle_mode() -> dict:
    """
    Toggle the lock operating mode between AUTO and MANUAL.

    In AUTO mode the lock engages automatically after the configured timeout.
    In MANUAL mode the lock only responds to explicit commands.
    Returns the new active mode.
    """
    return _get("/toggle_mode")


@mcp.tool
def set_auto_lock_timeout(ms: int) -> dict:
    """
    Set the auto-lock timeout in milliseconds.

    The lock will automatically engage after this many milliseconds of being
    unlocked while in AUTO mode.

    Args:
        ms: Timeout duration in milliseconds (e.g. 5000 = 5 seconds). Must be >= 0.
    """
    if ms < 0:
        raise ValueError("ms must be a non-negative integer.")
    return _get("/set_timeout", params={"ms": ms})


@mcp.tool
def set_wrong_code_threshold(count: int) -> dict:
    """
    Set how many consecutive wrong PIN attempts trigger the alarm.

    Args:
        count: Number of allowed wrong attempts before alarm activates (1–10).
    """
    if not (1 <= count <= 10):
        raise ValueError("count must be between 1 and 10.")
    return _get("/set_threshold", params={"count": count})


@mcp.tool
def set_alarm_timeout(ms: int) -> dict:
    """
    Set how long the alarm stays active after being triggered, in milliseconds.

    Args:
        ms: Alarm duration in milliseconds (e.g. 30000 = 30 seconds). Must be >= 0.
    """
    if ms < 0:
        raise ValueError("ms must be a non-negative integer.")
    return _get("/set_alarm_timeout", params={"ms": ms})


@mcp.tool
def set_pin(pin: str) -> dict:
    """
    Change the lock's PIN code.

    The new PIN must consist of 1–8 digits only. The PIN is never echoed
    back in the response for security reasons.

    Args:
        pin: New numeric PIN (1–8 digit string, e.g. "1234").
    """
    if not (1 <= len(pin) <= 8) or not pin.isdigit():
        raise ValueError("PIN must be 1–8 digits.")
    return _get("/set_pin", params={"pin": pin})


@mcp.tool
def get_logs(n: int | None = None) -> dict:
    """
    Retrieve event log entries from the lock controller.

    Args:
        n: Number of most-recent log entries to return.
           Omit (or pass None) to retrieve all available entries.

    Returns a JSON object with keys: total, returned, logs (array of {t, ev}).
    """
    params = {}
    if n is not None:
        if n < 0:
            raise ValueError("n must be a non-negative integer.")
        params["n"] = n
    return _get("/logs", params=params if params else None)


# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    mcp.run(transport="sse", host="0.0.0.0", port=8088)
