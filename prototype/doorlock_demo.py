"""Offline, hardware-free demonstration of the door-lock access flow.

Recognition outcomes are simulated input; this module does not recognize faces
or control a physical lock.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from enum import Enum
from typing import Callable


class State(str, Enum):
    LOCKED = "LOCKED"
    UNLOCKED = "UNLOCKED"
    CAMERA_ERROR = "CAMERA_ERROR"


@dataclass(frozen=True)
class Event:
    state: State
    display: str
    actuator: str


class DoorLock:
    """Small policy/state layer that can later sit between camera and GPIO."""

    def __init__(
        self,
        enrolled: set[str],
        confidence_threshold: float = 0.70,
        unlock_seconds: float = 5.0,
        clock: Callable[[], float] | None = None,
    ) -> None:
        if not 0.0 <= confidence_threshold <= 1.0:
            raise ValueError("confidence_threshold must be between 0 and 1")
        if unlock_seconds <= 0:
            raise ValueError("unlock_seconds must be positive")
        import time

        self.enrolled = set(enrolled)
        self.confidence_threshold = confidence_threshold
        self.unlock_seconds = unlock_seconds
        self._clock = clock or time.monotonic
        self.state = State.LOCKED
        self._unlock_until: float | None = None

    def tick(self) -> Event:
        if self.state == State.UNLOCKED and self._clock() >= self._unlock_until:
            self.state = State.LOCKED
            self._unlock_until = None
            return Event(self.state, "Door relocked", "LOCK (simulated)")
        return self._event("Door unlocked" if self.state == State.UNLOCKED else "Door locked")

    def camera_error(self) -> Event:
        # Fail secure: camera failure must never grant access.
        self.state = State.CAMERA_ERROR
        self._unlock_until = None
        return self._event("Camera unavailable - access denied")

    def recognize(self, identity: str | None, confidence: float) -> Event:
        if not 0.0 <= confidence <= 1.0:
            raise ValueError("confidence must be between 0 and 1")
        if identity in self.enrolled and confidence >= self.confidence_threshold:
            self.state = State.UNLOCKED
            self._unlock_until = self._clock() + self.unlock_seconds
            return self._event(f"Welcome, {identity}")
        self.state = State.LOCKED
        self._unlock_until = None
        return self._event("Access denied")

    def _event(self, message: str) -> Event:
        actuator = "UNLOCK (simulated)" if self.state == State.UNLOCKED else "LOCK (simulated)"
        return Event(self.state, message, actuator)


def run_demo(scenario: str) -> list[Event]:
    """Run one scripted scenario using mock recognition events."""
    demo = DoorLock({"Alex"})
    if scenario == "authorized":
        events = [demo.recognize("Alex", 0.93)]
    elif scenario == "unknown":
        events = [demo.recognize("Visitor", 0.96)]
    elif scenario == "low-confidence":
        events = [demo.recognize("Alex", 0.42)]
    else:
        events = [demo.camera_error()]
    return events


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "scenario",
        nargs="?",
        choices=("authorized", "unknown", "low-confidence", "camera-error"),
        default="authorized",
    )
    args = parser.parse_args()
    print("ESP32-S3 DOOR LOCK - OFFLINE SIMULATION (no camera or physical lock)\n")
    for event in run_demo(args.scenario):
        print(f"State:   {event.state.value}")
        print(f"Display: {event.display}")
        print(f"Output:  {event.actuator}\n")
    print("Run with --help to choose another simulated scenario.")


if __name__ == "__main__":
    main()
