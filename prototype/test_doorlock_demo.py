import unittest

from doorlock_demo import DoorLock, State, run_demo


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def __call__(self):
        return self.now


class DoorLockTests(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock()
        self.lock = DoorLock({"Alex"}, unlock_seconds=5, clock=self.clock)

    def test_enrolled_face_above_threshold_unlocks(self):
        event = self.lock.recognize("Alex", 0.70)
        self.assertEqual(event.state, State.UNLOCKED)
        self.assertIn("Welcome", event.display)

    def test_unknown_identity_stays_locked(self):
        event = self.lock.recognize("Visitor", 0.99)
        self.assertEqual(event.state, State.LOCKED)
        self.assertEqual(event.actuator, "LOCK (simulated)")

    def test_low_confidence_enrolled_face_is_denied(self):
        self.assertEqual(self.lock.recognize("Alex", 0.69).state, State.LOCKED)

    def test_unlock_times_out_and_relocks(self):
        self.lock.recognize("Alex", 0.95)
        self.clock.now = 4.99
        self.assertEqual(self.lock.tick().state, State.UNLOCKED)
        self.clock.now = 5.0
        event = self.lock.tick()
        self.assertEqual(event.state, State.LOCKED)
        self.assertEqual(event.display, "Door relocked")

    def test_camera_error_fails_secure(self):
        event = self.lock.camera_error()
        self.assertEqual(event.state, State.CAMERA_ERROR)
        self.assertEqual(event.actuator, "LOCK (simulated)")

    def test_cli_scenarios(self):
        self.assertEqual(run_demo("authorized")[0].state, State.UNLOCKED)
        self.assertEqual(run_demo("unknown")[0].state, State.LOCKED)
        self.assertEqual(run_demo("camera-error")[0].state, State.CAMERA_ERROR)


if __name__ == "__main__":
    unittest.main()
