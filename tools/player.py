class Player:
    """Reference implementation of the firmware Player (firmware/lib/buddy/player.h)."""

    def __init__(self, anims, start, now_ms=0):
        self.anims = anims
        self.loop_forever = False  # wearable mode: looping anims never hand off on their own
        self._start(start, now_ms)

    def _start(self, anim, now):
        self.anim = anim
        self.step = 0
        self.step_start = now
        self.anim_start = now

    def tick(self, now):
        while True:
            a = self.anims[self.anim]
            ms = a["steps"][self.step]["ms"]
            if now - self.step_start < ms:
                break
            self.step_start += ms
            self.step += 1
            if self.step < len(a["steps"]):
                continue
            self.step = 0
            holding = a["loop"] and (self.loop_forever or (self.step_start - self.anim_start) < a.get("hold_ms", 0))
            if not holding:
                self.anim = a["next"]
                self.anim_start = self.step_start
        return self.step

    def play(self, anim, now):
        self._start(anim, now)

    def toggle(self, now):
        if self.anim == "idle":
            self._start("to_coding", now)
        elif self.anim == "coding":
            self._start("to_idle", now)
