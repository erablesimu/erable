# Fusions depuis Lichii

Chaque fusion qui modifie le traducteur de shaders oblige les joueurs à recompiler tous leurs
shaders au premier lancement (10 à 30 minutes pour Ocarina of Time) : l'empreinte du cache suit
les sources de `src/shader_recompiler/` (hors `glasm`/`glsl`), `src/video_core/native_lowering.*`,
`src/video_core/mkw_wave_workaround.h`, `src/video_core/shader_environment.*` et
`src/video_core/renderer_vulkan/pipeline_shader_emission.h` (voir `src/video_core/CMakeLists.txt`,
`emission_sources`).

Règle :

- Les commits de Lichii qui ne touchent pas ces fichiers peuvent être repris à chaque build.
- Ceux qui les touchent sont regroupés et repris au plus une fois par semaine, dans un build dont
  les notes préviennent que les shaders seront recompilés.
- Pour savoir si une fusion les touche :

  ```
  git diff --name-only HEAD lichii/nx2 -- src/shader_recompiler src/video_core/native_lowering.cpp \
      src/video_core/native_lowering.h src/video_core/mkw_wave_workaround.h \
      src/video_core/shader_environment.cpp src/video_core/shader_environment.h \
      src/video_core/renderer_vulkan/pipeline_shader_emission.h
  ```
