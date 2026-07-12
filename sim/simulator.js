(() => {
  const W = 72;
  const H = 40;
  const LONG_MS = 700;
  const END_LOCK_MS = 500;
  const BUILD_TEXT = "v2.1 b82";
  const READING_LINES_PER_PAGE = 3;

  const canvas = document.getElementById("oled");
  const ctx = canvas.getContext("2d");
  const ledEl = document.getElementById("led");
  const buttonEl = document.getElementById("button");

  ctx.imageSmoothingEnabled = false;

  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  const rand = (lo, hi) => Math.floor(lo + Math.random() * (hi - lo));
  const save = (k, v) => localStorage.setItem(`esp32sim:${k}`, String(v));
  const loadNum = (k, fallback = 0) => Number(localStorage.getItem(`esp32sim:${k}`) ?? fallback);
  const loadText = (k, fallback = "JF") => localStorage.getItem(`esp32sim:${k}`) ?? fallback;
  const initials = () => loadText("profile.initials", "JF").slice(0, 2).padEnd(2, "A").toUpperCase();
  const dottedInitials = () => `${initials()[0]}.${initials()[1]}`;
  const saveInitials = (value) => save("profile.initials", value.slice(0, 2).toUpperCase());
  let introAnchorMs = 0;
  const introPage = () => Math.floor((performance.now() - introAnchorMs) / 2000) % 3;
  const drawPromptPage = () => {
    gfx.rect(0, 0, W, H);
    gfx.text(20, 16, "Press", 7);
    gfx.text(13, 29, "to Start", 7);
  };

  const FONT = {
    "A": ["010", "101", "111", "101", "101"],
    "B": ["110", "101", "110", "101", "110"],
    "C": ["111", "100", "100", "100", "111"],
    "D": ["110", "101", "101", "101", "110"],
    "E": ["111", "100", "110", "100", "111"],
    "F": ["111", "100", "110", "100", "100"],
    "G": ["111", "100", "101", "101", "111"],
    "H": ["101", "101", "111", "101", "101"],
    "I": ["111", "010", "010", "010", "111"],
    "J": ["001", "001", "001", "101", "111"],
    "K": ["101", "101", "110", "101", "101"],
    "L": ["100", "100", "100", "100", "111"],
    "M": ["101", "111", "111", "101", "101"],
    "N": ["101", "111", "111", "111", "101"],
    "O": ["111", "101", "101", "101", "111"],
    "P": ["111", "101", "111", "100", "100"],
    "Q": ["111", "101", "101", "111", "001"],
    "R": ["111", "101", "111", "110", "101"],
    "S": ["111", "100", "111", "001", "111"],
    "T": ["111", "010", "010", "010", "010"],
    "U": ["101", "101", "101", "101", "111"],
    "V": ["101", "101", "101", "101", "010"],
    "W": ["101", "101", "111", "111", "101"],
    "X": ["101", "101", "010", "101", "101"],
    "Y": ["101", "101", "010", "010", "010"],
    "Z": ["111", "001", "010", "100", "111"],
    "0": ["111", "101", "101", "101", "111"],
    "1": ["010", "110", "010", "010", "111"],
    "2": ["111", "001", "111", "100", "111"],
    "3": ["111", "001", "111", "001", "111"],
    "4": ["101", "101", "111", "001", "001"],
    "5": ["111", "100", "111", "001", "111"],
    "6": ["111", "100", "111", "101", "111"],
    "7": ["111", "001", "010", "010", "010"],
    "8": ["111", "101", "111", "101", "111"],
    "9": ["111", "101", "111", "001", "111"],
    " ": ["0", "0", "0", "0", "0"],
    ":": ["0", "1", "0", "1", "0"],
    ".": ["0", "0", "0", "0", "1"],
    "!": ["1", "1", "1", "0", "1"],
    "?": ["111", "001", "011", "000", "010"],
    "/": ["001", "001", "010", "100", "100"],
    "-": ["000", "000", "111", "000", "000"],
    "<": ["001", "010", "100", "010", "001"],
    "=": ["000", "111", "000", "111", "000"],
    "$": ["111", "110", "111", "011", "111"]
  };
  const FONT_H = 5;

  function textWidth(s) {
    return Array.from(String(s).toUpperCase()).reduce((w, ch, i) => {
      const glyph = FONT[ch] || FONT["?"];
      return w + glyph[0].length + (i ? 1 : 0);
    }, 0);
  }

  const gfx = {
    clear() {
      ctx.fillStyle = "#000";
      ctx.fillRect(0, 0, W, H);
      ctx.fillStyle = "#f8fbff";
      ctx.strokeStyle = "#f8fbff";
      ctx.lineWidth = 1;
    },
    pixel(x, y) {
      ctx.fillRect(Math.round(x), Math.round(y), 1, 1);
    },
    line(x1, y1, x2, y2) {
      ctx.beginPath();
      ctx.moveTo(Math.round(x1) + 0.5, Math.round(y1) + 0.5);
      ctx.lineTo(Math.round(x2) + 0.5, Math.round(y2) + 0.5);
      ctx.stroke();
    },
    rect(x, y, w, h) {
      ctx.strokeRect(Math.round(x) + 0.5, Math.round(y) + 0.5, Math.round(w), Math.round(h));
    },
    box(x, y, w, h) {
      ctx.fillRect(Math.round(x), Math.round(y), Math.round(w), Math.round(h));
    },
    circle(x, y, r) {
      ctx.beginPath();
      ctx.arc(Math.round(x), Math.round(y), r, 0, Math.PI * 2);
      ctx.stroke();
    },
    disc(x, y, r) {
      ctx.beginPath();
      ctx.arc(Math.round(x), Math.round(y), r, 0, Math.PI * 2);
      ctx.fill();
    },
    tri(x1, y1, x2, y2, x3, y3) {
      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.lineTo(x3, y3);
      ctx.closePath();
      ctx.stroke();
    },
    text(x, y, s, size = 6) {
      let cursor = Math.round(x);
      const top = Math.round(y) - FONT_H + 1;
      for (const rawCh of Array.from(String(s).toUpperCase())) {
        const glyph = FONT[rawCh] || FONT["?"];
        for (let row = 0; row < glyph.length; row++) {
          for (let col = 0; col < glyph[row].length; col++) {
            if (glyph[row][col] === "1") ctx.fillRect(cursor + col, top + row, 1, 1);
          }
        }
        cursor += glyph[0].length + 1;
      }
    },
    center(y, s, size = 6) {
      gfx.text(Math.max(1, Math.round((W - textWidth(s)) / 2)), y, s, size);
    }
  };

  class Button {
    constructor() {
      this.down = false;
      this.wasDown = false;
      this.pressedAt = 0;
      this.longSent = false;
    }
    setDown(down) {
      this.down = down;
      buttonEl.classList.toggle("pressed", down);
    }
    update(now) {
      const input = { down: this.down, pressed: false, released: false, click: false, longPress: false, holdMs: 0 };
      if (this.down && !this.wasDown) {
        input.pressed = true;
        this.pressedAt = now;
        this.longSent = false;
      }
      if (this.down) {
        input.holdMs = now - this.pressedAt;
        if (!this.longSent && input.holdMs >= LONG_MS) {
          input.longPress = true;
          this.longSent = true;
        }
      }
      if (!this.down && this.wasDown) {
        input.released = true;
        if (!this.longSent) input.click = true;
      }
      this.wasDown = this.down;
      return input;
    }
    reset(now) {
      this.wasDown = this.down;
      this.pressedAt = now;
      this.longSent = false;
    }
  }

  class Game {
    constructor(title) {
      this.title = title;
      this.phase = "start";
      this.phaseAt = 0;
    }
    begin(now) {
      this.phase = "start";
      this.phaseAt = now;
    }
    start(now) {
      this.phase = "running";
      this.phaseAt = now;
      this.reset();
    }
    finish(now) {
      this.phase = "end";
      this.phaseAt = now;
    }
    tick(dt, input, now) {
      if (this.phase === "start") {
        if (input.click) this.start(now);
      } else if (this.phase === "running") {
        this.update(dt, input, now);
      } else if (this.phase === "end" && now - this.phaseAt > END_LOCK_MS) {
        if (input.click) this.start(now);
        if (input.longPress) app.toMenu(now);
      }
    }
    render() {
      if (this.phase === "start") {
        introAnchorMs = this.phaseAt;
        this.drawStart();
      }
      if (this.phase === "running") this.draw();
      if (this.phase === "end") this.drawEnd();
    }
    reset() {}
    update() {}
    drawStart() {
      if (introPage() === 2) {
        drawPromptPage();
        return;
      }
      gfx.rect(0, 0, W, H);
      if (introPage() === 1) {
        gfx.text(3, 10, "Top Score", 7);
        gfx.text(3, 24, "--");
      } else {
        gfx.text(3, 10, this.title, 7);
      }
    }
    drawEnd() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 10, "Game Over", 7);
      gfx.text(3, 24, "Tap retry");
      gfx.text(3, 36, "Hold menu");
    }
  }

  class PlaceholderGame extends Game {
    drawStart() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 10, this.title, 7);
      gfx.text(3, 23, "Not ported");
      gfx.text(3, 36, "Hold menu");
    }
    draw() {
      this.drawStart();
    }
  }

  class TowerStacker extends Game {
    constructor() {
      super("Tower Stacker");
      this.best = loadNum("tower.best");
    }
    reset() {
      this.layers = [{ x: 17, w: 36 }];
      this.movingX = 1;
      this.dir = 1;
      this.speed = 16;
      this.score = 0;
    }
    update(dt, input, now) {
      this.movingX += this.dir * this.speed * dt;
      const maxX = 69 - this.layers.at(-1).w;
      if (this.movingX <= 1 || this.movingX >= maxX) {
        this.movingX = clamp(this.movingX, 1, maxX);
        this.dir *= -1;
      }
      if (input.click) {
        const prev = this.layers.at(-1);
        const x = Math.round(this.movingX);
        const left = Math.max(x, prev.x);
        const right = Math.min(x + prev.w, prev.x + prev.w);
        if (right <= left) {
          this.finish(now);
          return;
        }
        this.layers.push({ x: left, w: right - left });
        this.score = this.layers.length - 1;
        if (this.score > this.best) {
          this.best = this.score;
          save("tower.best", this.best);
        }
        this.movingX = 1;
        this.dir = 1;
        this.speed = Math.min(32, 16 + this.score * 0.9);
      }
    }
    draw() {
      gfx.rect(1, 0, 70, 40);
      const visible = 10;
      const first = Math.max(0, this.layers.length - visible);
      this.layers.slice(first).forEach((l, i) => gfx.box(1 + l.x, 37 - i * 3, l.w, 3));
      const topY = 37 - (this.layers.length - first) * 3;
      gfx.rect(1 + this.movingX, topY, this.layers.at(-1).w, 3);
    }
    drawStart() {
      if (introPage() === 2) {
        drawPromptPage();
        return;
      }
      gfx.rect(0, 0, W, H);
      if (introPage() === 1) {
        gfx.text(3, 10, "Top Score", 7);
        gfx.text(3, 24, this.best ? `${dottedInitials()} ${this.best}` : "--");
      } else {
        gfx.text(3, 10, this.title, 7);
      }
    }
    drawEnd() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, "Game Over", 7);
      gfx.text(3, 20, `Score:${this.score}`);
      gfx.text(3, 29, `Best:${this.best}`);
      gfx.text(3, 38, "Tap retry Hold menu");
    }
  }

  class MiniLander extends Game {
    constructor() {
      super("Mini Lander");
    }
    reset() {
      this.level = 1;
      this.crashed = false;
      this.loadLevel();
    }
    loadLevel() {
      this.state = "brief";
      this.brief = 0;
      this.briefArmed = false;
      this.alt0 = 90 + (this.level - 1) * 8;
      this.alt = this.alt0;
      this.vel = 3.5 + (this.level - 1) * 1.6;
      this.fuel0 = Math.max(22, 70 - (this.level - 1) * 6);
      this.fuel = this.fuel0;
    }
    gravity() {
      return this.level <= 3 ? 6.5 + (this.level - 1) * 1.4 : 11.5 + (this.level - 4) * 2.1;
    }
    safeSpeed() {
      if (this.level === 1) return 14;
      if (this.level === 2) return 13;
      if (this.level === 3) return 12;
      return Math.max(8, 12 - (this.level - 3));
    }
    burnCue() {
      if (this.level > 3 || this.fuel <= 0 || this.vel <= 1 || this.state !== "fall") return false;
      const thrust = 48;
      const g = this.gravity();
      const lead = 0.35;
      const decel = thrust - g;
      const reaction = this.vel * lead + 0.5 * g * lead * lead;
      const v2 = this.vel + g * lead;
      const stop = (v2 * v2) / (2 * decel);
      const err = this.alt - (reaction + stop);
      return err <= 3 && err >= -2;
    }
    update(dt, input, now) {
      if (this.state === "brief") {
        setLed(false);
        this.brief += dt * 1000;
        if (this.brief < 1000) return;
        if (!this.briefArmed) {
          if (!input.down) this.briefArmed = true;
          return;
        }
        if (input.click) this.state = "fall";
        return;
      }
      if (this.state === "landed") {
        setLed(false);
        this.landTimer += dt * 1000;
        if (this.landTimer > 900) {
          this.level++;
          this.loadLevel();
        }
        return;
      }
      const thrust = input.down && this.fuel > 0;
      const lowFuel = this.fuel < this.fuel0 * 0.2 && Math.floor(now / 180) % 2 === 0;
      setLed((this.burnCue() && !thrust) || lowFuel);
      let accel = this.gravity();
      if (thrust) {
        accel -= 48;
        this.fuel = Math.max(0, this.fuel - 24 * dt);
      }
      this.vel += accel * dt;
      this.alt -= this.vel * dt;
      if (this.alt <= 0) {
        this.alt = 0;
        setLed(false);
        if (this.vel <= this.safeSpeed()) {
          this.vel = 0;
          this.state = "landed";
          this.landTimer = 0;
        } else {
          this.crashed = true;
          this.finish(now);
        }
      }
    }
    drawStart() {
      if (introPage() === 2) {
        drawPromptPage();
        return;
      }
      gfx.rect(0, 0, W, H);
      if (introPage() === 1) {
        gfx.text(3, 10, "Mission", 7);
        gfx.text(3, 24, "Land softly");
        gfx.text(3, 34, "Watch V/Fuel");
        return;
      }
      gfx.circle(17, 20, 12);
      gfx.circle(12, 16, 2);
      gfx.circle(20, 23, 3);
      gfx.tri(52, 14, 57, 11, 55, 18);
      gfx.line(42, 9, 50, 13);
      gfx.text(3, 36, this.title, 7);
    }
    draw() {
      if (this.state === "brief") {
        gfx.rect(0, 0, W, H);
        gfx.text(3, 8, `Level ${this.level}`, 7);
        gfx.text(3, 17, `Fuel ${Math.round(this.fuel0)}  G ${Math.round(this.gravity())}`);
        gfx.text(3, 26, `Safe V <= ${this.safeSpeed()}`);
        gfx.text(3, 37, this.brief >= 1000 && this.briefArmed ? "Tap start" : "Ready...");
        return;
      }
      gfx.rect(1, 0, 70, 40);
      gfx.text(2, 6, `L${this.level}`);
      gfx.text(15, 8, `A${Math.round(this.alt)} V${Math.round(this.vel)}`, 7);
      gfx.rect(67, 10, 3, 24);
      gfx.box(68, 33 - Math.floor(22 * this.fuel / this.fuel0), 1, Math.floor(22 * this.fuel / this.fuel0));
      gfx.line(2, 36, 69, 36);
      gfx.box(28, 35, 18, 2);
      const y = 36 - 5 - (this.alt <= 0 ? 0 : Math.max(1, Math.ceil((this.alt / this.alt0) * 22)));
      gfx.tri(36, y - 4, 33, y + 2, 39, y + 2);
      gfx.line(33, y + 2, 31, y + 5);
      gfx.line(39, y + 2, 41, y + 5);
      if (this.state === "landed") gfx.text(22, 24, "LANDED");
    }
    drawEnd() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, this.crashed ? "Crashed" : "Mission End", 7);
      gfx.text(3, 21, `Level:${this.level}`, 7);
      gfx.text(3, 32, `V:${Math.round(this.vel)}`, 7);
      gfx.circle(57, 25, 8);
      gfx.circle(57, 25, 7);
      gfx.text(this.safeSpeed() >= 10 ? 52 : 55, 28, this.safeSpeed(), 7);
      gfx.text(3, 39, "Tap retry Hold menu");
    }
  }

  class StopwatchSim extends Game {
    constructor() {
      super("Stopwatch");
      this.running = false;
      this.elapsed = 0;
    }
    reset() {
      this.running = false;
      this.elapsed = 0;
    }
    update(dt, input, now) {
      if (input.click) this.running = !this.running;
      // long press resets stopwatch (do not return to menu)
      if (input.longPress) {
        this.running = false;
        this.elapsed = 0;
      }
      if (this.running) this.elapsed += Math.round(dt * 1000);
    }
    draw() {
      gfx.rect(0, 0, W, H);
      const ms = this.elapsed % 1000;
      const totalSec = Math.floor(this.elapsed / 1000);
      const sec = totalSec % 60;
      const min = Math.floor(totalSec / 60);
      const cs = Math.floor(ms / 10);
      const s = `${String(min).padStart(2,'0')}:${String(sec).padStart(2,'0')}.${String(cs).padStart(2,'0')}`;
      gfx.center(22, s);
      // tap hint at top, hold hint at bottom
      gfx.text(3, 9, this.running ? "Tap=pause" : "Tap=start", 4);
      // right-align hold hint
      const hold = "Hold=Reset";
      const holdX = Math.max(3, W - (textWidth(hold) + 2));
      gfx.text(holdX, 36, hold, 4);
    }
    drawStart() { this.draw(); }
  }

  class CounterSim extends Game {
    constructor() {
      super("Counter");
      this.count = 0;
    }
    reset() {
      this.count = 0;
    }
    update(dt, input, now) {
      if (input.click) this.count++;
      if (input.longPress) this.count = 0;
    }
    draw() {
      gfx.rect(0, 0, W, H);
      const s = `${this.count}`;
      gfx.center(24, s);
      gfx.text(3, 9, "Tap=+1", 4);
      const hold = "Hold=Reset";
      const holdX = Math.max(3, W - (textWidth(hold) + 2));
      gfx.text(holdX, 36, hold, 4);
    }
    drawStart() { this.draw(); }
  }

  class MouseJigglerSim extends Game {
    constructor() {
      super("Mouse Emulator");
      this.countdown = 0;
      this.interval = 10000;
      this.last = performance.now();
      this.active = true;
    }
    reset() {
      this.last = performance.now();
      this.active = true;
    }
    update(dt, input, now) {
      if (input.click) this.active = !this.active;
      if (input.longPress) app.toMenu(now);
    }
    draw() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 10, "Mouse Emulator", 7);
      gfx.text(3, 24, this.active ? "ACTIVE" : "PAUSED", 4);
      gfx.text(3, 36, "Tap toggle Hold menu");
    }
    drawStart() { this.draw(); }
  }

  class ReadingSim extends Game {
    constructor() {
      super("Reading");
      this.readings = [
        { ref: "Psalm 23:4", text: "Even though I walk through the valley of the shadow of death, I will fear no evil, for You are with me; Your rod and Your staff, they comfort me." },
        { ref: "John 16:33", text: "I have told you these things so that in Me you may have peace. In the world you will have tribulation. But take courage; I have overcome the world!" },
        { ref: "Isa 41:10", text: "Do not fear, for I am with you; do not be afraid, for I am your God. I will strengthen you; I will surely help you; I will uphold you with My righteous right hand." },
        { ref: "Phil 4:6-7", text: "Be anxious for nothing, but in everything, by prayer and petition, with thanksgiving, present your requests to God. And the peace of God, which surpasses all understanding, will guard your hearts and your minds in Christ Jesus." },
        { ref: "Ps 34:4-5,8", text: "I sought the LORD, and He answered me; He delivered me from all my fears. Those who look to Him are radiant with joy; their faces shall never be ashamed. Taste and see that the LORD is good; blessed is the man who takes refuge in Him!" },
        { ref: "Romans 8:28", text: "And we know that God works all things together for the good of those who love Him, who are called according to His purpose." },
        { ref: "Joshua 1:9", text: "Have I not commanded you to be strong and courageous? Do not be afraid; do not be discouraged, for the LORD your God is with you wherever you go." },
        { ref: "Matt 6:31-34", text: "Therefore do not worry, saying, 'What shall we eat?' or 'What shall we drink?' or 'What shall we wear?' For the Gentiles strive after all these things, and your heavenly Father knows that you need them. But seek first the kingdom of God and His righteousness, and all these things will be added unto you. Therefore do not worry about tomorrow, for tomorrow will worry about itself. Today has enough trouble of its own." },
        { ref: "Prov 3:5-6", text: "Trust in the LORD with all your heart, and lean not on your own understanding; in all your ways acknowledge Him, and He will make your paths straight." },
        { ref: "Romans 15:13", text: "Now may the God of hope fill you with all joy and peace as you believe in Him, so that you may overflow with hope by the power of the Holy Spirit." },
        { ref: "2 Chron 7:14", text: "and if My people who are called by My name humble themselves and pray and seek My face and turn from their wicked ways, then I will hear from heaven, forgive their sin, and heal their land." },
        { ref: "Phil 2:3-4", text: "Do nothing out of selfish ambition or empty pride, but in humility consider others more important than yourselves. Each of you should look not only to your own interests, but also to the interests of others." },
        { ref: "Isa 41:13", text: "For I am the LORD your God, who takes hold of your right hand and tells you: Do not fear, I will help you." },
        { ref: "1 Pet 5:6-7", text: "Humble yourselves, therefore, under God's mighty hand, so that in due time He may exalt you. Cast all your anxiety on Him, because He cares for you." },
        { ref: "Ps 94:18-19", text: "If I say, 'My foot is slipping,' Your loving devotion, O LORD, supports me. When anxiety overwhelms me, Your consolation delights my soul." },
        { ref: "Rev 21:4", text: "'He will wipe away every tear from their eyes,' and there will be no more death or mourning or crying or pain, for the former things have passed away." },
        { ref: "About BSP", text: "All Bible passages quoted from the Berean Standard Bible (BSB). The Berean Bible and Majority Bible texts are officially dedicated to the public domain as of April 30, 2023. All uses are freely permitted. Attribution Notice: The Holy Bible, Berean Standard Bible, BSB is produced in cooperation with Bible Hub, Discovery Bible, OpenBible.com, and the Berean Bible Translation Committee. This text of God's Word has been dedicated to the public domain." }
      ];
      this.selected = 0;
      this.page = 0;
      this.mode = "select";
    }
    begin(now) {
      this.start(now);
    }
    reset() {
      this.selected = 0;
      this.page = 0;
      this.mode = "select";
    }
    lines(text) {
      const words = text.split(/\s+/).filter(Boolean);
      const lines = [];
      let line = "";
      words.forEach((word) => {
        const next = line ? `${line} ${word}` : word;
        if (next.length > 17 && line) {
          lines.push(line);
          line = word;
        } else {
          line = next;
        }
      });
      if (line) lines.push(line);
      return lines.length ? lines : [""];
    }
    hasMoreAfterPage() {
      return this.lines(this.readings[this.selected].text).length > (this.page + 1) * READING_LINES_PER_PAGE;
    }
    update(dt, input, now) {
      if (this.mode === "select") {
        if (input.click) this.selected = (this.selected + 1) % (this.readings.length + 1);
        if (input.longPress) {
          if (this.selected >= this.readings.length) app.toMenu(now);
          else {
            this.page = 0;
            this.mode = "read";
          }
        }
      } else if (this.mode === "read") {
        if (input.longPress) this.mode = "select";
        else if (input.click) {
          if (this.hasMoreAfterPage()) this.page++;
          else this.mode = "end";
        }
      } else {
        if (input.longPress) this.mode = "select";
        if (input.click) {
          this.page = 0;
          this.mode = "read";
        }
      }
    }
    draw() {
      gfx.rect(0, 0, W, H);
      if (this.mode === "select") {
        gfx.text(3, 9, "Reading", 7);
        gfx.text(52, 9, `${this.selected + 1}/${this.readings.length + 1}`);
        if (this.selected >= this.readings.length) {
          gfx.center(23, "Back", 7);
          gfx.text(3, 38, "Hold menu");
        } else {
          gfx.center(23, this.readings[this.selected].ref, 7);
          gfx.text(3, 31, "Tap next");
          gfx.text(3, 38, "Hold open");
        }
        return;
      }
      const reading = this.readings[this.selected];
      if (this.mode === "read") {
        gfx.text(3, 7, reading.ref);
        this.lines(reading.text)
          .slice(this.page * READING_LINES_PER_PAGE, this.page * READING_LINES_PER_PAGE + READING_LINES_PER_PAGE)
          .forEach((line, i) => gfx.text(3, 15 + i * 7, line));
        gfx.text(3, 38, this.hasMoreAfterPage() ? "Tap more" : "Tap END");
      } else {
        gfx.center(17, "END", 7);
        gfx.center(27, reading.ref);
        gfx.text(3, 34, "Tap restart");
        gfx.text(3, 39, "Hold list");
      }
    }
    drawStart() { this.draw(); }
  }

  class CountdownSim extends Game {
    constructor() {
      super("Countdown");
      this.DURATIONS = [10,20,30,60,120,180,240,300,360,420,480,540,600,900,1200,1500,1800,2400,3000,3600];
      this.selected = 0; // default 10s (seconds-first)
      this.duration = this.DURATIONS[this.selected] * 1000;
      this.remaining = this.duration;
      this.mode = 'select'; // 'select'|'running'|'paused'|'finished'
      this.running = false;
      this.finished = false;
      this.flashMs = 0;
      this.flashOn = false;
    }
    reset() {
      this.duration = this.DURATIONS[this.selected] * 1000;
      this.remaining = this.duration;
      this.mode = 'select';
      this.running = false;
      this.finished = false;
      this.flashMs = 0;
      this.flashOn = false;
      setLed(false);
    }
    update(dt, input, now) {
      const ms = Math.round(dt * 1000);
      if (this.mode === 'select') {
        if (input.click) {
          this.selected = (this.selected + 1) % this.DURATIONS.length;
          this.duration = this.DURATIONS[this.selected] * 1000;
          this.remaining = this.duration;
        }
        if (input.longPress) {
          this.mode = 'running';
          this.running = true;
          this.finished = false;
        }
      } else if (this.mode === 'running') {
        if (input.click) {
          this.mode = 'paused';
          this.running = false;
        }
        if (input.longPress) {
          this.remaining = this.duration;
          this.running = true;
          this.mode = 'running';
          this.finished = false;
          this.flashOn = false;
          this.flashMs = 0;
        }
        if (this.running) {
          if (ms >= this.remaining) {
            this.remaining = 0;
            this.finished = true;
            this.running = false;
            this.mode = 'finished';
            this.flashMs = 0;
            this.flashOn = true;
          } else {
            this.remaining -= ms;
          }
        }
      } else if (this.mode === 'paused') {
        if (input.click) {
          this.mode = 'running';
          this.running = true;
        }
        if (input.longPress) {
          // long-press while paused: return to selection screen
          this.remaining = this.duration;
          this.mode = 'select';
          this.running = false;
          this.finished = false;
          this.flashOn = false;
          this.flashMs = 0;
          setLed(false);
        }
      } else if (this.mode === 'finished') {
        if (input.click) {
          this.remaining = this.duration;
          this.finished = false;
          this.mode = 'select';
          setLed(false);
        }
        if (input.longPress) {
          this.remaining = this.duration;
          this.finished = false;
          this.mode = 'select';
          setLed(false);
        }
      }

      if (this.mode === 'finished') {
        this.flashMs += ms;
        if (this.flashMs >= 300) {
          this.flashMs = 0;
          this.flashOn = !this.flashOn;
          setLed(this.flashOn);
        }
      }
    }
    draw() {
      if (this.mode === 'finished' && this.flashOn) {
        gfx.box(0, 0, W, H);
        gfx.text(12, 24, "TIME!", 7);
        return;
      }
      gfx.rect(0, 0, W, H);
      const totalSec = Math.floor(this.remaining / 1000);
      const sec = totalSec % 60;
      const min = Math.floor(totalSec / 60);
      const s = `${String(min).padStart(2,'0')}:${String(sec).padStart(2,'0')}`;
      gfx.center(24, s);
        if (this.mode === 'select') {
          gfx.text(3, 9, "Tap=cycle", 3);
          gfx.text(3, 36, "Hold=start", 3);
        } else if (this.mode === 'running') {
          gfx.text(3, 9, "Tap=pause", 3);
          gfx.text(3, 36, "Hold=restart", 3);
        } else if (this.mode === 'paused') {
          gfx.text(3, 9, "Tap=resume", 3);
          gfx.text(3, 36, "Hold=select", 3);
        } else if (this.mode === 'finished') {
          gfx.text(3, 9, "Tap=reset", 3);
          gfx.text(3, 36, "Hold=select", 3);
        }
    }
    drawStart() { this.draw(); }
  }

  class NeedSpeed extends Game {
    constructor() {
      super("Need Speed");
      this.bestLevel = loadNum("needspeed.level");
      this.bestTime = loadNum("needspeed.time");
      this.bestInit = loadText("needspeed.init", dottedInitials());
    }
    reset() {
      this.level = 1;
      this.totalTime = 0;
      this.dq = false;
      this.failed = false;
      this.startLevel();
    }
    startLevel() {
      this.state = "countdown";
      this.countdown = 3000;
      this.time = 0;
      this.gear = 1;
      this.rpm = 900;
      this.speed = 0;
      this.msg = "";
      this.msgTimer = 0;
      this.clearTimer = 0;
    }
    finishSpeed() {
      return 120 + (this.level - 1) * 28;
    }
    targetMs() {
      return 14500 - (this.level - 1) * 1100;
    }
    rpmMult() {
      return 1 + (this.level - 1) * 0.08;
    }
    speedMult() {
      return 1 + (this.level - 1) * 0.04;
    }
    update(dt, input, now) {
      if (this.msgTimer > 0) this.msgTimer -= dt * 1000; else this.msg = "";
      if (this.state === "countdown") {
        if (input.pressed) {
          this.dq = true;
          this.msg = "Disqualified";
          this.finish(now);
          return;
        }
        this.countdown -= dt * 1000;
        if (this.countdown <= 0) this.state = "launch";
        return;
      }
      if (this.state === "launch") {
        if (input.pressed) {
          this.state = "race";
          this.rpm = 900;
          this.speed = 1;
          this.time = 0;
        }
        return;
      }
      if (this.state === "clear") {
        setLed(false);
        this.clearTimer += dt * 1000;
        if (this.clearTimer >= 1200) {
          this.level++;
          this.startLevel();
        }
        return;
      }
      const climbs = [3300, 2700, 2200, 1750, 1400, 1100];
      const gains = [24, 22, 19, 16, 13, 10];
      const i = this.gear - 1;
      this.time += dt * 1000;
      this.totalTime += dt * 1000;
      this.rpm += climbs[i] * this.rpmMult() * dt;
      if (this.rpm > 7000) {
        this.rpm = 7000;
        this.msg = "Limiter";
        this.msgTimer = 180;
      }
      this.speed += gains[i] * this.speedMult() * (this.rpm > 6500 ? 0.72 : 1) * dt;
      if (input.pressed) this.shift();
      setLed(this.gear < 6 && (this.rpm >= 6500 ? Math.floor(now / 120) % 2 === 0 : this.rpm >= 5600));
      if (this.speed >= this.finishSpeed()) {
        setLed(false);
        this.recordCleared(this.level);
        if (this.level >= 5) this.finish(now);
        else {
          this.state = "clear";
          this.clearTimer = 0;
        }
      } else if (this.gear >= 6 && this.rpm >= 7000) {
        setLed(false);
        this.failed = true;
        this.finish(now);
      } else if (this.time > this.targetMs()) {
        setLed(false);
        this.failed = true;
        this.finish(now);
      }
    }
    shift() {
      if (this.gear >= 6) return;
      if (this.rpm < 5200) this.msg = "Short shift";
      else if (this.rpm >= 5600 && this.rpm <= 6500) {
        this.msg = "Perfect";
        this.speed += 3;
      } else if (this.rpm <= 6500) this.msg = "Good";
      else this.msg = "Late shift";
      this.msgTimer = 450;
      this.gear++;
      this.rpm = Math.max(900, this.rpm * 0.58);
      setLed(false);
    }
    recordCleared(level) {
      if (level > this.bestLevel || (level === this.bestLevel && (!this.bestTime || this.totalTime < this.bestTime))) {
        this.bestLevel = level;
        this.bestTime = this.totalTime;
        this.bestInit = dottedInitials();
        save("needspeed.level", this.bestLevel);
        save("needspeed.time", Math.round(this.bestTime));
        save("needspeed.init", this.bestInit);
      }
    }
    drawStart() {
      if (introPage() === 2) {
        drawPromptPage();
        return;
      }
      gfx.rect(0, 0, W, H);
      if (introPage() === 1) {
        gfx.text(3, 10, "Best Run", 7);
        if (this.bestLevel) {
          gfx.text(3, 23, `${this.bestInit} L${this.bestLevel}`);
          gfx.text(3, 32, `${Math.floor(this.bestTime / 1000)}.${Math.floor(this.bestTime / 100) % 10}s`);
        } else {
          gfx.text(3, 23, "--");
        }
        return;
      }
      this.drawCars();
      gfx.text(3, 9, this.title, 7);
    }
    drawCars() {
      gfx.line(8, 25, 26, 25);
      gfx.line(10, 22, 18, 18);
      gfx.line(18, 18, 25, 22);
      gfx.line(5, 28, 29, 28);
      gfx.circle(10, 29, 2);
      gfx.circle(24, 29, 2);
      gfx.line(41, 27, 62, 27);
      gfx.line(44, 24, 52, 20);
      gfx.line(52, 20, 61, 24);
      gfx.line(37, 30, 65, 30);
      gfx.circle(44, 31, 2);
      gfx.circle(59, 31, 2);
    }
    drawLights() {
      const on = Math.ceil(Math.max(0, this.countdown) / 1000);
      for (let i = 0; i < 3; i++) {
        const x = 23 + i * 10;
        gfx.circle(x, 16, 4);
        if (i < on) gfx.disc(x, 16, 2);
      }
    }
    drawTach() {
      const cx = 36, cy = 27, r = 14;
      const start = 2.35, sweep = 3.25;
      gfx.circle(cx, cy, r);
      const low = start + (5600 / 7000) * sweep;
      const high = start + (6500 / 7000) * sweep;
      for (let a = low; a <= high; a += 0.1) gfx.line(cx + Math.cos(a) * 9, cy + Math.sin(a) * 9, cx + Math.cos(a) * 13, cy + Math.sin(a) * 13);
      for (let i = 0; i <= 7; i++) {
        const a = start + i * sweep / 7;
        gfx.line(cx + Math.cos(a) * 11, cy + Math.sin(a) * 11, cx + Math.cos(a) * r, cy + Math.sin(a) * r);
      }
      const needle = start + Math.min(1, this.rpm / 7000) * sweep;
      gfx.line(cx, cy, cx + Math.cos(needle) * 11, cy + Math.sin(needle) * 11);
      gfx.text(3, 31, `${Math.round(this.rpm / 100)}00`);
    }
    draw() {
      gfx.rect(1, 0, 70, 40);
      if (this.state === "countdown") {
        this.drawLights();
        gfx.text(24, 34, "WAIT", 7);
        gfx.text(2, 6, `L${this.level}`);
        return;
      }
      if (this.state === "launch") {
        gfx.text(25, 24, "GO", 7);
        return;
      }
      if (this.state === "clear") {
        gfx.text(8, 16, "Level Clear", 7);
        gfx.text(20, 28, `Next L${this.level + 1}`);
        return;
      }
      gfx.text(2, 8, `L${this.level} G${this.gear}`, 7);
      gfx.text(24, 8, `${Math.round(this.speed)}k`, 7);
      this.drawTach();
      if (this.msg) gfx.text(3, 38, this.msg);
    }
    drawEnd() {
      setLed(false);
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, this.dq ? "False Start" : this.failed ? "Lost" : "Finished", 7);
      gfx.text(3, 21, `Time:${Math.floor(this.totalTime / 1000)}.${Math.floor(this.totalTime / 100) % 10}`, 7);
      if (this.failed) gfx.text(3, 32, `Target:${Math.round(this.finishSpeed())}k ${Math.floor(this.targetMs() / 1000)}.${Math.floor(this.targetMs() / 100) % 10}`, 7);
      else gfx.text(3, 32, `Level:${this.level}/5`, 7);
      gfx.text(3, 39, "Tap retry Hold menu");
    }
  }

  class NoonShooter extends Game {
    constructor() {
      super("Noon Shooter");
      this.bestReact = loadNum("shooter.react");
      this.bestLevel = loadNum("shooter.level");
    }
    reset() {
      this.level = 1;
      this.tooEarly = false;
      this.lost = false;
      this.startRound();
    }
    enemyDelay() {
      return this.level <= 6 ? 900 - this.level * 100 : Math.max(230, 300 - (this.level - 6) * 12);
    }
    startRound() {
      this.state = "wait";
      this.wait = rand(1000, 3200);
      this.elapsed = 0;
      this.reaction = 0;
      this.anim = 0;
    }
    update(dt, input, now) {
      this.elapsed += Math.min(200, dt * 1000);
      if (this.state === "playerShot" || this.state === "enemyShot") {
        this.anim += dt * 1000;
        if (this.anim > 1050) {
          if (this.state === "playerShot") {
            this.level++;
            if (this.level > this.bestLevel) {
              this.bestLevel = this.level;
              save("shooter.level", this.bestLevel);
            }
            this.startRound();
          } else this.finish(now);
        }
        return;
      }
      if (this.state === "wait") {
        if (input.pressed) {
          this.tooEarly = true;
          this.lost = true;
          this.state = "enemyShot";
          this.anim = 0;
          return;
        }
        if (this.elapsed >= this.wait) {
          this.state = "draw";
          this.elapsed = this.wait;
        }
        return;
      }
      if (input.pressed) {
        this.reaction = this.elapsed - this.wait;
        if (this.reaction < this.enemyDelay()) {
          if (!this.bestReact || this.reaction < this.bestReact) {
            this.bestReact = this.reaction;
            save("shooter.react", this.bestReact);
          }
          this.state = "playerShot";
        } else {
          this.lost = true;
          this.state = "enemyShot";
        }
        this.anim = 0;
      } else if (this.elapsed - this.wait >= this.enemyDelay()) {
        this.reaction = this.enemyDelay();
        this.lost = true;
        this.state = "enemyShot";
        this.anim = 0;
      }
    }
    drawPerson(x, y, right, fallen = false) {
      if (fallen) {
        const d = right ? 1 : -1;
        gfx.circle(x, y - 3, 2);
        gfx.line(x + d * 2, y - 3, x + d * 11, y - 2);
        gfx.line(x + d * 5, y - 2, x + d * 2, y);
        gfx.line(x + d * 7, y - 2, x + d * 11, y);
        return;
      }
      gfx.circle(x, y - 12, 2);
      gfx.line(x, y - 9, x, y - 3);
      gfx.line(x, y - 3, x - 3, y);
      gfx.line(x, y - 3, x + 3, y);
      gfx.line(x, y - 8, x + (right ? 7 : -7), y - 8);
    }
    drawRevolver(playerShot) {
      const cx = 36, cy = 20;
      gfx.circle(cx, cy, 10);
      gfx.circle(cx, cy, 4);
      for (let i = 0; i < 6; i++) gfx.pixel(cx + Math.cos(i * 1.047) * 7, cy + Math.sin(i * 1.047) * 7);
      if (this.anim > 150) {
        const x = playerShot ? 54 : 18;
        gfx.line(cx, cy, x, cy);
        gfx.line(x, cy - 3, x + (playerShot ? 5 : -5), cy);
        gfx.line(x, cy + 3, x + (playerShot ? 5 : -5), cy);
      }
      if (this.anim > 320) gfx.center(38, "BANG", 7);
    }
    draw() {
      gfx.rect(1, 0, 70, 40);
      if ((this.state === "playerShot" || this.state === "enemyShot") && this.anim < 480) {
        this.drawRevolver(this.state === "playerShot");
        return;
      }
      gfx.line(2, 31, 69, 31);
      this.drawPerson(13, 28, true, this.state === "enemyShot" && this.anim >= 480);
      this.drawPerson(57, 28, false, this.state === "playerShot" && this.anim >= 480);
      gfx.center(12, this.state === "wait" ? "WAIT" : this.state === "draw" ? "DRAW!" : this.state === "playerShot" ? "HIT" : "SHOT", 7);
      gfx.text(2, 38, `L${this.level} enemy ${this.enemyDelay()}`);
    }
    drawStart() {
      if (introPage() === 2) {
        drawPromptPage();
        return;
      }
      gfx.rect(0, 0, W, H);
      if (introPage() === 1) {
        gfx.text(3, 10, "Top Duel", 7);
        if (this.bestLevel) {
          gfx.text(3, 23, `${dottedInitials()} L${this.bestLevel}`);
          gfx.text(3, 32, this.bestReact ? `${Math.round(this.bestReact)}ms` : "--");
        } else {
          gfx.text(3, 23, "--");
        }
        return;
      }
      gfx.circle(56, 8, 5);
      gfx.line(4, 31, 67, 31);
      this.drawPerson(18, 28, true);
      this.drawPerson(46, 28, false);
      gfx.text(3, 10, this.title, 7);
    }
    drawEnd() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, this.tooEarly ? "Too Early" : "You Lost", 7);
      gfx.text(3, 20, `React ${this.reaction ? `${Math.round(this.reaction)}ms` : "--"}`);
      gfx.text(3, 29, `Best L${this.bestLevel} ${this.bestReact ? `${Math.round(this.bestReact)}ms` : "--"}`);
      gfx.text(3, 38, "Tap retry Hold menu");
    }
  }

  class FishingFlick extends Game {
    constructor() {
      super("Fishing Flick");
      this.best = loadNum("fish.best");
    }
    reset() {
      this.level = 1;
      this.weight = 0;
      this.loadLevel();
    }
    loadLevel() {
      this.state = "wait";
      this.wait = rand(700, 1900);
      this.elapsed = 0;
      this.hookTimer = 0;
      this.tension = 34;
      this.progress = 0;
      this.fighting = false;
      this.fight = rand(500, 1000);
      this.catchTimer = 0;
      this.fishWeight = 180 + this.level * 120 + rand(0, 140 + this.level * 25);
    }
    difficulty() {
      return Math.min(3.3, 0.75 + (this.level - 1) * 0.22);
    }
    fightBurst() {
      this.tension = Math.min(110, this.tension + 7 + this.difficulty() * 3.5);
    }
    update(dt, input, now) {
      if (this.state === "wait") {
        this.elapsed += dt * 1000;
        setLed(false);
        if (this.elapsed > this.wait) {
          this.state = "hook";
          this.hookTimer = 0;
        }
        return;
      }
      if (this.state === "hook") {
        this.hookTimer += dt * 1000;
        setLed(false);
        if (input.pressed) {
          this.state = "reel";
          this.tension = 36;
          this.progress = 6;
          this.fighting = false;
          this.fight = 650;
          return;
        }
        if (this.hookTimer >= 950) {
          this.state = "escaped";
          this.finish(now);
        }
        return;
      }
      if (this.state === "caught") {
        this.catchTimer += dt * 1000;
        setLed(false);
        if (this.catchTimer > 1100) {
          this.level++;
          this.loadLevel();
        }
        return;
      }
      if (this.state !== "reel") return;
      this.fight -= dt * 1000;
      if (this.fight <= 0) {
        this.fighting = !this.fighting;
        if (this.fighting) this.fightBurst();
        this.fight = rand(this.fighting ? 360 : 550, this.fighting ? 760 : 1200);
      }
      const hard = this.difficulty();
      if (input.down) {
        this.progress += (this.fighting ? 4 / hard : 32 / hard) * dt;
        this.tension += (this.fighting ? 42 + hard * 12 : 7 + hard * 2) * dt;
      } else {
        this.progress -= (this.fighting ? 2 : 0.8) * dt;
        this.tension -= (this.fighting ? 74 + hard * 7 : 42 + hard * 4) * dt;
      }
      this.tension = clamp(this.tension, 0, 110);
      this.progress = clamp(this.progress, 0, 100);
      setLed(this.tension >= 80 && Math.floor(now / 120) % 2 === 0);
      if (this.tension <= 0) {
        this.state = "escaped";
        setLed(false);
        this.finish(now);
        return;
      }
      if (this.tension >= 100) {
        this.state = "broken";
        setLed(false);
        this.finish(now);
      }
      if (this.progress >= 100) {
        this.state = "caught";
        this.catchTimer = 0;
        this.weight = this.fishWeight;
        if (this.weight > this.best) {
          this.best = this.weight;
          save("fish.best", this.best);
        }
        setLed(false);
      }
    }
    water() {
      gfx.line(2, 29, 69, 29);
      for (let i = 0; i < 5; i++) {
        const x = 4 + i * 14;
        gfx.line(x, 33, x + 5, 31);
        gfx.line(x + 5, 31, x + 10, 33);
      }
    }
    drawStart() {
      if (introPage() === 2) {
        drawPromptPage();
        return;
      }
      gfx.rect(0, 0, W, H);
      if (introPage() === 1) {
        gfx.text(3, 10, "Top Fish", 7);
        gfx.text(3, 24, this.best ? `${dottedInitials()} ${this.best}g` : "--");
        return;
      }
      this.water();
      gfx.line(12, 9, 24, 25);
      gfx.line(24, 25, 31, 24);
      gfx.text(3, 10, this.title, 7);
    }
    draw() {
      gfx.rect(1, 0, 70, 40);
      this.water();
      if (this.state === "wait") {
        gfx.text(3, 7, `L${this.level} ${this.fishWeight}g`);
        gfx.text(18, 13, "Waiting");
        gfx.line(34, 12, 34, 24);
        gfx.pixel(34, 25);
        return;
      }
      if (this.state === "hook") {
        gfx.text(3, 7, `L${this.level} ${this.fishWeight}g`);
        gfx.text(18, 13, "TUG!");
        const tug = Math.floor(performance.now() / 120) % 2 === 0 ? 3 : -3;
        gfx.line(34, 12, 34 + tug, 24);
        gfx.pixel(34 + tug, 25);
        gfx.text(18, 36, "Tap!");
        return;
      }
      if (this.state === "caught") {
        gfx.text(14, 17, "Caught!", 7);
        gfx.text(18, 28, `${this.weight}g`);
        return;
      }
      gfx.text(3, 7, `L${this.level} ${this.fighting ? "FIGHT" : "REEL"}`);
      gfx.rect(5, 13, 58, 5);
      gfx.box(6, 14, this.progress * 0.56, 3);
      gfx.text(5, 27, "T");
      gfx.rect(14, 22, 49, 5);
      gfx.box(15, 23, Math.min(47, this.tension * 0.47), 3);
      if (this.tension > 78) gfx.text(22, 36, "LINE!");
      else if (this.tension < 15) gfx.text(22, 36, "SLACK!");
    }
    drawEnd() {
      setLed(false);
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, this.state === "escaped" ? "Escaped" : "Line broke", 7);
      gfx.text(3, 20, `Fish ${this.weight ? `${this.weight}g` : "--"}`);
      gfx.text(3, 29, `Best ${this.best ? `${this.best}g` : "--"}`);
      gfx.text(3, 38, "Tap retry Hold menu");
    }
  }

  class TinyGolf extends Game {
    constructor() {
      super("Tiny Golf");
      this.best = loadNum("golf.best");
    }
    reset() {
      this.hole = 0;
      this.total = 0;
      this.loadHole();
    }
    holes() {
      return [
        { s: [8, 32], c: [59, 16], w: [[27, 12, 4, 20], [43, 8, 4, 18]] },
        { s: [10, 15], c: [58, 31], w: [[20, 22, 32, 3], [36, 10, 3, 12]] },
        { s: [9, 11], c: [61, 11], w: [[21, 10, 3, 18], [34, 17, 3, 21], [51, 8, 3, 20]] }
      ];
    }
    loadHole() {
      const h = this.holes()[this.hole % this.holes().length];
      this.ball = { x: h.s[0], y: h.s[1], vx: 0, vy: 0 };
      this.aim = 0;
      this.power = 0.25;
      this.pdir = 1;
      this.state = "aim";
    }
    update(dt, input, now) {
      const h = this.holes()[this.hole % this.holes().length];
      if (this.state === "aim") {
        this.aim = (this.aim + 1.9 * dt) % (Math.PI * 2);
        if (input.click) this.state = "power";
      } else if (this.state === "power") {
        this.power += this.pdir * 1.4 * dt;
        if (this.power > 1 || this.power < 0.2) {
          this.power = clamp(this.power, 0.2, 1);
          this.pdir *= -1;
        }
        if (input.click) {
          const s = 16 + 70 * this.power;
          this.ball.vx = Math.cos(this.aim) * s;
          this.ball.vy = Math.sin(this.aim) * s;
          this.total++;
          this.state = "roll";
        }
      } else if (this.state === "roll") {
        this.ball.x += this.ball.vx * dt;
        this.ball.y += this.ball.vy * dt;
        if (this.ball.x < 2 || this.ball.x > 68) this.ball.vx *= -0.88;
        if (this.ball.y < 9 || this.ball.y > 37) this.ball.vy *= -0.88;
        this.ball.x = clamp(this.ball.x, 2, 68);
        this.ball.y = clamp(this.ball.y, 9, 37);
        for (const w of h.w) {
          if (this.ball.x >= w[0] - 1 && this.ball.x <= w[0] + w[2] + 1 && this.ball.y >= w[1] - 1 && this.ball.y <= w[1] + w[3] + 1) {
            if (Math.abs(this.ball.vx) > Math.abs(this.ball.vy)) this.ball.vx *= -0.88; else this.ball.vy *= -0.88;
          }
        }
        let sp = Math.hypot(this.ball.vx, this.ball.vy);
        const dx = this.ball.x - h.c[0], dy = this.ball.y - h.c[1];
        if (dx * dx + dy * dy < 13 && sp < 34) {
          this.state = "holed";
          this.holed = 0;
          return;
        }
        sp = Math.max(0, sp - 24 * dt);
        if (sp < 4) {
          this.ball.vx = this.ball.vy = 0;
          this.state = "aim";
        } else {
          const a = Math.atan2(this.ball.vy, this.ball.vx);
          this.ball.vx = Math.cos(a) * sp;
          this.ball.vy = Math.sin(a) * sp;
        }
      } else {
        this.holed += dt * 1000;
        if (this.holed > 900) {
          this.hole++;
          if (this.hole >= this.holes().length) {
            if (!this.best || this.total < this.best) {
              this.best = this.total;
              save("golf.best", this.best);
            }
            this.finish(now);
          } else this.loadHole();
        }
      }
    }
    drawStart() {
      if (introPage() === 2) {
        drawPromptPage();
        return;
      }
      gfx.rect(0, 0, W, H);
      if (introPage() === 1) {
        gfx.text(3, 10, "Top Score", 7);
        gfx.text(3, 24, this.best ? `${dottedInitials()} ${this.best}` : "--");
      } else {
        gfx.text(3, 10, this.title, 7);
        gfx.text(3, 34, "Low score wins");
      }
    }
    draw() {
      const h = this.holes()[this.hole % this.holes().length];
      gfx.rect(1, 0, 70, 40);
      gfx.line(2, 7, 69, 7);
      gfx.text(3, 6, `H${this.hole + 1} S${this.total}`);
      gfx.circle(h.c[0] + 1, h.c[1], 2);
      for (const w of h.w) gfx.box(w[0] + 1, w[1], w[2], w[3]);
      gfx.disc(this.ball.x + 1, this.ball.y, 1);
      if (this.state === "aim" || this.state === "power") gfx.line(this.ball.x + 1, this.ball.y, this.ball.x + 1 + Math.cos(this.aim) * 8, this.ball.y + Math.sin(this.aim) * 8);
      if (this.state === "power") {
        gfx.rect(46, 2, 20, 4);
        gfx.box(47, 3, 18 * this.power, 2);
      }
    }
    drawEnd() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, "Course Done", 7);
      gfx.text(3, 20, `Score:${this.total}`);
      gfx.text(3, 29, `Best:${this.best || "--"}`);
      gfx.text(3, 38, "Tap retry Hold menu");
    }
  }

  class MazeRunner extends Game {
    constructor(collector = false) {
      super(collector ? "Maze Collector" : "Maze Runner");
      this.collector = collector;
      this.scoreKey = collector ? "mazecol.best" : "maze.best";
      this.best = loadNum(this.scoreKey);
      this.cols = 9;
      this.rows = 5;
      this.cellCount = this.cols * this.rows;
    }
    reset() {
      this.level = 1;
      this.nextLevel();
    }
    nextLevel() {
      this.generateMaze();
      this.placePickups();
      this.runnerCell = 0;
      this.dir = 1;
      this.state = "choosing";
      this.stepTimer = 0;
      this.timeLeft = this.collector ? Math.max(18000, 34000 - this.level * 750) : Math.max(15000, 28000 - this.level * 750);
      this.collectOptions();
      if (this.level > this.best) {
        this.best = this.level;
        save(this.scoreKey, this.best);
        save(`${this.scoreKey}.init`, dottedInitials());
      }
    }
    generateMaze() {
      this.walls = Array(this.cellCount).fill(1 | 2 | 4 | 8);
      const visited = Array(this.cellCount).fill(false);
      const stack = [0];
      visited[0] = true;
      while (stack.length) {
        const cell = stack[stack.length - 1];
        const options = [];
        for (let d = 0; d < 4; d++) {
          const n = this.neighbor(cell, d);
          if (n < this.cellCount && !visited[n]) options.push(d);
        }
        if (!options.length) {
          stack.pop();
          continue;
        }
        const d = options[rand(0, options.length)];
        const n = this.neighbor(cell, d);
        this.removeWall(cell, d);
        visited[n] = true;
        stack.push(n);
      }
    }
    placePickups() {
      this.pickups = [];
      this.collected = new Set();
      if (!this.collector) return;
      const count = Math.min(3, 1 + (this.level >= 3 ? 1 : 0) + (this.level >= 6 ? 1 : 0));
      while (this.pickups.length < count) {
        const cell = rand(1, this.cellCount - 1);
        if (!this.pickups.includes(cell)) this.pickups.push(cell);
      }
    }
    update(dt, input, now) {
      const deltaMs = dt * 1000;
      this.timeLeft -= deltaMs;
      if (this.timeLeft <= 0) {
        this.finish(now);
        return;
      }
      if (this.state === "choosing") {
        if (!this.options.length) this.collectOptions();
        if (!this.options.length) {
          this.finish(now);
          return;
        }
        if (input.click && this.options.length) this.selected = (this.selected + 1) % this.options.length;
        if ((input.down && input.holdMs > 300) || input.longPress) this.chooseDirection();
        return;
      }
      this.stepTimer += deltaMs;
      const stepDelay = Math.max(180, 430 - this.level * 18);
      if (this.stepTimer >= stepDelay) {
        this.stepTimer = 0;
        this.stepRunner();
      }
    }
    collectOptions() {
      this.options = [];
      const back = this.reverse(this.dir);
      for (let d = 0; d < 4; d++) {
        if (d !== back && this.canMove(this.runnerCell, d)) this.options.push(d);
      }
      if (!this.options.length && this.canMove(this.runnerCell, back)) this.options.push(back);
      this.selected = 0;
    }
    chooseDirection() {
      if (!this.options.length) this.collectOptions();
      if (!this.options.length) return;
      this.dir = this.options[this.selected];
      this.state = "moving";
      this.stepTimer = 0;
    }
    stepRunner() {
      if (!this.canMove(this.runnerCell, this.dir)) {
        this.state = "choosing";
        this.collectOptions();
        return;
      }
      this.runnerCell = this.neighbor(this.runnerCell, this.dir);
      if (this.collector && this.pickups.includes(this.runnerCell)) this.collected.add(this.runnerCell);
      if (this.runnerCell === this.cellCount - 1 && this.exitUnlocked()) {
        this.level++;
        this.nextLevel();
        return;
      }
      if (this.shouldStopForChoice()) {
        this.state = "choosing";
        this.collectOptions();
      } else {
        this.collectOptions();
        if (this.options.length === 1) this.dir = this.options[0];
      }
    }
    exitUnlocked() {
      return !this.collector || this.collected.size === this.pickups.length;
    }
    shouldStopForChoice() {
      const back = this.reverse(this.dir);
      let count = 0;
      for (let d = 0; d < 4; d++) {
        if (d !== back && this.canMove(this.runnerCell, d)) count++;
      }
      return count !== 1;
    }
    canMove(cell, dir) {
      return (this.walls[cell] & (1 << dir)) === 0;
    }
    reverse(dir) {
      return (dir + 2) % 4;
    }
    neighbor(cell, dir) {
      const c = cell % this.cols;
      const r = Math.floor(cell / this.cols);
      if (dir === 0 && r > 0) return cell - this.cols;
      if (dir === 1 && c < this.cols - 1) return cell + 1;
      if (dir === 2 && r < this.rows - 1) return cell + this.cols;
      if (dir === 3 && c > 0) return cell - 1;
      return this.cellCount;
    }
    removeWall(cell, dir) {
      const next = this.neighbor(cell, dir);
      if (next >= this.cellCount) return;
      this.walls[cell] &= ~(1 << dir);
      this.walls[next] &= ~(1 << this.reverse(dir));
    }
    drawStart() {
      if (introPage() === 2) {
        drawPromptPage();
        return;
      }
      gfx.rect(0, 0, W, H);
      if (introPage() === 1) {
        gfx.text(3, 10, "Top Score", 7);
        gfx.text(3, 24, this.best ? `${dottedInitials()} L${this.best}` : "--");
      } else {
        gfx.text(3, 10, this.title, 7);
        gfx.text(3, 21, this.collector ? "Get keys" : "Tap choose");
        gfx.text(3, 29, "Hold run");
      }
    }
    draw() {
      gfx.rect(0, 0, W, H);
      const keysLeft = this.collector ? ` K${this.pickups.length - this.collected.size}` : "";
      gfx.text(2, 6, `L${this.level} T${Math.max(0, Math.ceil(this.timeLeft / 1000))}${keysLeft}`);
      const gx = 3, gy = 7, cw = 7, ch = 6;
      for (let r = 0; r < this.rows; r++) {
        for (let c = 0; c < this.cols; c++) {
          const cell = r * this.cols + c;
          const x = gx + c * cw;
          const y = gy + r * ch;
          if (this.walls[cell] & 1) gfx.line(x, y, x + cw, y);
          if (this.walls[cell] & 2) gfx.line(x + cw, y, x + cw, y + ch);
          if (this.walls[cell] & 4) gfx.line(x, y + ch, x + cw, y + ch);
          if (this.walls[cell] & 8) gfx.line(x, y, x, y + ch);
        }
      }
      const c = this.runnerCell % this.cols;
      const r = Math.floor(this.runnerCell / this.cols);
      const rx = gx + c * cw + 3;
      const ry = gy + r * ch + 3;
      if (this.collector) {
        for (const p of this.pickups) {
          if (this.collected.has(p)) continue;
          const pc = p % this.cols;
          const pr = Math.floor(p / this.cols);
          gfx.circle(gx + pc * cw + 3, gy + pr * ch + 3, 2);
        }
      }
      gfx.box(rx, ry - 1, 2, 2);
      if (this.state === "choosing" && this.options.length) {
        const d = this.options[this.selected];
        const dx = d === 1 ? 5 : d === 3 ? -5 : 0;
        const dy = d === 2 ? 4 : d === 0 ? -4 : 0;
        gfx.line(rx, ry, rx + dx, ry + dy);
      }
      const ex = gx + (this.cols - 1) * cw + 2;
      const ey = gy + (this.rows - 1) * ch + 1;
      gfx.rect(ex, ey, 4, 4);
      if (!this.exitUnlocked()) {
        gfx.line(ex, ey, ex + 3, ey + 3);
        gfx.line(ex + 3, ey, ex, ey + 3);
      }
    }
    drawEnd() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, "Lost", 7);
      gfx.text(3, 20, `Level ${this.level}`);
      gfx.text(3, 29, `Best L${this.best}`);
      gfx.text(3, 38, "Tap retry Hold menu");
    }
  }

  class PipeMania extends Game {
    constructor() {
      super("Pipe Mania");
      this.best = loadNum("pipe.best");
      this.cols = 7;
      this.rows = 5;
      this.cellCount = this.cols * this.rows;
    }
    reset() {
      this.level = 1;
      this.score = 0;
      this.won = false;
      this.loadLevel();
    }
    loadLevel() {
      this.grid = Array(this.cellCount).fill(0);
      this.filled = Array(this.cellCount).fill(false);
      this.state = "build";
      this.current = rand(1, 7);
      this.flowCell = Math.floor(this.rows / 2) * this.cols;
      this.buildEnd = this.flowCell;
      this.buildDir = 1;
      this.flowDir = 1;
      this.flowed = 1;
      this.target = Math.min(20, 6 + this.level);
      this.buildTimer = 0;
      this.flowTimer = 0;
      this.completeTimer = 0;
      this.placedThisHold = false;
      this.grid[this.flowCell] = 1;
      this.filled[this.flowCell] = true;
      this.updateCursor();
    }
    update(dt, input, now) {
      const deltaMs = dt * 1000;
      if (input.released) this.placedThisHold = false;
      if (this.state === "complete") {
        this.completeTimer += deltaMs;
        if (this.completeTimer >= 900) {
          this.level++;
          this.loadLevel();
        }
        return;
      }
      if (this.state === "build") {
        this.buildTimer += deltaMs;
        if (input.click) this.cyclePiece();
        if (input.down && input.holdMs > 260 && !this.placedThisHold) {
          this.placePiece();
          this.placedThisHold = true;
        }
        if (this.buildTimer >= this.buildDelay()) {
          this.state = "flow";
          this.flowTimer = 0;
        }
      }
      if (this.state === "flow") {
        this.flowTimer += deltaMs;
        const flowDelay = this.flowDelay();
        if (this.flowTimer >= flowDelay) {
          this.flowTimer = 0;
          this.advanceFlow(now);
        }
      }
    }
    buildDelay() {
      return Math.max(5500, 12500 - this.level * 550);
    }
    flowDelay() {
      return Math.max(500, 1500 - this.level * 70);
    }
    cyclePiece() {
      this.current++;
      if (this.current > 6) this.current = 1;
    }
    placePiece() {
      if (this.cursor >= this.cellCount || this.cursor === this.flowCell || this.filled[this.cursor] || this.grid[this.cursor]) return;
      const enteredFrom = this.opposite(this.buildDir);
      if (!this.hasOpening(this.current, enteredFrom)) return;
      this.grid[this.cursor] = this.current;
      this.buildEnd = this.cursor;
      this.buildDir = this.nextDirection(this.current, enteredFrom);
      this.updateCursor();
      this.current = rand(1, 7);
    }
    updateCursor() {
      const next = this.stepCell(this.buildEnd, this.buildDir);
      this.cursor = next < this.cellCount && !this.grid[next] && !this.filled[next] ? next : this.cellCount;
    }
    advanceFlow(now) {
      if (!this.hasOpening(this.grid[this.flowCell], this.flowDir)) {
        this.finishLevel(now);
        return;
      }
      const next = this.stepCell(this.flowCell, this.flowDir);
      if (next >= this.cellCount) {
        this.finishLevel(now);
        return;
      }
      const enteredFrom = this.opposite(this.flowDir);
      if (!this.grid[next] || !this.hasOpening(this.grid[next], enteredFrom)) {
        this.finishLevel(now);
        return;
      }
      this.flowCell = next;
      this.filled[this.flowCell] = true;
      this.flowed++;
      this.score += 5;
      this.flowDir = this.nextDirection(this.grid[this.flowCell], enteredFrom);
    }
    finishLevel(now) {
      if (this.flowed >= this.target) {
        this.state = "complete";
        this.completeTimer = 0;
        this.score += this.flowed;
        this.saveBest();
      } else {
        this.won = false;
        this.saveBest();
        this.finish(now);
      }
    }
    saveBest() {
      if (this.score > this.best) {
        this.best = this.score;
        save("pipe.best", this.best);
      }
    }
    openings(pipe) {
      if (pipe === 1) return 8 | 2;
      if (pipe === 2) return 1 | 4;
      if (pipe === 3) return 1 | 2;
      if (pipe === 4) return 2 | 4;
      if (pipe === 5) return 4 | 8;
      if (pipe === 6) return 8 | 1;
      return 0;
    }
    hasOpening(pipe, dir) {
      return (this.openings(pipe) & (1 << dir)) !== 0;
    }
    opposite(dir) {
      return (dir + 2) % 4;
    }
    nextDirection(pipe, enteredFrom) {
      const open = this.openings(pipe) & ~(1 << enteredFrom);
      for (let d = 0; d < 4; d++) {
        if (open & (1 << d)) return d;
      }
      return enteredFrom;
    }
    stepCell(cell, dir) {
      const c = cell % this.cols;
      const r = Math.floor(cell / this.cols);
      if (dir === 0 && r > 0) return cell - this.cols;
      if (dir === 1 && c < this.cols - 1) return cell + 1;
      if (dir === 2 && r < this.rows - 1) return cell + this.cols;
      if (dir === 3 && c > 0) return cell - 1;
      return this.cellCount;
    }
    drawPipe(x, y, pipe, filled) {
      const cx = x + 3, cy = y + 3;
      if (!pipe) return;
      const oldFill = ctx.fillStyle;
      const oldStroke = ctx.strokeStyle;
      if (filled) {
        gfx.box(x + 1, y + 1, 4, 4);
        ctx.fillStyle = "#000";
        ctx.strokeStyle = "#000";
      }
      if (this.hasOpening(pipe, 0)) gfx.line(cx, cy, cx, y);
      if (this.hasOpening(pipe, 1)) gfx.line(cx, cy, x + 5, cy);
      if (this.hasOpening(pipe, 2)) gfx.line(cx, cy, cx, y + 5);
      if (this.hasOpening(pipe, 3)) gfx.line(cx, cy, x, cy);
      if (filled) {
        gfx.disc(cx, cy, 1);
        ctx.fillStyle = oldFill;
        ctx.strokeStyle = oldStroke;
      }
    }
    drawStart() {
      if (introPage() === 2) {
        drawPromptPage();
        return;
      }
      gfx.rect(0, 0, W, H);
      if (introPage() === 1) {
        gfx.text(3, 9, "Top Score", 7);
        gfx.text(3, 24, this.best ? `${dottedInitials()} ${this.best}` : "--");
      } else {
        gfx.text(3, 9, this.title, 7);
        this.drawPipe(8, 16, 1, true);
        this.drawPipe(14, 16, 4, true);
        this.drawPipe(14, 22, 2, true);
        gfx.text(30, 20, "Tap piece");
        gfx.text(30, 29, "Hold place");
      }
    }
    draw() {
      gfx.rect(0, 0, W, H);
      gfx.text(2, 6, `L${this.level} ${this.flowed}/${this.target}`);
      const gx = 2, gy = 8, cell = 6;
      for (let i = 0; i < this.cellCount; i++) {
        const c = i % this.cols;
        const r = Math.floor(i / this.cols);
        const x = gx + c * cell;
        const y = gy + r * cell;
        gfx.rect(x, y, cell, cell);
        this.drawPipe(x, y, this.grid[i], this.filled[i]);
        if (i === this.cursor && this.state === "build") {
          if (Math.floor(performance.now() / 180) % 2 === 0) gfx.rect(x - 2, y - 2, cell + 4, cell + 4);
          this.drawPipe(x, y, this.current, false);
        }
      }
      gfx.text(48, 13, "Next");
      this.drawPipe(54, 17, this.current, false);
      const wait = Math.max(0, Math.ceil((this.buildDelay() - this.buildTimer) / 1000));
      gfx.text(43, 36, this.state === "build" ? (this.cursor >= this.cellCount ? "Blocked" : `T${wait}`) : this.state === "complete" ? "Clear" : "Goo");
    }
    drawEnd() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, "Pipe End", 7);
      gfx.text(3, 20, `Score ${this.score}`);
      gfx.text(3, 29, `Best ${this.best}`);
      gfx.text(3, 38, "Tap retry Hold menu");
    }
  }

  class Blackjack extends Game {
    constructor() {
      super("Blackjack");
      this.best = loadNum("blackjack.best", 100);
    }
    reset() {
      this.bank = 100;
      this.bet = 10;
      this.shuffleDeck();
      this.beginBetting();
    }
    shuffleDeck() {
      this.deck = Array.from({ length: 104 }, (_, i) => (i % 13) + 1);
      for (let i = this.deck.length - 1; i > 0; i--) {
        const j = rand(0, i + 1);
        [this.deck[i], this.deck[j]] = [this.deck[j], this.deck[i]];
      }
    }
    card() {
      if (!this.deck.length) this.shuffleDeck();
      return this.deck.pop();
    }
    value(cards) {
      let total = 0, aces = 0;
      for (const c of cards) {
        if (c === 1) {
          total += 11;
          aces++;
        } else total += c > 10 ? 10 : c;
      }
      while (total > 21 && aces) {
        total -= 10;
        aces--;
      }
      return total;
    }
    beginBetting() {
      this.state = "bet";
      this.result = "";
      this.player = [];
      this.dealer = [];
      this.timer = 0;
      this.dealStep = 0;
      if (this.bank < this.bet) this.bet = Math.max(1, Math.min(10, this.bank));
    }
    update(dt, input, now) {
      const deltaMs = dt * 1000;
      if (this.state === "shuffle") {
        this.timer += deltaMs;
        if (this.timer > 1000) {
          this.shuffleDeck();
          this.beginBetting();
        }
        return;
      }
      if (this.state === "bet") {
        if (this.bank <= 0) this.finish(now);
        else if (input.click) this.cycleBet();
        else if (input.longPress) this.startDeal();
        return;
      }
      if (this.state === "deal") {
        this.timer += deltaMs;
        if (this.timer >= 300) {
          this.timer = 0;
          if (this.dealStep === 0) this.player.push(this.card());
          else if (this.dealStep === 1) this.dealer.push(this.card());
          else if (this.dealStep === 2) this.player.push(this.card());
          else if (this.dealStep === 3) this.dealer.push(this.card());
          this.dealStep++;
          if (this.dealStep >= 4) this.state = this.value(this.player) === 21 ? "reveal" : "player";
        }
        return;
      }
      if (this.state === "player") {
        if (input.longPress) {
          this.state = "reveal";
          this.timer = 0;
        }
        else if (input.click) {
          this.player.push(this.card());
          if (this.value(this.player) > 21) {
            this.bank -= this.bet;
            this.result = "Bust";
            this.roundOver();
          }
        }
        return;
      }
      if (this.state === "reveal" || this.state === "dealer") {
        this.timer += deltaMs;
        if (this.timer >= 500) {
          this.timer = 0;
          if (this.state === "reveal") {
            this.state = "dealer";
          } else if (this.value(this.dealer) < 17 && this.dealer.length < 7) {
            this.dealer.push(this.card());
          } else {
            this.stand();
          }
        }
        return;
      }
      if (input.longPress || this.bank <= 0) this.finish(now);
      else if (input.click) {
        if (this.deck.length <= 10) {
          this.state = "shuffle";
          this.timer = 0;
        } else {
          this.beginBetting();
        }
      }
    }
    cycleBet() {
      for (const option of [10, 20, 30, 50]) {
        if (option > this.bet && option <= this.bank) {
          this.bet = option;
          return;
        }
      }
      this.bet = this.bank >= 10 ? 10 : this.bank;
    }
    startDeal() {
      if (this.deck.length <= 10) {
        this.state = "shuffle";
        this.timer = 0;
        return;
      }
      this.player = [];
      this.dealer = [];
      this.dealStep = 0;
      this.timer = 300;
      this.state = "deal";
    }
    stand() {
      const p = this.value(this.player), d = this.value(this.dealer);
      if (d > 21 || p > d) {
        this.bank += p === 21 && this.player.length === 2 ? this.bet + Math.floor(this.bet / 2) : this.bet;
        this.result = p === 21 && this.player.length === 2 ? "Blackjack" : "Win";
      } else if (p === d) this.result = "Push";
      else {
        this.bank -= this.bet;
        this.result = "Lose";
      }
      this.roundOver();
    }
    roundOver() {
      this.state = "over";
      if (this.bank > this.best) {
        this.best = this.bank;
        save("blackjack.best", this.best);
        save("blackjack.best.init", dottedInitials());
      }
    }
    label(c) {
      return c === 1 ? "A" : c === 10 ? "T" : c === 11 ? "J" : c === 12 ? "Q" : c === 13 ? "K" : String(c);
    }
    drawCards(x, y, cards, hide) {
      cards.slice(0, 6).forEach((c, i) => {
        gfx.rect(x + i * 8, y - 6, 7, 8);
        gfx.text(x + i * 8 + 2, y, hide && i === 0 ? "?" : this.label(c));
      });
      if (!hide) gfx.text(x + 48, y, this.value(cards));
    }
    drawStart() {
      if (introPage() === 2) {
        drawPromptPage();
        return;
      }
      gfx.rect(0, 0, W, H);
      if (introPage() === 1) {
        gfx.text(3, 9, "Top Bank", 7);
        gfx.text(3, 24, this.best > 100 ? `${loadText("blackjack.best.init", dottedInitials())} $${this.best}` : "--");
      } else {
        gfx.rect(8, 16, 10, 14);
        gfx.rect(21, 14, 10, 14);
        gfx.disc(50, 24, 5);
        gfx.circle(58, 25, 5);
        gfx.text(3, 9, this.title, 7);
        gfx.text(14, 38, "2 deck shoe");
      }
    }
    draw() {
      gfx.rect(1, 0, 70, 40);
      gfx.text(2, 6, `$${this.bank}`);
      gfx.text(38, 6, `Bet ${this.bet}`);
      if (this.state === "shuffle") {
        gfx.text(12, 20, "Shuffle", 7);
        gfx.text(8, 32, "2 deck shoe");
        return;
      }
      if (this.state === "bet") {
        gfx.text(17, 18, "Bet", 7);
        gfx.text(38, 18, this.bet, 7);
        gfx.text(2, 29, `${this.deck.length} cards`);
        gfx.text(2, 38, "Tap bet Hold deal");
        return;
      }
      gfx.text(2, 15, "D");
      this.drawCards(10, 15, this.dealer, this.state === "player" || this.state === "deal");
      gfx.text(2, 27, "P");
      this.drawCards(10, 27, this.player, false);
      const footer = this.state === "deal" ? "Dealing"
        : this.state === "player" ? "Tap hit Hold stay"
        : this.state === "reveal" ? "Dealer shows"
        : this.state === "dealer" ? (this.value(this.dealer) < 17 ? "Dealer hit" : "Dealer stay")
        : `${this.result} Tap next`;
      gfx.text(2, 38, footer);
    }
    drawEnd() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, "Cash Out", 7);
      gfx.text(3, 20, `Bank ${this.bank}`);
      gfx.text(3, 29, `Best ${loadText("blackjack.best.init", dottedInitials())} ${this.best}`);
      gfx.text(3, 38, "Tap retry Hold menu");
    }
  }

  class OptionsGame extends Game {
    constructor() {
      super("Options");
    }
    reset() {
      this.chars = initials().split("");
      this.selected = 0;
    }
    update(dt, input, now) {
      if (input.click) this.chars[this.selected] = this.next(this.chars[this.selected]);
      if (input.longPress) {
        if (this.selected === 0) this.selected = 1;
        else {
          saveInitials(this.chars.join(""));
          app.toMenu(now);
        }
      }
    }
    next(ch) {
      const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
      const index = alphabet.indexOf(ch);
      return alphabet[(index + 1) % alphabet.length] || "A";
    }
    drawStart() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, "Options", 7);
      gfx.text(3, 21, "Player");
      gfx.text(36, 21, dottedInitials());
      gfx.text(3, 38, "Tap edit");
    }
    draw() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, "Initials", 7);
      gfx.text(26, 24, `${this.chars[0]}.${this.chars[1]}`, 7);
      gfx.line(this.selected === 0 ? 25 : 37, 27, this.selected === 0 ? 30 : 42, 27);
      gfx.text(3, 38, this.selected === 0 ? "Tap char Hold next" : "Hold save/menu");
    }
    drawEnd() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, "Saved", 7);
      gfx.text(25, 24, `${this.chars[0]}.${this.chars[1]}`, 7);
      gfx.text(3, 38, "Hold menu");
    }
  }

  class AboutSim extends Game {
    constructor() {
      super("About");
      this.items = ["License", "Credits"];
      this.credits = ["thedarkfalcon: MicriOS", "atomic14: Inspired by", "thedarkfalcon: Breakout '76", "thedarkfalcon: City Racer", "thedarkfalcon: Alien Raiders", "thedarkfalcon: Cave Chopper", "thedarkfalcon: Tower", "thedarkfalcon: Golf", "thedarkfalcon: Lander", "thedarkfalcon: Need Speed", "thedarkfalcon: Micri Field", "thedarkfalcon: Noon Shooter", "thedarkfalcon: Fishing Flick", "thedarkfalcon: Maze Runner", "thedarkfalcon: Maze Collector", "thedarkfalcon: Pipe Mania", "thedarkfalcon: Blackjack", "thedarkfalcon: Knife Throw", "thedarkfalcon: Reactor", "thedarkfalcon: Simon", "thedarkfalcon: Pet Simulator", "thedarkfalcon: Counter", "thedarkfalcon: Mouse Emulator", "thedarkfalcon: Reading", "thedarkfalcon: Stopwatch", "thedarkfalcon: Countdown", "thedarkfalcon: Dice Roller", "thedarkfalcon: Coin Flipper", "thedarkfalcon: Random Number", "thedarkfalcon: Metronome", "thedarkfalcon: Options", "thedarkfalcon: About"];
    }
    reset() {
      this.mode = "select";
      this.selection = 0;
      this.page = 0;
    }
    update(dt, input, now) {
      if (this.mode === "select") {
        if (input.click) this.selection = (this.selection + 1) % this.items.length;
        if (input.longPress) {
          this.mode = this.selection === 0 ? "license" : "credits";
          this.page = 0;
        }
        return;
      }
      if (input.longPress) {
        app.toMenu(now);
        return;
      }
      if (input.click) {
        const count = this.mode === "license" ? 6 : this.credits.length;
        if (++this.page >= count) {
          this.mode = "select";
          this.page = 0;
        }
      }
    }
    draw() {
      gfx.rect(0, 0, W, H);
      if (this.mode === "select") {
        gfx.text(3, 8, "About");
        gfx.text(58, 8, `${this.selection + 1}/2`);
        gfx.text(3, 22, this.items[this.selection], 7);
        gfx.text(3, 32, "Tap next", 7);
        gfx.text(3, 39, "Hold open");
        return;
      }
      if (this.mode === "license") {
        const pages = [
          ["License", "WTFPL +", "No Warranty"],
          ["Copyright", "2026", "github:", "thedarkfalcon"],
          ["Repo", "github.com", "Squishyware", "Official/MicriOS"],
          ["Permission", "copy modify", "publish sell", "as you want"],
          ["MIT Parts", "NerdMiner", "mining code", "uses MIT"],
          ["No Warranty", "provided as-is", "use at your", "own risk"]
        ];
        const page = pages[this.page];
        gfx.text(3, 9, page[0], 7);
        page.slice(1).forEach((line, i) => gfx.text(3, 20 + i * 9, line));
        return;
      }
      const [who, game] = this.credits[this.page].split(": ");
      gfx.text(3, 9, game, 7);
      gfx.text(3, 19, "Developer");
      gfx.text(3, 28, "github:");
      gfx.text(3, 35, who);
    }
    drawStart() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 10, "About", 7);
      gfx.text(3, 24, "Tap to view", 7);
      gfx.text(3, 36, BUILD_TEXT);
    }
    drawEnd() {
      gfx.rect(0, 0, W, H);
      gfx.text(3, 12, "About", 7);
      gfx.text(3, 26, "Tap replay", 7);
      gfx.text(3, 32, BUILD_TEXT);
      gfx.text(3, 38, "Hold menu");
    }
  }

  class App {
    constructor() {
      this.button = new Button();
      this.gameApps = [
        new PlaceholderGame("Alien Raiders"),
        new Blackjack(),
        new PlaceholderGame("Breakout '76"),
        new PlaceholderGame("Cave Chopper"),
        new PlaceholderGame("City Racer"),
        new PlaceholderGame("Micri Field"),
        new FishingFlick(),
        new PlaceholderGame("Knife Throw"),
        new MazeRunner(true),
        new MazeRunner(),
        new MiniLander(),
        new NeedSpeed(),
        new NoonShooter(),
        new PlaceholderGame("Pet Simulator"),
        new PipeMania(),
        new PlaceholderGame("Reactor"),
        new PlaceholderGame("Simon"),
        new TinyGolf(),
        new TowerStacker()
      ];
      this.utilityApps = [
        new StopwatchSim(),
        new CountdownSim(),
        new CounterSim(),
        new PlaceholderGame("Dice Roller"),
        new PlaceholderGame("Coin Flipper"),
        new PlaceholderGame("Random Number"),
        new PlaceholderGame("Metronome"),
        new MouseJigglerSim(),
        new ReadingSim()
      ];
      this.options = new OptionsGame();
      this.credits = new AboutSim();
      this.rootEntries = [
        { label: "Games", action: "games" },
        { label: "Utilities", action: "utilities" },
        { app: this.options, action: "launch" },
        { app: this.credits, action: "launch" }
      ];
      this.menu = "root";
      this.menuIndex = 0;
      this.inMenu = true;
      this.launchArmed = false;
      this.active = null;
      this.returnMenu = "root";
      this.last = performance.now();
      this.bootUntil = this.last + 2000;
    }
    toMenu(now = performance.now()) {
      this.inMenu = true;
      this.launchArmed = false;
      this.active = null;
      this.button.reset(now);
      setLed(false);
    }
    resetActive(now = performance.now()) {
      if (this.active) this.active.begin(now);
    }
    tick(now) {
      const dt = Math.min(0.05, (now - this.last) / 1000);
      this.last = now;
      const input = this.button.update(now);
      gfx.clear();
      if (now < this.bootUntil) {
        this.drawBootSplash();
      } else if (this.inMenu) {
        if (input.longPress) this.launchArmed = true;
        if (input.click && !this.launchArmed) this.menuIndex = (this.menuIndex + 1) % this.currentEntries().length;
        if (this.launchArmed && input.released) {
          this.selectMenuEntry(now);
          this.button.reset(now);
        }
        this.drawMenu();
      } else {
        this.active.tick(dt, input, now);
        this.active.render();
      }
      requestAnimationFrame((t) => this.tick(t));
    }
    currentEntries() {
      if (this.menu === "games") return [...this.gameApps.map((app) => ({ app, action: "launch" })), { label: "Back", action: "back" }];
      if (this.menu === "utilities") return [...this.utilityApps.map((app) => ({ app, action: "launch" })), { label: "Back", action: "back" }];
      return this.rootEntries;
    }
    menuTitle() {
      if (this.menu === "games") return "Games";
      if (this.menu === "utilities") return "Utilities";
      return "MicriOS";
    }
    selectMenuEntry(now) {
      const entry = this.currentEntries()[this.menuIndex];
      this.launchArmed = false;
      if (entry.action === "games" || entry.action === "utilities") {
        this.menu = entry.action;
        this.menuIndex = 0;
        return;
      }
      if (entry.action === "back") {
        this.menu = "root";
        this.menuIndex = 0;
        return;
      }
      if (entry.app) {
        this.returnMenu = this.menu;
        this.active = entry.app;
        this.active.begin(now);
        this.inMenu = false;
      }
    }
    drawBootSplash() {
      gfx.rect(0, 0, W, H);
      gfx.text(8, 12, "MicriDeck", 7);
      gfx.text(30, 25, "C3", 7);
      gfx.text(10, 38, `MicriOS ${BUILD_TEXT}`, 4);
    }
    drawMenu() {
      const entries = this.currentEntries();
      const entry = entries[this.menuIndex];
      const title = entry.app ? entry.app.title : entry.label;
      gfx.rect(0, 0, W, H);
      gfx.text(3, 9, this.menuTitle(), 4);
      gfx.text(52, 9, `${this.menuIndex + 1}/${entries.length}`, 4);
      gfx.text(3, 21, title, title.length > 12 ? 4 : 7);
      gfx.text(3, 31, this.launchArmed ? "Release" : "Tap next");
      gfx.text(3, 38, this.launchArmed ? (entry.action === "back" ? "to back" : "to open") : "Hold open");
    }
  }

  function setLed(on) {
    ledEl.classList.toggle("on", !!on);
  }

  const app = new App();
  window.app = app;

  document.addEventListener("keydown", (e) => {
    if (e.code === "Space") {
      e.preventDefault();
      app.button.setDown(true);
    }
  });
  document.addEventListener("keyup", (e) => {
    if (e.code === "Space") {
      e.preventDefault();
      app.button.setDown(false);
    }
  });
  ["pointerdown", "mousedown", "touchstart"].forEach((eventName) => {
    buttonEl.addEventListener(eventName, (e) => {
      e.preventDefault();
      app.button.setDown(true);
    });
  });
  ["pointerup", "pointerleave", "mouseup", "touchend"].forEach((eventName) => {
    buttonEl.addEventListener(eventName, (e) => {
      e.preventDefault();
      app.button.setDown(false);
    });
  });

  document.getElementById("reset").addEventListener("click", () => app.resetActive());
  document.getElementById("menu").addEventListener("click", () => app.toMenu());
  document.getElementById("clearScores").addEventListener("click", () => {
    Object.keys(localStorage).filter((k) => k.startsWith("esp32sim:")).forEach((k) => localStorage.removeItem(k));
    location.reload();
  });

  requestAnimationFrame((t) => {
    app.last = t;
    app.tick(t);
  });
})();
