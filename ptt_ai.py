import os, sys, json, shlex, subprocess, signal, time, wave
from vosk import Model, KaldiRecognizer

MODEL_DIR = os.path.expanduser("~/models/vosk-fr")
RATE = 16000
WAV = "/tmp/ptt.wav"
MODEL_NAME = os.getenv("MODEL_NAME", "gpt-5-mini")
API_KEY = os.getenv("OPENAI_API_KEY")

def say_fr(text):
    cmd = 'espeak-ng -v fr {} --stdout | paplay'.format(shlex.quote(text))
    subprocess.run(cmd, shell=True)

def record_until_enter():
    print("[REC] Parle... (Entrée pour arrêter)")
    proc = subprocess.Popen(["arecord","-D","pulse","-r",str(RATE),"-f","S16_LE","-q","-t","wav",WAV])
    try:
        input()  # attendre Entrée
    except KeyboardInterrupt:
        pass
    try:
        proc.send_signal(signal.SIGINT)
        proc.wait(timeout=3)
    except Exception:
        proc.kill()

def transcribe_fr(path):
    if not os.path.isdir(MODEL_DIR):
        return ""
    m = Model(MODEL_DIR)
    rec = KaldiRecognizer(m, RATE)
    with wave.open(path, "rb") as wf:
        if wf.getframerate() != RATE or wf.getnchannels() != 1:
            # ré-échantillonne proprement si besoin
            tmp = "/tmp/ptt16k.wav"
            subprocess.run(["ffmpeg","-v","error","-i",path,"-ar",str(RATE),"-ac","1",tmp,"-y"])
            path = tmp
        with wave.open(path, "rb") as wf2:
            while True:
                data = wf2.readframes(4000)
                if len(data) == 0: break
                rec.AcceptWaveform(data)
    try:
        res = json.loads(rec.FinalResult())
        return (res.get("text") or "").strip()
    except Exception:
        return ""

def call_openai(prompt):
    if not API_KEY:
        return None, "Pas de clé API (OPENAI_API_KEY). Réponse locale."
    import requests
    url = "https://api.openai.com/v1/chat/completions"
    headers = {"Authorization": "Bearer {}".format(API_KEY), "Content-Type":"application/json"}
    body = {
        "model": MODEL_NAME,
        "messages": [
            {"role":"system","content":"Tu es un assistant vocal domestique, concis et clair, en français."},
            {"role":"user","content": prompt}
        ],
        "temperature": 0.4,
        "max_tokens": 300
    }
    r = requests.post(url, headers=headers, data=json.dumps(body), timeout=60)
    if r.status_code != 200:
        return None, "Erreur API {}: {}".format(r.status_code, r.text[:200])
    out = r.json()["choices"][0]["message"]["content"].strip()
    return out, None

def main():
    print("=== Assistant PTT ===")
    print("Entrée = DÉMARRER l'enregistrement ; Entrée = ARRÊTER ; Ctrl+C = quitter.")
    say_fr("Assistant prêt.")
    while True:
        try:
            input("Appuie Entrée pour parler...")
        except KeyboardInterrupt:
            print("\nBye.")
            break
        record_until_enter()
        if not os.path.exists(WAV) or os.path.getsize(WAV) < 1000:
            say_fr("Je n'ai rien capté.")
            continue
        txt = transcribe_fr(WAV)
        print("ASR:", txt)
        if not txt:
            say_fr("Je n'ai pas compris.")
            continue
        reply, err = call_openai(txt)
        if err:
            print(err)
            reply = "Tu as dit : {}. (Mode local, pas de clé API.)".format(txt)
        print("BOT:", reply)
        say_fr(reply)

if __name__ == "__main__":
    main()
