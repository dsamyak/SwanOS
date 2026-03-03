"""
SwanOS — Modern Python GUI Frontend
Communicates with the C kernel via VirtualBox named pipe.

Usage:
  python swanos_gui.py --demo                     # Standalone demo
  python swanos_gui.py --pipe \\\\.\\pipe\\swanos   # VirtualBox pipe

Requirements:  pip install PyQt5 pywin32 requests
"""

import sys, os, math, time, random, argparse, threading
from datetime import datetime

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QLineEdit, QTextEdit, QFrame,
    QStackedWidget, QGraphicsDropShadowEffect, QTreeWidget, QTreeWidgetItem,
)
from PyQt5.QtCore import Qt, QTimer, pyqtSignal, QObject, QThread
from PyQt5.QtGui import (
    QFont, QColor, QLinearGradient, QPainter, QPen, QBrush,
    QPainterPath, QRadialGradient, QPalette, QTextCursor, QTextCharFormat,
)

# ── Config ─────────────────────────────────────────────────
API_URL = "https://api.groq.com/openai/v1/chat/completions"
MODEL = "llama-3.3-70b-versatile"
API_KEY = os.getenv("GROQ_API_KEY", "")
if not API_KEY:
    env_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env")
    if os.path.exists(env_path):
        for line in open(env_path):
            if line.startswith("GROQ_API_KEY="):
                API_KEY = line.split("=", 1)[1].strip()

SYSTEM_PROMPT = (
    "You are SwanOS AI, the intelligence inside a bare-metal operating system. "
    "Be concise and helpful. Keep responses under 200 words. "
    "You are running directly on x86 hardware with no other OS underneath."
)

# ── Colors ─────────────────────────────────────────────────
C = {
    "bg0": "#0a0e17", "bg1": "#0f1423", "bg2": "#151b2e",
    "bg3": "#1a2139", "bg4": "#212a45",
    "brd": "#2a3555", "brd2": "#3a4a70",
    "t1": "#e8ecf4", "t2": "#8892a8", "t3": "#556080",
    "cyan": "#00d4ff", "blue": "#4d7cff", "purple": "#a855f7",
    "green": "#22c55e", "orange": "#f59e0b", "red": "#ef4444",
}
FONT_MONO = "Cascadia Code, JetBrains Mono, Consolas, monospace"

def call_llm(query):
    if not API_KEY:
        return "Error: GROQ_API_KEY not set. Set it in .env file."
    try:
        import requests
        r = requests.post(API_URL, headers={
            "Content-Type": "application/json",
            "Authorization": f"Bearer {API_KEY}",
        }, json={
            "model": MODEL, "max_tokens": 512, "temperature": 0.7,
            "messages": [
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": query},
            ],
        }, timeout=30)
        if r.status_code == 200:
            return r.json()["choices"][0]["message"]["content"]
        return f"API error {r.status_code}"
    except Exception as e:
        return f"Error: {e}"


# ══════════════════════════════════════════════════════════════
#  WINDOWS NAMED PIPE CONNECTION
# ══════════════════════════════════════════════════════════════
class PipeConnection:
    """Handles Windows named pipe I/O using win32file."""

    def __init__(self, pipe_path):
        self.pipe_path = pipe_path
        self.handle = None

    def connect(self):
        """Open the named pipe. Returns True on success."""
        try:
            import win32file
            import win32pipe
            self.handle = win32file.CreateFile(
                self.pipe_path,
                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0, None,
                win32file.OPEN_EXISTING,
                0, None
            )
            # Set pipe to byte mode
            win32pipe.SetNamedPipeHandleState(
                self.handle,
                win32pipe.PIPE_READMODE_BYTE,
                None, None
            )
            return True
        except Exception as e:
            print(f"[Pipe] Connect failed: {e}", file=sys.stderr)
            self.handle = None
            return False

    def read(self, size=1):
        """Read bytes from pipe. Returns bytes or None."""
        if not self.handle:
            return None
        try:
            import win32file
            hr, data = win32file.ReadFile(self.handle, size)
            return data
        except Exception:
            return None

    def write(self, data):
        """Write bytes to pipe."""
        if not self.handle:
            return
        try:
            import win32file
            if isinstance(data, str):
                data = data.encode('ascii', errors='replace')
            win32file.WriteFile(self.handle, data)
        except Exception as e:
            print(f"[Pipe] Write error: {e}", file=sys.stderr)

    def close(self):
        if self.handle:
            try:
                import win32file
                win32file.CloseHandle(self.handle)
            except:
                pass
            self.handle = None


class SerialWorker(QObject):
    """Reads from VirtualBox pipe, emits signals to GUI."""
    data_received = pyqtSignal(str)
    llm_query_received = pyqtSignal(str)
    connection_status = pyqtSignal(bool)

    def __init__(self, pipe_path):
        super().__init__()
        self.pipe_path = pipe_path
        self.running = True
        self.pipe = PipeConnection(pipe_path)

    def start_reading(self):
        """Connect and read loop (runs in worker thread)."""
        # Retry connection
        while self.running:
            print(f"[Serial] Connecting to {self.pipe_path}...", file=sys.stderr)
            if self.pipe.connect():
                print("[Serial] Connected!", file=sys.stderr)
                self.connection_status.emit(True)
                self._read_loop()
                self.connection_status.emit(False)
            else:
                time.sleep(2)

    def _read_loop(self):
        state = 'normal'
        query_buf = ""
        ansi_buf = ""

        while self.running:
            data = self.pipe.read(1)
            if not data:
                time.sleep(0.01)
                continue

            ch = data.decode('ascii', errors='replace')

            if state == 'normal':
                if ch == '\x01':
                    state = 'marker'
                elif ch == '\x1b':
                    state = 'ansi'
                    ansi_buf = '\x1b'
                else:
                    self.data_received.emit(ch)

            elif state == 'marker':
                if ch == 'Q':
                    state = 'query'
                    query_buf = ""
                else:
                    state = 'normal'

            elif state == 'query':
                if ch == '\x04':
                    self.llm_query_received.emit(query_buf)
                    state = 'normal'
                else:
                    query_buf += ch

            elif state == 'ansi':
                ansi_buf += ch
                if ch == 'm' or len(ansi_buf) > 10:
                    self.data_received.emit(ansi_buf)
                    state = 'normal'

    def send_char(self, ch):
        self.pipe.write(ch)

    def send_response(self, text):
        """Send LLM response back to kernel."""
        self.pipe.write(text.encode('ascii', errors='replace') + b'\x04')

    def stop(self):
        self.running = False
        self.pipe.close()


# ══════════════════════════════════════════════════════════════
#  PARTICLES
# ══════════════════════════════════════════════════════════════
class Particle:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.reset()
    def reset(self):
        self.x = random.uniform(0, self.w)
        self.y = random.uniform(0, self.h)
        self.vx = random.uniform(-0.3, 0.3)
        self.vy = random.uniform(-0.5, -0.1)
        self.size = random.uniform(1, 3)
        self.life = random.uniform(0.5, 1.0)
    def update(self):
        self.x += self.vx; self.y += self.vy; self.life -= 0.005
        if self.life <= 0: self.reset(); self.y = self.h


# ══════════════════════════════════════════════════════════════
#  BOOT SPLASH
# ══════════════════════════════════════════════════════════════
class BootSplash(QWidget):
    boot_finished = pyqtSignal()
    def __init__(self):
        super().__init__()
        self.setFixedSize(900, 600)
        self.progress = 0.0; self.phase = 0; self.alpha = 0.0
        self.particles = [Particle(900, 600) for _ in range(60)]
        self.msgs = ["Initializing kernel...", "Loading GDT...", "Remapping PIC...",
            "Initializing IDT...", "Starting PIT timer...", "Loading keyboard driver...",
            "Initializing COM1 serial...", "Setting up memory (4 MB)...",
            "Mounting filesystem...", "Starting user manager...",
            "Connecting to AI bridge...", "All systems online."]
        self.msg_idx = 0
        self.timer = QTimer(self); self.timer.timeout.connect(self._tick); self.timer.start(16)
        self._t0 = time.time()

    def _tick(self):
        dt = time.time() - self._t0
        if self.phase == 0:
            self.alpha = min(1.0, dt); 
            if self.alpha >= 1.0: self.phase = 1; self._t0 = time.time()
        elif self.phase == 1:
            if dt > 1.0: self.phase = 2; self._t0 = time.time()
        elif self.phase == 2:
            self.progress = min(1.0, dt / 4.0)
            self.msg_idx = min(len(self.msgs)-1, int(self.progress * len(self.msgs)))
            if self.progress >= 1.0: self.phase = 3; self._t0 = time.time()
        elif self.phase == 3:
            self.alpha = max(0.0, 1.0 - dt / 0.8)
            if self.alpha <= 0.0: self.timer.stop(); self.boot_finished.emit(); return
        for p in self.particles: p.update()
        self.update()

    def paintEvent(self, e):
        p = QPainter(self); p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()
        bg = QLinearGradient(0,0,0,h)
        bg.setColorAt(0, QColor(8,12,22)); bg.setColorAt(1, QColor(6,10,18))
        p.fillRect(0,0,w,h,bg)
        gl = QRadialGradient(w/2, h*0.35, 300)
        gl.setColorAt(0, QColor(0,120,180,25)); gl.setColorAt(1, QColor(0,0,0,0))
        p.setBrush(QBrush(gl)); p.setPen(Qt.NoPen)
        p.drawEllipse(int(w/2-300), int(h*0.35-300), 600, 600)
        for pt in self.particles:
            p.setBrush(QColor(0,200,255,int(max(0,pt.life)*200*self.alpha))); p.setPen(Qt.NoPen)
            p.drawEllipse(int(pt.x), int(pt.y), int(pt.size), int(pt.size))
        cx, cy = w//2, int(h*0.32)
        self._swan(p, cx, cy)
        p.setOpacity(self.alpha)
        f = QFont("Segoe UI", 36, QFont.Light); f.setLetterSpacing(QFont.AbsoluteSpacing, 8)
        p.setFont(f); p.setPen(QColor(232,236,244))
        p.drawText(0, cy+75, w, 50, Qt.AlignCenter, "SWAN OS")
        sf = QFont("Segoe UI", 12); sf.setLetterSpacing(QFont.AbsoluteSpacing, 4)
        p.setFont(sf); p.setPen(QColor(0,212,255,int(180*self.alpha)))
        p.drawText(0, cy+115, w, 30, Qt.AlignCenter, "LLM-POWERED OPERATING SYSTEM")
        if self.phase >= 2:
            bw, bx, by = 320, (w-320)//2, cy+170
            p.setPen(Qt.NoPen); p.setBrush(QColor(30,40,60,int(200*self.alpha)))
            p.drawRoundedRect(bx, by, bw, 4, 2, 2)
            fw = int(bw*self.progress)
            if fw > 0:
                g2 = QLinearGradient(bx,0,bx+bw,0)
                g2.setColorAt(0, QColor(0,180,255,int(255*self.alpha)))
                g2.setColorAt(1, QColor(34,197,94,int(255*self.alpha)))
                p.setBrush(QBrush(g2)); p.drawRoundedRect(bx, by, fw, 4, 2, 2)
            mf = QFont("Cascadia Code", 9); p.setFont(mf)
            p.setPen(QColor(136,146,168,int(200*self.alpha)))
            if self.msg_idx < len(self.msgs):
                p.drawText(0, by+20, w, 25, Qt.AlignCenter, self.msgs[self.msg_idx])
        p.setOpacity(1.0); p.end()

    def _swan(self, p, cx, cy):
        p.setOpacity(self.alpha)
        p.setPen(QPen(QColor(0,212,255,int(180*self.alpha)), 2)); p.setBrush(Qt.NoBrush)
        p.drawEllipse(cx-45, cy-45, 90, 90)
        p.setPen(Qt.NoPen); p.setBrush(QColor(232,236,244,int(230*self.alpha)))
        path = QPainterPath(); path.addEllipse(cx-18, cy-5, 36, 22); p.drawPath(path)
        for i,(nx,ny) in enumerate([(-10,0),(-16,-10),(-18,-20),(-14,-28),(-8,-32)]):
            r = 5-i*0.5; p.drawEllipse(int(cx+nx-r), int(cy+ny-r), int(r*2), int(r*2))
        p.drawEllipse(cx-11, cy-37, 14, 12)
        p.setBrush(QColor(10,14,25,int(255*self.alpha))); p.drawEllipse(cx-5, cy-33, 3, 3)
        p.setBrush(QColor(245,158,11,int(230*self.alpha)))
        bk = QPainterPath(); bk.moveTo(cx+2,cy-33); bk.lineTo(cx+12,cy-30)
        bk.lineTo(cx+2,cy-28); bk.closeSubpath(); p.drawPath(bk)
        p.setOpacity(1.0)


# ══════════════════════════════════════════════════════════════
#  LOGIN SCREEN
# ══════════════════════════════════════════════════════════════
class LoginScreen(QWidget):
    login_success = pyqtSignal(str)
    def __init__(self):
        super().__init__()
        self.particles = [Particle(900, 600) for _ in range(40)]
        self._build()
        self.pt = QTimer(self)
        self.pt.timeout.connect(lambda: [pp.update() for pp in self.particles] or self.update())
        self.pt.start(30)

    def paintEvent(self, e):
        p = QPainter(self); p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()
        bg = QLinearGradient(0,0,0,h)
        bg.setColorAt(0, QColor(8,12,22)); bg.setColorAt(1, QColor(12,18,35))
        p.fillRect(0,0,w,h,bg)
        for pt in self.particles:
            p.setBrush(QColor(0,200,255,int(max(0,pt.life)*200))); p.setPen(Qt.NoPen)
            p.drawEllipse(int(pt.x), int(pt.y), int(pt.size), int(pt.size))
        p.end(); super().paintEvent(e)

    def _build(self):
        lo = QVBoxLayout(self); lo.setAlignment(Qt.AlignCenter)
        card = QFrame(); card.setFixedSize(380, 400)
        card.setStyleSheet("QFrame{background:rgba(15,20,35,0.88);border:1px solid rgba(42,53,85,0.6);border-radius:20px;}")
        cl = QVBoxLayout(card); cl.setContentsMargins(40,30,40,30); cl.setSpacing(14)
        ic = QLabel("🦢"); ic.setAlignment(Qt.AlignCenter)
        ic.setStyleSheet("font-size:42px;background:transparent;border:none;"); cl.addWidget(ic)
        t = QLabel("Swan OS"); t.setAlignment(Qt.AlignCenter)
        t.setStyleSheet(f"color:{C['t1']};font-size:26px;font-weight:300;letter-spacing:3px;background:transparent;border:none;")
        cl.addWidget(t)
        s = QLabel("LLM-Powered Operating System"); s.setAlignment(Qt.AlignCenter)
        s.setStyleSheet(f"color:{C['cyan']};font-size:10px;letter-spacing:2px;margin-bottom:12px;background:transparent;border:none;")
        cl.addWidget(s)
        iss = (f"QLineEdit{{background:rgba(21,27,46,0.9);border:1px solid {C['brd']};"
               f"border-radius:10px;padding:10px 16px;color:{C['t1']};font-size:13px;}}"
               f"QLineEdit:focus{{border:1px solid {C['cyan']};}}"
               f"QLineEdit::placeholder{{color:{C['t3']};}}")
        self.ui = QLineEdit(); self.ui.setPlaceholderText("Username"); self.ui.setStyleSheet(iss)
        self.ui.setMinimumHeight(44); cl.addWidget(self.ui)
        self.pi = QLineEdit(); self.pi.setPlaceholderText("Password"); self.pi.setEchoMode(QLineEdit.Password)
        self.pi.setStyleSheet(iss); self.pi.setMinimumHeight(44); self.pi.returnPressed.connect(self._go)
        cl.addWidget(self.pi)
        self.err = QLabel(""); self.err.setAlignment(Qt.AlignCenter)
        self.err.setStyleSheet(f"color:{C['red']};font-size:11px;background:transparent;border:none;"); cl.addWidget(self.err)
        btn = QPushButton("Sign In"); btn.setMinimumHeight(44); btn.setCursor(Qt.PointingHandCursor)
        btn.setStyleSheet(f"QPushButton{{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 {C['cyan']},stop:1 {C['blue']});"
            f"color:#0a0e17;border:none;border-radius:10px;font-size:14px;font-weight:600;letter-spacing:1px;}}"
            f"QPushButton:hover{{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #33ddff,stop:1 #6690ff);}}")
        btn.clicked.connect(self._go); cl.addWidget(btn)
        h2 = QLabel("Default: admin / admin"); h2.setAlignment(Qt.AlignCenter)
        h2.setStyleSheet(f"color:{C['t3']};font-size:10px;background:transparent;border:none;"); cl.addWidget(h2)
        sh = QGraphicsDropShadowEffect(); sh.setBlurRadius(60); sh.setColor(QColor(0,150,220,40)); sh.setOffset(0,10)
        card.setGraphicsEffect(sh); lo.addWidget(card)

    def _go(self):
        u = self.ui.text().strip()
        if not u: self.err.setText("Enter a username"); return
        self.login_success.emit(u)


# ══════════════════════════════════════════════════════════════
#  TERMINAL
# ══════════════════════════════════════════════════════════════
class Terminal(QTextEdit):
    command_entered = pyqtSignal(str)
    def __init__(self, user="admin"):
        super().__init__()
        self.user = user; self.hist = []; self.hi = -1; self.pp = 0
        self._serial = False; self._color = C['t1']
        self.setStyleSheet(f"QTextEdit{{background:{C['bg0']};color:{C['t1']};border:none;padding:12px;"
                          f"font-family:{FONT_MONO};font-size:13px;}}")

    def set_serial(self, on): self._serial = on
    def prompt(self):
        if self._serial: return
        c = self.textCursor(); c.movePosition(QTextCursor.End)
        f1 = QTextCharFormat(); f1.setForeground(QColor(C['green'])); f1.setFontFamily("Cascadia Code")
        c.insertText(f"  {self.user}", f1)
        f2 = QTextCharFormat(); f2.setForeground(QColor(C['cyan']))
        c.insertText(" ❯ ", f2)
        self.setTextCursor(c); self.pp = c.position()

    def out(self, text, color=None):
        c = self.textCursor(); c.movePosition(QTextCursor.End)
        f = QTextCharFormat(); f.setFontFamily("Cascadia Code")
        f.setForeground(QColor(color or C['t2']))
        c.insertText(text, f); self.setTextCursor(c); self.ensureCursorVisible()

    def serial_char(self, ch):
        c = self.textCursor(); c.movePosition(QTextCursor.End)
        f = QTextCharFormat(); f.setFontFamily("Cascadia Code")
        f.setForeground(QColor(self._color))
        c.insertText(ch, f); self.setTextCursor(c); self.ensureCursorVisible()
        self.pp = c.position()

    def set_ansi(self, code):
        m = {30:C['t3'],31:C['red'],32:C['green'],33:C['orange'],34:C['blue'],
             35:C['purple'],36:C['cyan'],37:C['t1'],90:C['t3'],91:C['red'],
             92:C['green'],93:C['orange'],94:C['blue'],95:C['purple'],96:C['cyan'],97:C['t1']}
        self._color = m.get(code, C['t1'])

    def ai_out(self, text):
        c = self.textCursor(); c.movePosition(QTextCursor.End)
        f1 = QTextCharFormat(); f1.setForeground(QColor(C['cyan'])); f1.setFontFamily("Cascadia Code")
        c.insertText("\n  SwanOS AI ❯ ", f1)
        f2 = QTextCharFormat(); f2.setForeground(QColor(C['t1'])); f2.setFontFamily("Cascadia Code")
        c.insertText(text + "\n", f2)
        self.setTextCursor(c); self.ensureCursorVisible()

    def keyPressEvent(self, e):
        c = self.textCursor()
        if c.position() < self.pp: c.movePosition(QTextCursor.End); self.setTextCursor(c)
        if e.key() in (Qt.Key_Return, Qt.Key_Enter):
            c.movePosition(QTextCursor.End); self.setTextCursor(c)
            if self._serial:
                c.setPosition(self.pp); c.movePosition(QTextCursor.End, QTextCursor.KeepAnchor)
                self.command_entered.emit(c.selectedText())
                self.pp = self.textCursor().position()
            else:
                c.setPosition(self.pp); c.movePosition(QTextCursor.End, QTextCursor.KeepAnchor)
                cmd = c.selectedText().strip(); self.out("\n")
                if cmd: self.hist.append(cmd); self.hi = len(self.hist); self.command_entered.emit(cmd)
                else: self.prompt()
        elif e.key() == Qt.Key_Backspace:
            if c.position() > self.pp:
                if self._serial: self.command_entered.emit('\b')
                super().keyPressEvent(e)
        elif e.key() == Qt.Key_Up and not self._serial and self.hist and self.hi > 0:
            self.hi -= 1; self._rep(self.hist[self.hi])
        elif e.key() == Qt.Key_Down and not self._serial:
            if self.hi < len(self.hist)-1: self.hi += 1; self._rep(self.hist[self.hi])
            else: self.hi = len(self.hist); self._rep("")
        else:
            if self._serial and e.text():
                for ch in e.text(): self.command_entered.emit(ch)
            super().keyPressEvent(e)

    def _rep(self, t):
        c = self.textCursor(); c.setPosition(self.pp)
        c.movePosition(QTextCursor.End, QTextCursor.KeepAnchor)
        f = QTextCharFormat(); f.setForeground(QColor(C['t1'])); f.setFontFamily("Cascadia Code")
        c.removeSelectedText(); c.insertText(t, f); self.setTextCursor(c)


# ══════════════════════════════════════════════════════════════
#  SIDEBAR
# ══════════════════════════════════════════════════════════════
class Sidebar(QWidget):
    def __init__(self, user="admin"):
        super().__init__()
        self.user = user; self.setMinimumWidth(240); self.setMaximumWidth(260)
        self._t0 = time.time(); self._build()
        self.ut = QTimer(self); self.ut.timeout.connect(self._tick); self.ut.start(1000)

    def _build(self):
        self.setStyleSheet(f"QWidget{{background:{C['bg1']};border-right:1px solid {C['brd']};}}")
        lo = QVBoxLayout(self); lo.setContentsMargins(0,0,0,0); lo.setSpacing(0)
        hd = QFrame(); hd.setFixedHeight(56)
        hd.setStyleSheet(f"QFrame{{background:{C['bg2']};border-bottom:1px solid {C['brd']};border-right:none;}}")
        hl = QHBoxLayout(hd); hl.setContentsMargins(16,0,16,0)
        ic = QLabel("🦢"); ic.setStyleSheet("font-size:22px;background:transparent;border:none;"); hl.addWidget(ic)
        b = QLabel("Swan OS"); b.setStyleSheet(f"color:{C['t1']};font-size:16px;font-weight:300;letter-spacing:2px;background:transparent;border:none;")
        hl.addWidget(b); hl.addStretch()
        v = QLabel("v2.0"); v.setStyleSheet(f"color:{C['t3']};font-size:10px;background:transparent;border:none;"); hl.addWidget(v)
        lo.addWidget(hd)
        ct = QWidget(); ct.setStyleSheet("border:none;background:transparent;")
        cl = QVBoxLayout(ct); cl.setContentsMargins(16,16,16,16); cl.setSpacing(16)
        cl.addWidget(self._sec("SYSTEM"))
        self._row("👤", "User", self.user, cl)
        self._row("🤖", "AI", "Groq LLM", cl)
        self.up_v = self._row("⏱️", "Uptime", "0h 0m 0s", cl)
        sw = QWidget(); sw.setStyleSheet("background:transparent;border:none;")
        sh = QHBoxLayout(sw); sh.setContentsMargins(0,0,0,0)
        self.sd = QLabel("●"); self.sd.setStyleSheet(f"color:{C['green']};font-size:10px;background:transparent;border:none;"); sh.addWidget(self.sd)
        self.st = QLabel("Online"); self.st.setStyleSheet(f"color:{C['green']};font-size:11px;background:transparent;border:none;"); sh.addWidget(self.st)
        sh.addStretch(); cl.addWidget(sw)
        cl.addWidget(self._sec("QUICK ACTIONS"))
        for a, co in [("ask <question>",C['cyan']),("help",C['green']),("ls",C['orange']),("status",C['purple'])]:
            l = QLabel(f"  ❯  {a}")
            l.setStyleSheet(f"color:{co};font-size:11px;font-family:{FONT_MONO};background:transparent;border:none;padding:2px 0;")
            cl.addWidget(l)
        cl.addStretch(); lo.addWidget(ct)

    def _sec(self, t):
        l = QLabel(t)
        l.setStyleSheet(f"color:{C['t3']};font-size:10px;font-weight:700;letter-spacing:2px;"
                       f"padding-bottom:6px;border-bottom:1px solid {C['brd']};background:transparent;")
        return l

    def _row(self, icon, key, val, parent):
        w = QWidget(); w.setStyleSheet("background:transparent;border:none;")
        h = QHBoxLayout(w); h.setContentsMargins(0,0,0,0); h.setSpacing(8)
        i = QLabel(icon); i.setStyleSheet("font-size:13px;background:transparent;border:none;"); h.addWidget(i)
        k = QLabel(key); k.setStyleSheet(f"color:{C['t3']};font-size:11px;background:transparent;border:none;"); h.addWidget(k)
        vl = QLabel(val); vl.setStyleSheet(f"color:{C['t1']};font-size:11px;background:transparent;border:none;")
        h.addWidget(vl); h.addStretch(); parent.addWidget(w); return vl

    def _tick(self):
        e = int(time.time()-self._t0); self.up_v.setText(f"{e//3600}h {(e%3600)//60}m {e%60}s")

    def set_conn(self, on):
        if on:
            self.sd.setStyleSheet(f"color:{C['green']};font-size:10px;background:transparent;border:none;")
            self.st.setText("Connected"); self.st.setStyleSheet(f"color:{C['green']};font-size:11px;background:transparent;border:none;")
        else:
            self.sd.setStyleSheet(f"color:{C['orange']};font-size:10px;background:transparent;border:none;")
            self.st.setText("Demo"); self.st.setStyleSheet(f"color:{C['orange']};font-size:11px;background:transparent;border:none;")


# ══════════════════════════════════════════════════════════════
#  MAIN DESKTOP
# ══════════════════════════════════════════════════════════════
class Desktop(QWidget):
    def __init__(self, user="admin", demo=False, worker=None):
        super().__init__()
        self.user = user; self.demo = demo; self.worker = worker
        self._build()
        if worker:
            worker.data_received.connect(self._on_data)
            worker.llm_query_received.connect(self._on_llm)
            worker.connection_status.connect(self.sidebar.set_conn)

    def _build(self):
        lo = QHBoxLayout(self); lo.setContentsMargins(0,0,0,0); lo.setSpacing(0)
        self.sidebar = Sidebar(self.user)
        if self.demo: self.sidebar.set_conn(False)
        lo.addWidget(self.sidebar)
        main = QWidget(); main.setStyleSheet(f"background:{C['bg0']};")
        ml = QVBoxLayout(main); ml.setContentsMargins(0,0,0,0); ml.setSpacing(0)
        tb = QFrame(); tb.setFixedHeight(40)
        tb.setStyleSheet(f"QFrame{{background:{C['bg2']};border-bottom:1px solid {C['brd']};}}")
        tl = QHBoxLayout(tb); tl.setContentsMargins(16,0,16,0)
        tab = QLabel("◉  Terminal")
        tab.setStyleSheet(f"color:{C['cyan']};font-size:12px;font-weight:600;padding:4px 12px;"
                         f"background:{C['bg0']};border-radius:6px;border:1px solid {C['brd']};")
        tl.addWidget(tab); tl.addStretch()
        cd = QLabel("●"); cd.setStyleSheet(f"color:{C['green'] if not self.demo else C['orange']};font-size:8px;"); tl.addWidget(cd)
        cl = QLabel("VirtualBox" if not self.demo else "Demo Mode")
        cl.setStyleSheet(f"color:{C['t3']};font-size:10px;"); tl.addWidget(cl)
        ml.addWidget(tb)
        self.term = Terminal(self.user)
        if not self.demo and self.worker: self.term.set_serial(True)
        ml.addWidget(self.term)
        sb = QFrame(); sb.setFixedHeight(28)
        sb.setStyleSheet(f"QFrame{{background:{C['bg2']};border-top:1px solid {C['brd']};}}")
        sl = QHBoxLayout(sb); sl.setContentsMargins(12,0,12,0)
        for t, co in [("🦢 SwanOS",C['t1']),("│",C['brd']),("Groq LLM",C['t3']),("│",C['brd']),("x86",C['t3'])]:
            l = QLabel(t); l.setStyleSheet(f"color:{co};font-size:10px;"); sl.addWidget(l)
        sl.addStretch()
        self.stm = QLabel(""); self.stm.setStyleSheet(f"color:{C['t3']};font-size:10px;"); sl.addWidget(self.stm)
        self.tt = QTimer(self); self.tt.timeout.connect(lambda: self.stm.setText(datetime.now().strftime("%H:%M:%S")))
        self.tt.start(1000); self.stm.setText(datetime.now().strftime("%H:%M:%S"))
        ml.addWidget(sb); lo.addWidget(main)
        self.term.command_entered.connect(self._cmd)
        if self.demo:
            self.term.out(f"\n  Welcome to Swan OS, {self.user}!\n", C['cyan'])
            self.term.out(f"  Type help for commands, ask <question> to talk to AI.\n\n", C['t2'])
            self.term.prompt()

    def _on_data(self, data):
        if data.startswith('\x1b[') and data.endswith('m'):
            try: self.term.set_ansi(int(data[2:-1]))
            except: pass
        else: self.term.serial_char(data)

    def _on_llm(self, query):
        def go():
            r = call_llm(query)
            if self.worker: self.worker.send_response(r)
        threading.Thread(target=go, daemon=True).start()

    def _cmd(self, cmd):
        if not self.demo and self.worker:
            if cmd == '\b': self.worker.send_char('\b')
            elif len(cmd) == 1: self.worker.send_char(cmd)
            else:
                for ch in cmd: self.worker.send_char(ch)
                self.worker.send_char('\n')
            return
        parts = cmd.split(None, 1); c = parts[0].lower() if parts else ""; a = parts[1] if len(parts)>1 else ""
        t = self.term
        if c == "help":
            for tx, co in [("\n  SwanOS Commands\n",C['cyan']),("  ─────────────────────────\n",C['brd']),
                ("  ask <question>   Ask AI\n",C['t1']),("  ls / cat / write / mkdir / rm\n",C['t1']),
                ("  calc <expr>  /  echo <text>\n",C['t1']),("  help / status / whoami / time\n",C['t1']),
                ("  clear / shutdown\n\n",C['t1'])]: t.out(tx, co)
        elif c == "clear": t.clear(); t.prompt(); return
        elif c == "ask":
            if not a: t.out("  Usage: ask <question>\n", C['red'])
            else:
                t.out("  ⏳ Thinking...\n", C['t3']); QApplication.processEvents()
                t.ai_out(call_llm(a))
        elif c == "status":
            e=int(time.time()-self.sidebar._t0)
            t.out(f"\n  User: {self.user} | Arch: x86 | LLM: Groq\n", C['t1'])
            t.out(f"  Uptime: {e//3600}h {(e%3600)//60}m {e%60}s | ONLINE\n\n", C['green'])
        elif c == "whoami": t.out(f"  {self.user} @ SwanOS v2.0\n", C['green'])
        elif c == "echo": t.out(f"  {a}\n", C['t1'])
        elif c == "ls": t.out("  📁 documents/  📁 programs/  📄 readme.txt\n", C['cyan'])
        elif c == "cat":
            if a == "readme.txt": t.out("  Welcome to SwanOS! A bare-metal AI-powered OS.\n", C['t1'])
            else: t.out(f"  Not found: {a}\n", C['red'])
        elif c == "calc":
            try:
                ok = set("0123456789+-*/().% ")
                if a and all(x in ok for x in a): t.out(f"  = {eval(a)}\n", C['cyan'])
                else: t.out("  Invalid\n", C['red'])
            except: t.out("  Error\n", C['red'])
        elif c == "time":
            e=int(time.time()-self.sidebar._t0); t.out(f"  {e//3600}h {(e%3600)//60}m {e%60}s\n", C['t1'])
        elif c == "shutdown": t.out("\n  Goodbye.\n", C['orange']); QTimer.singleShot(1000, QApplication.quit); return
        else: t.out(f"  Unknown: {c}. Type 'help'\n", C['red'])
        t.out("\n"); t.prompt()


# ══════════════════════════════════════════════════════════════
#  MAIN WINDOW
# ══════════════════════════════════════════════════════════════
class SwanOS(QMainWindow):
    def __init__(self, demo=False, pipe=None):
        super().__init__()
        self.demo = demo; self.pipe = pipe
        self.worker = None; self.wthread = None
        self.setWindowTitle("Swan OS"); self.setMinimumSize(1100,700); self.resize(1280,780)
        self.setStyleSheet(f"QMainWindow{{background:{C['bg0']};}}")
        self.stack = QStackedWidget(); self.setCentralWidget(self.stack)
        self.splash = BootSplash(); self.splash.boot_finished.connect(self._login)
        self.stack.addWidget(self.splash)
        self.logscr = LoginScreen(); self.logscr.login_success.connect(self._go)
        self.stack.addWidget(self.logscr)
        self.desk = None; self.stack.setCurrentWidget(self.splash)
        if pipe and not demo: self._start_serial(pipe)

    def _start_serial(self, pipe):
        self.worker = SerialWorker(pipe)
        self.wthread = QThread()
        self.worker.moveToThread(self.wthread)
        self.wthread.started.connect(self.worker.start_reading)
        self.wthread.start()

    def _login(self): self.stack.setCurrentWidget(self.logscr); self.logscr.ui.setFocus()

    def _go(self, user):
        self.desk = Desktop(user, self.demo, self.worker)
        self.stack.addWidget(self.desk); self.stack.setCurrentWidget(self.desk)
        self.desk.term.setFocus()
        if self.worker and not self.demo:
            def send():
                time.sleep(0.5)
                for ch in user: self.worker.send_char(ch)
                self.worker.send_char('\n')
                time.sleep(0.3)
                for ch in "admin": self.worker.send_char(ch)
                self.worker.send_char('\n')
                time.sleep(0.3); self.worker.send_char('2')
            threading.Thread(target=send, daemon=True).start()

    def closeEvent(self, e):
        if self.worker: self.worker.stop()
        if self.wthread: self.wthread.quit(); self.wthread.wait(2000)
        e.accept()


def main():
    parser = argparse.ArgumentParser(description="SwanOS")
    parser.add_argument("--demo", action="store_true")
    parser.add_argument("--pipe", help="Named pipe path, e.g. \\\\.\\pipe\\swanos")
    args = parser.parse_args()
    app = QApplication(sys.argv); app.setApplicationName("SwanOS")
    pal = QPalette()
    pal.setColor(QPalette.Window, QColor(C['bg0']))
    pal.setColor(QPalette.WindowText, QColor(C['t1']))
    pal.setColor(QPalette.Base, QColor(C['bg1']))
    pal.setColor(QPalette.Text, QColor(C['t1']))
    pal.setColor(QPalette.Highlight, QColor(C['cyan']))
    pal.setColor(QPalette.HighlightedText, QColor(C['bg0']))
    app.setPalette(pal)
    w = SwanOS(demo=args.demo or not args.pipe, pipe=args.pipe)
    w.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()
