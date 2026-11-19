# LeveMaster

Plugin VST3 de masterização com cadeia fixa e interface minimalista (só sliders e texto, sem gráficos pesados).

## Cadeia de sinal

Entrada → EQ → Compressor → Saturação → Estéreo → Push → Limiter → Saída

| Estágio | Parâmetros |
|---|---|
| Entrada | -12 a +12 dB |
| EQ | Grave (shelf 100 Hz), Médio (sino 2,5 kHz), Agudo (shelf 10 kHz), cada um ±6/±4/±6 dB |
| Compressor | Threshold, Ratio, Ataque, Soltura, Mix paralelo. Detecção linkada (L/R), joelho de 6 dB, makeup automático (metade da redução estática) |
| Saturação | Drive 0–10 (tanh suave, ganho unitário em sinais baixos), oversampling 2x |
| Estéreo | Largura 0–200 % (mid/side) |
| Limiter | Push (0–12 dB empurrando para dentro do limiter), Teto, Soltura. Brickwall com lookahead de ~1,5 ms |
| Saída | -12 a +12 dB |

Cada estágio tem bypass próprio e há um bypass geral. Duplo clique num slider volta ao padrão.
A latência é constante e reportada ao host (lookahead do limiter + oversampling), mesmo com estágios em bypass.

Observação: o limiter garante pico de **amostra** abaixo do teto, não true-peak entre amostras.
Deixe o teto em -1 dB (padrão) para entrega.

## Como obter o .vst3 pronto (sem instalar compilador)

1. Crie um repositório no GitHub e suba esta pasta inteira.
2. Aba **Actions** → rode o workflow **Build VST3** (ele roda sozinho a cada push).
3. Ao terminar, baixe o artefato `LeveMaster-windows-VST3` ou `LeveMaster-macos-VST3`.
4. Copie a pasta `LeveMaster.vst3` para:
   - Windows: `C:\Program Files\Common Files\VST3\`
   - macOS: `~/Library/Audio/Plug-Ins/VST3/`
5. Reescaneie plugins na sua DAW.

macOS: se o sistema bloquear o plugin baixado, rode no Terminal:
`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/LeveMaster.vst3`

## Compilar localmente

Requisitos: CMake 3.22+, Git e um compilador C++17 (Visual Studio 2022, Xcode ou GCC/Clang).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

O JUCE 8.0.8 é baixado automaticamente. O resultado fica em `build/LeveMaster_artefacts/.../VST3/`.
Para copiar direto para a pasta de plugins do sistema, adicione `-DLEVE_COPY_AFTER_BUILD=ON`
(no Windows, rode o terminal como administrador).

Linux: instale antes `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev`.

## Testes do DSP

```
g++ -std=c++17 -O2 tests/dsp_test.cpp -o dsp_test && ./dsp_test
```

## Estrutura

- `Source/Dsp.h` — EQ, compressor, limiter e saturador (C++ puro, sem JUCE)
- `Source/PluginProcessor.*` — parâmetros e cadeia de áudio
- `Source/PluginEditor.*` — interface
