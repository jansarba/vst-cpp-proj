# vst-cpp-proj

**Wtyczka Audio VST3 z Efektami (JUCE)**

Podstawowy łańcuch efektów w następującej kolejności:
1.  **Pitch Shifter** (-12, +12, +24 półtony) - 20/80
2.  **Delay** (z feedbackiem) - własny
3.  **Chorus** - 50/50
4.  **Reverb** - stockowy

**Jak zbudować?**

1.  Otwórz plik `.jucer` w Projucerze.
2.  Zapisz projekt dla swojego środowiska (np. Visual Studio 2022).
3.  Otwórz i skompiluj projekt w trybie "Release".
4.  Skompilowany plik `.vst3` znajdzie się w folderze `Builds/`.

Gotowy plik `.vst3` należy umieścić w standardowym folderze wtyczek VST3, aby był widoczny w programie DAW.
