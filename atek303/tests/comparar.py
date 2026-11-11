"""Compara dos WAV float32 mono: nivel, diferencia y parecido del espectro por paso."""
import sys, numpy as np
def leer(p):
    d = open(p, 'rb').read(); return np.frombuffer(d[44:], dtype=np.float32).astype(np.float64)
a, b = leer(sys.argv[1]), leer(sys.argv[2])
n = min(len(a), len(b)); a, b = a[:n], b[:n]
rms = lambda x: np.sqrt(np.mean(x * x))
print(f"nivel: ref {rms(a):.4f} rms, pico {np.max(np.abs(a)):.3f} | port {rms(b):.4f} rms, pico {np.max(np.abs(b)):.3f}")
print(f"diferencia: {20*np.log10(rms(a-b)/rms(a)+1e-12):.1f} dB respecto a la referencia")
print(f"correlación: {np.corrcoef(a, b)[0,1]:.4f}")
step = int(44100*60/125/4); fr = []
for s in range(0, n - step, step):
    A = np.abs(np.fft.rfft(a[s:s+step] * np.hanning(step))); B = np.abs(np.fft.rfft(b[s:s+step] * np.hanning(step)))
    la, lb = 20*np.log10(A+1e-9), 20*np.log10(B+1e-9); m = la > la.max() - 60
    fr.append(np.mean(np.abs(la[m]-lb[m])))
print(f"espectro: diferencia media {np.mean(fr):.2f} dB por paso (peor paso {np.max(fr):.2f} dB)")
