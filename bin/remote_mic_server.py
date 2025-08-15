from flask import Flask, request, make_response
import subprocess, time, os
app = Flask(__name__)

HTML = """<!doctype html>
<html lang="fr"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Assistant vocal</title>
<style>
 body{font-family:system-ui,Roboto,Arial;margin:20px}
 h2{margin:0 0 10px}
 .row{display:flex;gap:10px;flex-wrap:wrap}
 button{font-size:18px;padding:12px 16px;border:0;border-radius:12px;background:#0ea5e9;color:#fff}
 #stt{background:#22c55e}
 #status{margin-top:12px;font-family:ui-monospace,monospace;white-space:pre-wrap}
 input[type=text]{flex:1;min-width:220px;font-size:18px;padding:10px;border-radius:10px;border:1px solid #ccc}
</style>
<h2>Assistant vocal</h2>
<div class="row">
  <button id="stt" title="Reco côté Chrome (recommandé)">Dicter (Chrome)</button>
  <button id="btn" title="Ancien enregistrement">Enregistrer & envoyer</button>
</div>
<div class="row" style="margin-top:10px">
  <input id="txt" type="text" placeholder="…ou tape ici et ENVOI" />
  <button id="send">Envoyer</button>
</div>
<pre id="status">Prêt.</pre>
<script>
const st  = document.getElementById('status');
const btn = document.getElementById('btn');
const stt = document.getElementById('stt');
const txt = document.getElementById('txt');
const send= document.getElementById('send');

// --- Envoi texte brut vers /upload_text ---
async function sendText(toSend){
  try{
    st.textContent = "Envoi du texte…";
    await fetch('/upload_text',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({text:toSend||''})});
    st.textContent = "Envoyé. Tu peux recommencer.";
  }catch(e){ st.textContent = "Erreur envoi: "+e; }
}
send.addEventListener('click', ()=>{ const v=txt.value.trim(); if(v){ sendText(v); txt.value=''; } });
txt.addEventListener('keydown', (e)=>{ if(e.key==='Enter'){ send.click(); } });

// --- Dicter (Chrome) via Web Speech ---
stt.addEventListener('click', ()=>{
  const Rec = window.SpeechRecognition || window.webkitSpeechRecognition;
  if(!Rec){ st.textContent = "Web Speech non supporté sur ce navigateur."; return; }
  const rec = new Rec();
  rec.lang='fr-FR'; rec.interimResults=false; rec.maxAlternatives=1;
  stt.disabled=true; st.textContent="Parle…";
  rec.onresult = (e)=>{
    const text = e.results[0][0].transcript || "";
    st.textContent = "Texte reconnu: " + text + " — envoi…";
    sendText(text).finally(()=>{ stt.disabled=false; });
  };
  rec.onerror = (e)=>{ st.textContent = "Erreur STT: " + e.error; stt.disabled=false; };
  rec.onend   = ()=>{ if(stt.disabled) stt.disabled=false; };
  rec.start();
});

// --- Ancien mode (enregistrement fichier -> /upload). Utile en dépannage ---
function typeOk(t){return window.MediaRecorder&&MediaRecorder.isTypeSupported(t);}
function pickMime(){const c=['audio/webm;codecs=opus','audio/ogg;codecs=opus','audio/webm']; for(const x of c){if(typeOk(x)) return x;} return '';}
function mimeExt(m){if(m.includes('ogg')) return 'ogg'; if(m.includes('webm')) return 'webm'; return 'webm';}
let rec,chunks=[],stream,mime='';
async function startRec(){
  try{
    st.textContent='Demande du micro…';
    stream=await navigator.mediaDevices.getUserMedia({audio:{channelCount:1,echoCancellation:true,noiseSuppression:true,autoGainControl:true}});
    mime=pickMime(); const opts = mime? {mimeType:mime,audioBitsPerSecond:128000}:{audioBitsPerSecond:128000};
    rec=new MediaRecorder(stream,opts); chunks=[];
    rec.ondataavailable=e=>{ if(e.data&&e.data.size) chunks.push(e.data); };
    rec.onstart=()=>{ st.textContent='Enregistrement…'; btn.textContent='Arrêter et envoyer'; };
    rec.onstop=async ()=>{
      const blob=new Blob(chunks,{type:mime||'audio/webm'}); const ext=mimeExt(blob.type);
      st.textContent=`Encodage: ${blob.type} • ${blob.size} bytes. Envoi…`;
      try{ await fetch(`/upload?ext=${ext}`,{method:'POST',body:blob}); st.textContent='Envoyé. Tu peux recommencer.'; }
      catch(e){ st.textContent='Erreur envoi: '+e; }
      finally{ btn.textContent='Enregistrer & envoyer'; stream.getTracks().forEach(t=>t.stop()); }
    };
    rec.start();
  }catch(e){ st.textContent='Erreur: '+e; }
}
btn.addEventListener('click', ()=>{ if(rec&&rec.state==='recording') rec.stop(); else startRec(); });
</script>
</html>"""




@app.route("/")
def index(): return make_response(HTML, 200)

@app.route("/upload", methods=["POST"])
def upload():
  ext = request.args.get("ext","webm")
  raw = f"/tmp/ptt_phone.{ext}"
  wav = "/tmp/ptt.wav"
  open(raw,"wb").write(request.data)
  subprocess.run(["ffmpeg","-v","error","-y","-i",raw,"-ar","16000","-ac","1",wav])
  open("/tmp/ptt.ready","w").write(str(time.time()))
  return ("OK",200)

if __name__ == "__main__":
  cert=os.path.expanduser("~/voice-assistant/bin/cert.pem")
  key =os.path.expanduser("~/voice-assistant/bin/key.pem")
  if os.path.exists(cert) and os.path.exists(key):
    app.run(host="0.0.0.0", port=5443, ssl_context=(cert,key))
  else:
    app.run(host="0.0.0.0", port=5000)
