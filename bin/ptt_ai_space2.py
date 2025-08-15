import os, shlex, subprocess, signal, wave, json, time, csv, pathlib
from pynput import keyboard
from vosk import Model, KaldiRecognizer

RATE = 16000
WAV_RAW = "/tmp/ptt.wav"
WAV_CLEAN = "/tmp/ptt_clean.wav"
MODEL_DIR = os.path.expanduser("~/models/vosk-fr")

# === Config API & coût (tu peux changer via variables d'env) ===
MODEL_NAME = os.getenv("MODEL_NAME", "gpt-5-mini")
API_KEY = os.getenv("OPENAI_API_KEY")
USAGE_CSV = os.path.expanduser("~/ai-usage.csv")
# Prix par 1M tokens (si tu veux une estimation; sinon laisse vide)
PRICE_IN  = float(os.getenv("PRICE_IN",  "0"))   # ex: 0.25 pour mini
PRICE_OUT = float(os.getenv("PRICE_OUT", "0"))   # ex: 2.0  pour mini
CURRENCY  = os.getenv("CURRENCY", "USD")

def say_fr(text):
    cmd = 'espeak-ng -v fr {} --stdout | paplay'.format(shlex.quote(text))
    subprocess.run(cmd, shell=True)

def record_start():
    return subprocess.Popen(["arecord","-D","pulse","-r",str(RATE),"-f","S16_LE","-q","-t","wav",WAV_RAW])

def record_stop(p):
    try:
        p.send_signal(signal.SIGINT); p.wait(timeout=3)
    except Exception:
        p.kill()

def denoise_and_trim(src, dst):
    # coupe les silences au début/fin (~ -35 dB) et force 16k mono
    subprocess.run([
        "ffmpeg","-v","error","-y",
        "-i", src,
        "-af","silenceremove=start_periods=1:start_threshold=-35dB:start_silence=0.2:stop_periods=1:stop_threshold=-35dB:stop_silence=0.2",
        "-ar", str(RATE), "-ac","1",
        dst
    ])

def asr(path):
    if not os.path.isdir(MODEL_DIR): return ""
    m = Model(MODEL_DIR)
    rec = KaldiRecognizer(m, RATE)
    with wave.open(path,"rb") as wf:
        while True:
            data = wf.readframes(4000)
            if not data: break
            rec.AcceptWaveform(data)
    try:
        return (json.loads(rec.FinalResult()).get("text") or "").strip()
    except: return ""

def call_openai(prompt):
    if not API_KEY: return None, "no_api", None
    import requests, json as js
    body = {
        "model": MODEL_NAME,
        "messages":[
            {"role":"system","content":"Assistant vocal concis, en français."},
            {"role":"user","content":prompt}],
        "temperature":0.4, "max_tokens":300
    }
    r = requests.post(
        "https://api.openai.com/v1/chat/completions",
        headers={"Authorization":"Bearer "+API_KEY,"Content-Type":"application/json"},
        data=js.dumps(body), timeout=60
    )
    if r.status_code!=200:
        return None, f"api_err {r.status_code}", None
    j = r.json()
    out = j["choices"][0]["message"]["content"].strip()
    usage = j.get("usage", {})
    return out, None, usage

def log_usage(usage):
    if not usage: return
    pathlib.Path(USAGE_CSV).parent.mkdir(parents=True, exist_ok=True)
    exists = pathlib.Path(USAGE_CSV).exists()
    row = {
        "ts": time.strftime("%Y-%m-%d %H:%M:%S"),
        "model": MODEL_NAME,
        "prompt_tokens": usage.get("prompt_tokens") or usage.get("input_tokens") or 0,
        "completion_tokens": usage.get("completion_tokens") or usage.get("output_tokens") or 0,
        "total_tokens": usage.get("total_tokens") or ( (usage.get("prompt_tokens") or 0) + (usage.get("completion_tokens") or 0) )
    }
    # coût estimé si tu as mis PRICE_IN / PRICE_OUT
    try:
        cost = (row["prompt_tokens"]*PRICE_IN + row["completion_tokens"]*PRICE_OUT)/1_000_000.0
    except Exception:
        cost = 0.0
    row["est_cost_"+CURRENCY] = f"{cost:.6f}"
    with open(USAGE_CSV,"a",newline="") as f:
        w = csv.DictWriter(f, fieldnames=row.keys())
        if not exists: w.writeheader()
        w.writerow(row)

rec_proc=None
pressed=False

def on_press(key):
    global rec_proc, pressed
    if key==keyboard.Key.space and not pressed:
        pressed=True
        print("[REC] (Espace maintenu) …")
        rec_proc = record_start()
        say_fr("Parle.")

def on_release(key):
    global rec_proc, pressed
    if key==keyboard.Key.space and pressed:
        pressed=False
        print("[STOP]")
        if rec_proc: record_stop(rec_proc)
        if not os.path.exists(WAV_RAW) or os.path.getsize(WAV_RAW) < 1000:
            say_fr("Rien capté."); return
        denoise_and_trim(WAV_RAW, WAV_CLEAN)
        wav = WAV_CLEAN if os.path.exists(WAV_CLEAN) and os.path.getsize(WAV_CLEAN)>0 else WAV_RAW
        txt = asr(wav)
        print("ASR:", txt)
        if not txt:
            say_fr("Je n'ai pas compris."); return
        reply, err, usage = call_openai(txt)
        if err=="no_api":
            reply = f"Tu as dit : {txt}."
        elif err:
            reply = "Problème réseau."
        print("BOT:", reply)
        say_fr(reply)
        log_usage(usage)

if __name__=="__main__":
    say_fr("Espace prêt.")
    from pynput import keyboard
    with keyboard.Listener(on_press=on_press, on_release=on_release) as L:
        L.join()
