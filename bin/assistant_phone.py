import os
import json
import vosk
from pathlib import Path
VOSK_MODEL_DIR = os.environ.get('VOSK_MODEL_DIR', str(Path.home()/'voice-assistant/models/vosk-fr-large'))
VOSK_SR=16000
VOSK_MODEL=None

import time, wave, json, shlex, subprocess
from pathlib import Path
from pathlib import Path
WB=os.environ.get('WHISPER_BIN', str(Path.home()/ 'whisper.cpp/wb.sh'))

RATE=16000; WAV="/tmp/ptt.wav"; READY="/tmp/ptt.ready"
VA=os.path.expanduser("~/voice-assistant")

# ---------- TTS : Pico par défaut ----------
def say_fr(text):
    text=(text or "").strip()
    if not text: return
    if not text.endswith((".","!","?")): text+="."
    wav="/tmp/tts_pico.wav"
    if subprocess.call(f'pico2wave -l fr-FR -w {shlex.quote(wav)} "{text}"', shell=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)==0 and os.path.exists(wav):
        subprocess.call(["paplay",wav], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return
    subprocess.call(f'espeak-ng -v fr "{text}" --stdout | paplay', shell=True)

# ---------- ASR : whisper.cpp (wrapper) ----------
def find_whisper_bin():
    cands = [
        os.environ.get("WHISPER_BIN"),
        os.path.expanduser("~/whisper.cpp/wb.sh"),
        os.path.expanduser("~/whisper.cpp/build/bin/whisper-cli"),
        os.path.expanduser("~/whisper.cpp/build/bin/main"),
    ]
    for c in cands:
        if c and os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    return None

WHISPER_BIN   = find_whisper_bin()
WHISPER_MODEL = os.environ.get("WHISPER_MODEL", os.path.expanduser("~/whisper.cpp/models/ggml-base.bin"))

def asr_whispercpp(wav):
    import subprocess, shlex
    try:
        p = subprocess.run([WB, wav], stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        if p.stderr.strip():
            print("WHISPER_ERR:", p.stderr.strip()[:500], flush=True)
        return p.stdout.strip()
    except FileNotFoundError:
        print("WHISPER_ERR: WB not found:", WB, flush=True)
        return ""
    except Exception as e:
        print("WHISPER_ERR: exception:", e, flush=True)
        return ""
def make_clean_wav(src, dst):
    filt="silenceremove=start_periods=1:start_duration=0.15:start_threshold=-35dB:stop_periods=1:stop_duration=0.15:stop_threshold=-35dB"
    r = subprocess.call(["ffmpeg","-v","error","-y","-i",src,"-af",filt,"-ar",str(RATE),"-ac","1",dst])
    if r!=0 or (not os.path.exists(dst)) or os.path.getsize(dst)<512:
        subprocess.call(["ffmpeg","-v","error","-y","-i",src,"-ar",str(RATE),"-ac","1",dst])

def chatgpt(prompt):
    API_KEY=os.getenv("OPENAI_API_KEY"); MODEL=os.getenv("MODEL_NAME","gpt-5-mini")
    if not API_KEY: return "Tu as dit : %s."%prompt
    import requests
    r = requests.post("https://api.openai.com/v1/chat/completions",
        headers={"Authorization":"Bearer "+API_KEY,"Content-Type":"application/json"},
        json={"model":MODEL,"messages":[
              {"role":"system","content":"Assistant vocal concis, en français."},
              {"role":"user","content":prompt}], "temperature":0.4, "max_tokens":300}, timeout=60)
    if r.status_code!=200: return "Tu as dit : %s."%prompt
    return r.json()["choices"][0]["message"]["content"].strip()

if __name__=="__main__":
    print("Assistant prêt. ASR:", "whisper.cpp" if WHISPER_BIN else "absent",
          "| TTS: PICO",
          "| BIN:", WHISPER_BIN or "<none>")
    say_fr("Assistant prêt.")
    last=0
    while True:
        try:
            t=os.path.getmtime(READY) if os.path.exists(READY) else 0
            if t>last and os.path.exists(WAV):
                last=t
                make_clean_wav(WAV, "/tmp/ptt_clean.wav")
                wav="/tmp/ptt_clean.wav" if (os.path.exists("/tmp/ptt_clean.wav") and os.path.getsize("/tmp/ptt_clean.wav")>512) else WAV
                txt=asr_whispercpp(wav)
                print("ASR:",txt)
                say_fr("Je n'ai pas compris.") if not txt else say_fr(chatgpt(txt))
            time.sleep(0.15)
        except KeyboardInterrupt:
            break


def asr_vosk(wav_in):
    global VOSK_MODEL
    import subprocess, wave
    # convertit en WAV 16k mono (rapide)
    wav16 = "/tmp/ptt_vosk.wav"
    subprocess.run(["ffmpeg","-v","error","-y","-i", wav_in, "-af", "highpass=f=120,lowpass=f=7000,afftdn=nr=12:nf=-25,compand=attacks=0.05:decays=0.5:points=-80/-80|-40/-20|-20/-6|0/-2:soft-knee=6:gain=8", "-ar", str(VOSK_SR), "-ac", "1", wav16], check=False)
    if not os.path.exists(wav16) or os.path.getsize(wav16) < 1000:
        print("VOSK_ERR: conversion échouée ou fichier trop petit", flush=True); return ""
    if VOSK_MODEL is None:
        try:
            print("VOSK: loading model…", flush=True)
            VOSK_MODEL = vosk.Model(VOSK_MODEL_DIR)
        except Exception as e:
            print("VOSK_ERR: model load:", e, flush=True); return ""
    rec = vosk.KaldiRecognizer(VOSK_MODEL, VOSK_SR)
    try:
        with wave.open(wav16, "rb") as wf:
            while True:
                data = wf.readframes(4000)
                if not data: break
                rec.AcceptWaveform(data)
        out = json.loads(rec.FinalResult()).get("text","").strip()
        return out
    except Exception as e:
        print("VOSK_ERR:", e, flush=True); return ""



@app.route("/upload_text", methods=["POST"])
def upload_text():
    from flask import request
    data = request.get_json(silent=True) or {}
    text = (data.get("text") or "").strip()
    print("ASR(TEXT):", text or "<VIDE>", flush=True)
    if not text:
        reply = "Je n'ai pas compris."
    else:
        reply = text  # ici tu peux brancher ton LLM si tu veux
    tts(reply)
    return ("OK", 200)

