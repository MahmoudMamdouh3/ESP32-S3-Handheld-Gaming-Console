function drawFavoritesIcon(ctx) {
  ctx.fillStyle = "#FFE033";
  ctx.beginPath();
  const cx = 29;
  const cy = 36;
  const spikes = 5;
  const outerRadius = 24;
  const innerRadius = 11;
  let rot = (Math.PI / 2) * 3;
  const step = Math.PI / spikes;
  ctx.moveTo(cx, cy - outerRadius);
  for (let i = 0; i < spikes; i++) {
    ctx.lineTo(cx + Math.cos(rot) * outerRadius, cy + Math.sin(rot) * outerRadius);
    rot += step;
    ctx.lineTo(cx + Math.cos(rot) * innerRadius, cy + Math.sin(rot) * innerRadius);
    rot += step;
  }
  ctx.lineTo(cx, cy - outerRadius);
  ctx.closePath();
  ctx.fill();

  ctx.fillStyle = "#0F2620";
  ctx.beginPath();
  ctx.arc(23, 34, 2, 0, Math.PI * 2);
  ctx.arc(35, 34, 2, 0, Math.PI * 2);
  ctx.fill();

  ctx.strokeStyle = "#0F2620";
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.arc(29, 36, 3.5, 0.1 * Math.PI, 0.9 * Math.PI, false);
  ctx.stroke();

  ctx.fillStyle = "#F48FB1";
  ctx.beginPath();
  ctx.arc(20, 37, 1.8, 0, Math.PI * 2);
  ctx.arc(38, 37, 1.8, 0, Math.PI * 2);
  ctx.fill();
}

function drawGameBoyIcon(ctx, c) {
  const isGb = c.id === "ROM_GB";
  ctx.fillStyle = isGb ? "#5FB49C" : "#9C88FF";
  ctx.beginPath();
  ctx.roundRect(11, 6, 36, 62, [6, 6, 12, 6]);
  ctx.fill();
  ctx.fillStyle = "#1A4B42";
  ctx.fillRect(15, 11, 28, 24);
  ctx.fillStyle = isGb ? "#CEF5E4" : "#D4F7EA";
  ctx.fillRect(19, 14, 20, 18);
  ctx.fillStyle = "#0F2620";
  ctx.fillRect(23, 20, 2, 3);
  ctx.fillRect(33, 20, 2, 3);
  ctx.fillRect(26, 26, 6, 2);
  ctx.fillStyle = "#FFE033";
  ctx.fillRect(16, 42, 10, 3);
  ctx.fillRect(19, 39, 4, 9);
  ctx.fillStyle = "#FF91A5";
  ctx.beginPath();
  ctx.arc(38, 43, 2.5, 0, Math.PI * 2);
  ctx.arc(32, 47, 2.5, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = "rgba(15, 38, 32, 0.4)";
  ctx.fillRect(36, 57, 6, 1);
  ctx.fillRect(38, 60, 5, 1);
}

function drawNesIcon(ctx) {
  ctx.fillStyle = "#C8D6E5";
  ctx.beginPath();
  ctx.roundRect(8, 20, 42, 28, 3);
  ctx.fill();
  ctx.fillStyle = "#1E272E";
  ctx.fillRect(11, 23, 36, 22);
  ctx.fillStyle = "#C8D6E5";
  ctx.fillRect(14, 30, 8, 3);
  ctx.fillRect(17, 27, 3, 9);
  ctx.fillStyle = "#FF4757";
  ctx.beginPath();
  ctx.arc(36, 34, 2.5, 0, Math.PI * 2);
  ctx.arc(42, 34, 2.5, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillRect(24, 39, 8, 2);
}

function drawSnesIcon(ctx) {
  ctx.fillStyle = "#D2D7DE";
  ctx.beginPath();
  ctx.roundRect(6, 20, 46, 26, 13);
  ctx.fill();
  ctx.fillStyle = "#A4B0BE";
  ctx.beginPath();
  ctx.roundRect(10, 23, 38, 20, 10);
  ctx.fill();
  ctx.fillStyle = "#2F3542";
  ctx.fillRect(13, 30, 8, 3);
  ctx.fillRect(16, 27, 3, 9);
  const buttons = [
    ["#FFE033", 38, 27],
    ["#2ED573", 34, 31],
    ["#3742FA", 42, 31],
    ["#FF4757", 38, 35],
  ];
  for (const [color, bx, by] of buttons) {
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.arc(bx, by, 2, 0, Math.PI * 2);
    ctx.fill();
  }
}

function drawGenesisIcon(ctx) {
  ctx.fillStyle = "#1E272E";
  ctx.beginPath();
  ctx.roundRect(7, 20, 44, 26, [14, 14, 8, 8]);
  ctx.fill();
  ctx.fillStyle = "#FF4757";
  ctx.fillRect(25, 24, 7, 2);
  ctx.fillStyle = "#57606F";
  ctx.beginPath();
  ctx.arc(17, 33, 6, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = "#747D8C";
  ctx.beginPath();
  ctx.arc(33, 36, 2.2, 0, Math.PI * 2);
  ctx.arc(38, 33, 2.2, 0, Math.PI * 2);
  ctx.arc(43, 30, 2.2, 0, Math.PI * 2);
  ctx.fill();
}

function drawHandheldLandscapeIcon(ctx) {
  ctx.fillStyle = "#2F3542";
  ctx.beginPath();
  ctx.roundRect(6, 18, 46, 32, 8);
  ctx.fill();
  ctx.fillStyle = "#50C8D2";
  ctx.fillRect(17, 23, 24, 18);
  ctx.fillStyle = "#FFE033";
  ctx.fillRect(10, 28, 4, 4);
  ctx.fillStyle = "#FF4757";
  ctx.beginPath();
  ctx.arc(45, 30, 2, 0, Math.PI * 2);
  ctx.arc(45, 35, 2, 0, Math.PI * 2);
  ctx.fill();
}

function drawAtariIcon(ctx) {
  ctx.fillStyle = "#2F3542";
  ctx.beginPath();
  ctx.roundRect(14, 30, 30, 26, 4);
  ctx.fill();
  ctx.strokeStyle = "#FF7F50";
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.arc(29, 43, 8, 0, Math.PI * 2);
  ctx.stroke();
  ctx.fillStyle = "#FF4757";
  ctx.beginPath();
  ctx.arc(19, 36, 3, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = "#1E272E";
  ctx.fillRect(27, 14, 4, 20);
  ctx.beginPath();
  ctx.arc(29, 14, 5, 0, Math.PI * 2);
  ctx.fill();
}

function drawPico8Icon(ctx) {
  ctx.fillStyle = "#303A52";
  ctx.beginPath();
  ctx.roundRect(12, 14, 34, 44, 4);
  ctx.fill();
  ctx.fillStyle = "#FFE033";
  ctx.fillRect(16, 20, 26, 26);
  const colors = ["#FF004D", "#FFA300", "#FFEC27", "#00E436", "#29ADFF"];
  for (let ci = 0; ci < colors.length; ci++) {
    ctx.fillStyle = colors[ci];
    ctx.fillRect(16 + ci * 5.2, 42, 5.2, 4);
  }
  ctx.fillStyle = "#1D2B53";
  ctx.beginPath();
  ctx.arc(29, 30, 4, 0, Math.PI * 2);
  ctx.fill();
}

function drawDoomIcon(ctx) {
  ctx.fillStyle = "#2F3542";
  ctx.beginPath();
  ctx.roundRect(14, 18, 30, 32, 6);
  ctx.fill();
  ctx.fillStyle = "#2ED573";
  ctx.fillRect(18, 26, 22, 7);
  ctx.fillStyle = "#FF4757";
  ctx.beginPath();
  ctx.moveTo(14, 20);
  ctx.lineTo(10, 12);
  ctx.lineTo(18, 16);
  ctx.moveTo(44, 20);
  ctx.lineTo(48, 12);
  ctx.lineTo(40, 16);
  ctx.fill();
}

export function drawGenericCartridgeIcon(ctx, c) {
  ctx.fillStyle = "#1E272E";
  ctx.beginPath();
  ctx.roundRect(10, 14, 38, 44, 6);
  ctx.fill();
  ctx.fillStyle = c.color;
  ctx.fillRect(14, 20, 30, 20);
  ctx.fillStyle = "#0B0F19";
  ctx.font = "bold 9px 'Press Start 2P', monospace";
  ctx.textAlign = "center";
  ctx.fillText(c.badge, 29, 34);
  ctx.fillStyle = "#A4B0BE";
  ctx.fillRect(16, 44, 26, 3);
  ctx.fillRect(16, 49, 18, 2);
}

export const PIXEL_ART_PAINTERS = {
  ROM_FAVORITES: drawFavoritesIcon,
  ROM_GB: drawGameBoyIcon,
  ROM_GBC: drawGameBoyIcon,
  ROM_NES: drawNesIcon,
  ROM_SNES: drawSnesIcon,
  ROM_GENESIS: drawGenesisIcon,
  ROM_GG: drawHandheldLandscapeIcon,
  ROM_LYNX: drawHandheldLandscapeIcon,
  ROM_ATARI: drawAtariIcon,
  ROM_PICO8: drawPico8Icon,
  ROM_WAD: drawDoomIcon,
};
