import os, shlex, subprocess, signal, wave, json
from pynput import keyboard
from vosk import Model, KaldiRecognizer

RATE = 16000
WAV = "/tmp/ptt.wav"
MODEL_DIR = os.path.expanduser("~/models/vosk-fr")
MODEL_NAME = os.getenv("MODEL_NAME", "gpt-5-mini")
API_KEY = os.getenv("OPENAI_API_KEY")

def say_fr(text):
    cmd = 'espeak-ng -v fr {} --stdout | paplay'.format(shlex.quote(text))
    subprocess.run(cmd, shell=True)

def record_start():
    return subprocess.Popen(["arecord","-D","pulse","-r",str(RATE),"-f","S16_LE","-q","-t","wav",WAV])

def record_stop(p):
    try:
        p.send_signal(signal.SIGINT); p.wait(timeout=3)
    except Exception:
        p.kill()

def asr(path):
    if not os.path.isdir(MODEL_DIR): return ""
    m = Model(MODEL_DIR)
    rec = KaldiRecognizer(m, RATE)
    # ré-échantillonne si besoin
    with wave.open(path,"rb") as wf:
        if wf.getframerate()!=RATE or wf.getnchannels()!=1:
            tmp="/tmp/ptt16k.wav"
            subprocess.run(["ffmpeg","-v","error","-i",path,"-ar",str(RATE),"-ac","1",tmp,"-y"])
            path=tmp
    with wave.open(path,"rb") as wf:
        while True:
            data = wf.readframes(4000)
            if not data: break
            rec.AcceptWaveform(data)
    try:
        return (json.loads(rec.FinalResult()).get("text") or "").strip()
    except: return ""

def call_openai(prompt):
    if not API_KEY: return None, "no_api"
    import requests, json as js
    r = requests.post(
        "https://api.openai.com/v1/chat/completions",
        headers={"Authorization":"Bearer "+API_KEY, "Content-Type":"application/json"},
        data=js.dumps({
            "model": MODEL_NAME,
            "messages":[
                {"role":"system","content":"Assistant vocal concis, en français."},
                {"role":"user","content":prompt}],
            "temperature":0.4,"max_tokens":300
        }), timeout=60
    )
    if r.status_code!=200: return None, f"api_err {r.status_code}"
    return r.json()["choices"][0]["message"]["content"].strip(), None

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
        if not os.path.exists(WAV) or os.path.getsize(WAV)<1000:
            say_fr("Rien capté."); return
        txt = asr(WAV)
        print("ASR:", txt)
        if not txt:
            say_fr("Je n'ai pas compris."); return
        reply, err = call_openai(txt)
        if err=="no_api":
            reply = f"Tu as dit : {txt}."
        elif err:
            reply = "Problème réseau."
        print("BOT:", reply)
        say_fr(reply)

if __name__=="__main__":
    say_fr("Espace prêt.")
    with keyboard.Listener(on_press=on_press, on_release=on_release) as L:
        L.join()
