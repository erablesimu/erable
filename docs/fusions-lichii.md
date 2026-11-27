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

## Historique

- build-39 (15 novembre 2026) : lot avec changement du traducteur (#1757, #1807, #1825, #1848,
  #1853). Prochain lot au plus tôt une semaine après.
- build-41 (17 novembre 2026) : deuxième lot, avancé à la demande du propriétaire (#1914, #1915,
  #1940, #2074, #2083, #2103 ; #1954 était déjà là). Un seul recompilage pour les joueurs, en
  même temps que le cache Vulkan d'Erable sur NVIDIA.
- build-44 (18 novembre 2026) : troisième lot, avancé à la demande du propriétaire (racines
  NVN2 1-7, #2132, #2140 et quatre abaissements d'instructions), avec la réécriture des
  write-back d'images de Lichii (« AIO-10 component #1501 », #2133, #2134). Laissés de côté :
  #1445 (repose sur leurs traces d'upload) et la reprise de l'agrandissement d'Ocarina en
  portable (#1866), qui demande leur passe FSR.
- build-45 (20 novembre 2026) : six correctifs hors shaders choisis à la veille du 19 novembre :
  #2233 (déverrouillages de pages nulles bornés à 1 Mo sous Windows), #2234 (prepo 10500, plantage
  2010-0212 d'Ocarina), #2235 (recherches de descripteurs bornées), #2236 (échantillonneur de
  secours), #2252 (événements dont le propriétaire a disparu), #2253 (capture d'échec MME).
  Laissés de côté : l'exécution native ARM64 et Apple Silicon (#2246, #2249, #2254), les
  statistiques Dynarmic (#2248), la liaison des sauvegardes Ryujinx (#2245, en conflit avec notre
  fenêtre Sauvegardes) et l'option Yoshi (#2242).
- build-53 (21 novembre 2026) : 28 commits choisis aux veilles du 20 au soir et du 21 au matin.
  Traducteur de shaders : #2301 (TrackAddend itératif), #1763 (recherche StorageBufferAddr par
  prédicat). Ailleurs : relais des salons qui garde l'adresse et l'identité ENet de chaque joueur
  (#2336) ; petites cartes dédiées et anciens pilotes AMD (#2316, #2311) ; front du GPU qui dort
  au lieu de tourner (#2237, #2247) ; textures et cache (#1898, #2167) ; plantages et robustesse
  (#2282, #2299, #2304, #2327, #2177, #1972) ; et #1858, #2151, #2176, #1540, #1971, #2292,
  #2250, #2313, #2293, #1811, #2349, #2347, #2346. Laissés de côté : nom « Lichii Optical Flow »
  (#1633), compteur Dynarmic (#2315), réglages Duskbloods (#2288, #2186), commits « AIO only »,
  tests seuls ; en conflit, à porter plus tard : série Switch 2 Edition et NUP (#2329, #2332,
  #2340, #2338, #2362), #2291, #2319, #2312, #2296, #2305, #2262, #2358, #2360, série FPS Unlock
  d'Ocarina (#2179, #2314, #2317, #2318).
- build-55 (22 novembre 2026) : #2393 (décodage ASTC par lots sur toutes les cartes, sur les
  pics de ménage du cache d'Ocarina) et #2414 (copie linéaire bornée), choisis à la veille du
  22 au matin. Laissés de côté : la boutique de mods GameBanana (#2405, contraire au jeu en ligne
  sans mods) et FlyHigh (#2406, modèle sous licence non commerciale, touche leurs workflows).
- build-56 (27 novembre 2026) : choisis aux veilles du 22 au 27 novembre. Traducteur de shaders :
  #2416, #2421, #2480, #2523, #2567, #2577, #2614, #2647. Ailleurs : #2361, #2415, #2420, #2436,
  #2464, #2173, #2215, #2452, #2488, #2498, #2418, #2533, #2465, #2539, #2483, #2551, #2608,
  #2609, #2619, #2610, #2623, #2628, #2641, #2643, #2657 ; portés à la main : #2566 et #2573
  (vues de la scène d'Ocarina). Laissés de côté : #2482 (annulé chez Lichii), #2459 (lightmaps de
  Mario Kart World, constructeur d'image trop différent du nôtre, et seulement utile quand la
  mémoire vidéo manque), fil de rendu GPU séparé (#2425, #2449), Optical Flow (#2386), boutique
  de mods, et les conflits #2470, #2481, #2466, #2542, #2517, #2485, #1640, #2502, #2411, #2576,
  #2591, #2605, #2606.
