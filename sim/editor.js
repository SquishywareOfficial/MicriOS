(() => {
  const W = 72;
  const H = 40;
  const MAZE_COLS = 9;
  const MAZE_ROWS = 5;
  const WALL_N = 1;
  const WALL_E = 2;
  const WALL_S = 4;
  const WALL_W = 8;

  const canvas = document.getElementById("editorCanvas");
  const ctx = canvas.getContext("2d");
  const output = document.getElementById("output");
  const hint = document.getElementById("hint");
  const golfTab = document.getElementById("golfTab");
  const mazeTab = document.getElementById("mazeTab");
  const golfTools = document.getElementById("golfTools");
  const mazeTools = document.getElementById("mazeTools");

  let mode = "golf";
  let golfTool = "start";
  let dragStart = null;
  const golf = {
    start: [8, 32],
    cup: [59, 16],
    walls: [],
    guides: []
  };
  const maze = {
    walls: Array.from({ length: MAZE_COLS * MAZE_ROWS }, (_, i) => {
      const x = i % MAZE_COLS;
      const y = Math.floor(i / MAZE_COLS);
      let mask = 0;
      if (y === 0) mask |= WALL_N;
      if (x === MAZE_COLS - 1) mask |= WALL_E;
      if (y === MAZE_ROWS - 1) mask |= WALL_S;
      if (x === 0) mask |= WALL_W;
      return mask;
    })
  };

  function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  function canvasPoint(event) {
    const rect = canvas.getBoundingClientRect();
    const x = Math.floor(((event.clientX - rect.left) / rect.width) * W);
    const y = Math.floor(((event.clientY - rect.top) / rect.height) * H);
    return [clamp(x - 1, 0, 69), clamp(y, 0, 39)];
  }

  function setMode(nextMode) {
    mode = nextMode;
    dragStart = null;
    golfTab.classList.toggle("active", mode === "golf");
    mazeTab.classList.toggle("active", mode === "maze");
    golfTools.classList.toggle("hidden", mode !== "golf");
    mazeTools.classList.toggle("hidden", mode !== "maze");
    hint.textContent = mode === "golf"
      ? "Drag walls/guides, click ball/cup. Coordinates match Tiny Golf's game area."
      : "Click near a cell edge to toggle that wall. Neighbor walls are updated too.";
    draw();
  }

  function drawPixelRect(x, y, w, h, color = "#f8fafc") {
    ctx.fillStyle = color;
    ctx.fillRect(x + 1, y, w, h);
  }

  function drawGolf(previewPoint = null) {
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, W, H);
    ctx.strokeStyle = "#f8fafc";
    ctx.strokeRect(0.5, 0.5, 70, 39);
    ctx.strokeStyle = "#64748b";
    ctx.beginPath();
    ctx.moveTo(2, 7.5);
    ctx.lineTo(69, 7.5);
    ctx.stroke();

    for (const wall of golf.walls) {
      drawPixelRect(wall.x, wall.y, wall.w, wall.h);
    }
    ctx.strokeStyle = "#facc15";
    for (const guide of golf.guides) {
      ctx.beginPath();
      ctx.moveTo(guide.x1 + 1.5, guide.y1 + 0.5);
      ctx.lineTo(guide.x2 + 1.5, guide.y2 + 0.5);
      ctx.stroke();
    }

    ctx.strokeStyle = "#22c55e";
    ctx.beginPath();
    ctx.arc(golf.cup[0] + 1, golf.cup[1], 2.3, 0, Math.PI * 2);
    ctx.stroke();
    ctx.fillStyle = "#38bdf8";
    ctx.beginPath();
    ctx.arc(golf.start[0] + 1, golf.start[1], 1.8, 0, Math.PI * 2);
    ctx.fill();

    if (dragStart && previewPoint) {
      ctx.strokeStyle = "#fb7185";
      if (golfTool === "wall") {
        const rect = normalizeRect(dragStart, previewPoint);
        ctx.strokeRect(rect.x + 1.5, rect.y + 0.5, rect.w, rect.h);
      } else if (golfTool === "guide") {
        ctx.beginPath();
        ctx.moveTo(dragStart[0] + 1.5, dragStart[1] + 0.5);
        ctx.lineTo(previewPoint[0] + 1.5, previewPoint[1] + 0.5);
        ctx.stroke();
      }
    }
  }

  function mazeIndex(x, y) {
    return y * MAZE_COLS + x;
  }

  function drawMaze() {
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, W, H);
    ctx.strokeStyle = "#f8fafc";
    ctx.lineWidth = 1;
    for (let y = 0; y < MAZE_ROWS; y++) {
      for (let x = 0; x < MAZE_COLS; x++) {
        const sx = x * 8;
        const sy = y * 8;
        const mask = maze.walls[mazeIndex(x, y)];
        ctx.beginPath();
        if (mask & WALL_N) {
          ctx.moveTo(sx, sy + 0.5);
          ctx.lineTo(sx + 8, sy + 0.5);
        }
        if (mask & WALL_E) {
          ctx.moveTo(sx + 8.5, sy);
          ctx.lineTo(sx + 8.5, sy + 8);
        }
        if (mask & WALL_S) {
          ctx.moveTo(sx, sy + 8.5);
          ctx.lineTo(sx + 8, sy + 8.5);
        }
        if (mask & WALL_W) {
          ctx.moveTo(sx + 0.5, sy);
          ctx.lineTo(sx + 0.5, sy + 8);
        }
        ctx.stroke();
      }
    }
    ctx.fillStyle = "#38bdf8";
    ctx.fillRect(2, 2, 3, 3);
    ctx.strokeStyle = "#22c55e";
    ctx.strokeRect(65.5, 33.5, 5, 5);
  }

  function draw(previewPoint = null) {
    ctx.imageSmoothingEnabled = false;
    if (mode === "golf") {
      drawGolf(previewPoint);
    } else {
      drawMaze();
    }
    renderOutput();
  }

  function normalizeRect(a, b) {
    const x = Math.min(a[0], b[0]);
    const y = Math.min(a[1], b[1]);
    return {
      x,
      y,
      w: Math.max(1, Math.abs(a[0] - b[0]) + 1),
      h: Math.max(1, Math.abs(a[1] - b[1]) + 1)
    };
  }

  function paddedItems(items, limit, empty, formatter) {
    const result = [];
    for (let i = 0; i < limit; i++) {
      result.push(i < items.length ? formatter(items[i]) : empty);
    }
    return result.join(", ");
  }

  function renderGolfOutput() {
    const walls = paddedItems(
      golf.walls,
      4,
      "{0, 0, 0, 0}",
      w => `{${w.x}, ${w.y}, ${w.w}, ${w.h}}`
    );
    const guides = paddedItems(
      golf.guides,
      3,
      "{0, 0, 0, 0}",
      g => `{${g.x1}, ${g.y1}, ${g.x2}, ${g.y2}}`
    );
    return [
      "// Paste into HOLES in TinyGolfGame.cpp.",
      "// Ball/cup use x,y. Walls use x,y,width,height. Guides use x1,y1,x2,y2.",
      `{${golf.start[0]}, ${golf.start[1]}, ${golf.cup[0]}, ${golf.cup[1]}, {${walls}}, {${guides}}},`
    ].join("\n");
  }

  function renderMazeOutput() {
    const rows = [];
    for (let y = 0; y < MAZE_ROWS; y++) {
      const values = [];
      for (let x = 0; x < MAZE_COLS; x++) {
        values.push(String(maze.walls[mazeIndex(x, y)]).padStart(2, " "));
      }
      rows.push(`  ${values.join(", ")}`);
    }
    return [
      "// Wall bits: N=1, E=2, S=4, W=8.",
      "// Paste the values into a Maze Runner/Collector map definition.",
      "const uint8_t mazeWalls[45] = {",
      rows.join(",\n"),
      "};"
    ].join("\n");
  }

  function renderOutput() {
    output.value = mode === "golf" ? renderGolfOutput() : renderMazeOutput();
  }

  function applyGolfClick(point) {
    if (golfTool === "start") golf.start = point;
    if (golfTool === "cup") golf.cup = point;
  }

  function addGolfShape(point) {
    if (!dragStart) return;
    if (golfTool === "wall" && golf.walls.length < 4) {
      golf.walls.push(normalizeRect(dragStart, point));
    } else if (golfTool === "guide" && golf.guides.length < 3) {
      golf.guides.push({ x1: dragStart[0], y1: dragStart[1], x2: point[0], y2: point[1] });
    }
    dragStart = null;
  }

  function toggleMazeWall(point) {
    const x = clamp(Math.floor((point[0] + 1) / 8), 0, MAZE_COLS - 1);
    const y = clamp(Math.floor(point[1] / 8), 0, MAZE_ROWS - 1);
    const localX = (point[0] + 1) - x * 8;
    const localY = point[1] - y * 8;
    const distances = [
      { wall: WALL_N, dx: 0, dy: -1, other: WALL_S, distance: localY },
      { wall: WALL_E, dx: 1, dy: 0, other: WALL_W, distance: 8 - localX },
      { wall: WALL_S, dx: 0, dy: 1, other: WALL_N, distance: 8 - localY },
      { wall: WALL_W, dx: -1, dy: 0, other: WALL_E, distance: localX }
    ].sort((a, b) => a.distance - b.distance);
    const edge = distances[0];
    if (edge.distance > 3) return;

    const index = mazeIndex(x, y);
    const nextOn = (maze.walls[index] & edge.wall) === 0;
    if (nextOn) maze.walls[index] |= edge.wall;
    else maze.walls[index] &= ~edge.wall;

    const nx = x + edge.dx;
    const ny = y + edge.dy;
    if (nx >= 0 && nx < MAZE_COLS && ny >= 0 && ny < MAZE_ROWS) {
      const neighbor = mazeIndex(nx, ny);
      if (nextOn) maze.walls[neighbor] |= edge.other;
      else maze.walls[neighbor] &= ~edge.other;
    }
  }

  golfTab.addEventListener("click", () => setMode("golf"));
  mazeTab.addEventListener("click", () => setMode("maze"));

  golfTools.addEventListener("click", event => {
    const button = event.target.closest("button");
    if (!button) return;
    const tool = button.dataset.tool;
    if (tool) {
      golfTool = tool;
      for (const item of golfTools.querySelectorAll("[data-tool]")) {
        item.classList.toggle("active", item === button);
      }
    }
  });

  document.getElementById("undoGolf").addEventListener("click", () => {
    if (golf.guides.length > 0) golf.guides.pop();
    else if (golf.walls.length > 0) golf.walls.pop();
    draw();
  });

  document.getElementById("clearGolf").addEventListener("click", () => {
    golf.walls.length = 0;
    golf.guides.length = 0;
    draw();
  });

  document.getElementById("clearMaze").addEventListener("click", () => {
    maze.walls.fill(0);
    for (let y = 0; y < MAZE_ROWS; y++) {
      for (let x = 0; x < MAZE_COLS; x++) {
        const i = mazeIndex(x, y);
        if (y === 0) maze.walls[i] |= WALL_N;
        if (x === MAZE_COLS - 1) maze.walls[i] |= WALL_E;
        if (y === MAZE_ROWS - 1) maze.walls[i] |= WALL_S;
        if (x === 0) maze.walls[i] |= WALL_W;
      }
    }
    draw();
  });

  canvas.addEventListener("mousedown", event => {
    const point = canvasPoint(event);
    if (mode === "maze") {
      toggleMazeWall(point);
      draw();
      return;
    }
    if (golfTool === "start" || golfTool === "cup") {
      applyGolfClick(point);
      draw();
    } else {
      dragStart = point;
    }
  });

  canvas.addEventListener("mousemove", event => {
    if (dragStart && mode === "golf") draw(canvasPoint(event));
  });

  window.addEventListener("mouseup", event => {
    if (!dragStart || mode !== "golf") return;
    addGolfShape(canvasPoint(event));
    draw();
  });

  draw();
})();
