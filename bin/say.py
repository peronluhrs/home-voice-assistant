import shlex, subprocess

def say_fr(text: str):
    # Utilise espeak-ng -> pipe vers paplay sur la sortie par défaut
    cmd = f'espeak-ng -v fr {shlex.quote(text)} --stdout | paplay'
    subprocess.run(cmd, shell=True, check=True)

if __name__ == "__main__":
    say_fr("Salut Vincent, test de synthèse vocale.")
