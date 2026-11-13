# Erable : pistes à explorer

## Build PGO (optimisation guidée par un profil)

- Le compilateur optimise l'émulateur d'après un profil d'utilisation réel. Gain général attendu
  de quelques pourcents ; peu d'effet sur les saccades de la première partie.
- La CI sait déjà le produire : lancer `ci.yml` avec l'entrée `pgo` sur `both`. L'archive est
  `erable-windows-x86_64-pgo.zip`.
- Jamais essayé chez nous. Points à vérifier avant de le donner à la famille :
  - le profil vient d'Eglantine, pas de notre code actuel, donc le gain peut être plus faible ;
  - il est compilé avec MinGW clang au lieu de MSVC : comparer le rendu sur Zelda ;
  - la mise à jour automatique n'est activée que dans le build MSVC.
- Essai prévu : sur un seul PC, comparer les images par seconde et les saccades avec le build
  standard dans la même scène.

## Saccades de Zelda Ocarina of Time : mesurer avant et après

Lancer avec `EDEN_NX2_STUTTER_TRACE=1`, jouer le même trajet, fermer l'émulateur, puis lire
`eden_log.txt` avec `python3 -I tools/nx2/cache_metrics.py parse <log>` (ou `compare <a> <b>`).

Repères du build-12 sur le PC i5-14400F / RTX 4070 SUPER (7 minutes de jeu) :

| Réglages | Image médiane | Images > 33 ms | Images > 100 ms |
|---|---|---|---|
| Ancien profil (précision GPU haute) | 31,5 ms | 5021 sur 10841 | 91 |
| Réglages recommandés | 20,5 ms | 950 sur 18446 | 61 |

Ce qui reste, dans l'ordre :

1. Changements de zone : jusqu'à 3,6 s d'image figée. Allocation de mémoire vidéo et création
   d'images (environ 1 s), envoi de textures (environ 1 s), traduction de pipelines (0,6 s).
2. Traduction des pipelines en jeu : 513 pipelines, 5,2 s sur le fil GPU, soit 10 ms chacun.
   La ligne `Runtime pipelines in the last 10 s` du journal donne ce temps par tranche de 10 s.
3. Vitesse de fond : le fil GPU reste le frein dans les zones chargées.

Traduction parallèle : `EDEN_NX2_PARALLEL_POOL_KIB=1024` remet l'ancienne limite (Zelda traduit
alors tout sur le fil GPU), `0` coupe la traduction parallèle. Sans la variable, la limite est de
4 Mio. Dans le journal, « ms on the GPU thread waiting for ... stage translation on workers »
doit devenir non nul quand elle sert.

Mesure du build-13 (même PC, trajets différents) : 9,8 ms par pipeline sur le fil GPU avec
l'ancienne limite, 7,3 ms avec la traduction parallèle. Le fil GPU attend encore la traduction
(4,5 s d'attente pour 6,2 s de travail en 6 minutes, 213 pipelines en 10 s à la découverte d'un
village).

## Traduction asynchrone (activée par défaut depuis le build-15)

Portage du correctif d'Eglantine 0f1ae997c2 (branche `fix/oot-map-change`), adapté à Lichii.
Elle sert avec les shaders asynchrones et le mode Bounded ou Skip ; `EDEN_NX2_ASYNC_TRANSLATION=0`
la coupe pour comparer.

Un nouveau pipeline natif dont le dessin peut être sauté n'est plus traduit sur le fil GPU. Le
fil GPU copie ce que la traduction lit (tampons constants, table de textures, quelques
registres), puis la traduction et la compilation partent sur les fils de `shader_jobs`. Le
dessin attend dans la limite du budget par image (`native_pipeline_wait_ms`), puis est sauté ;
un dessin suivant récupère le pipeline et l'enregistre dans le cache.

Différences avec Eglantine :

- La table de textures est copiée au lieu d'être relue plus tard : la traduction voit l'état du
  moment du dessin.
- Les mots des tampons constants qui contiennent une adresse GPU valide sont repérés sur le fil
  GPU. La traduction ne consulte donc jamais la mémoire du jeu depuis un autre fil.
- Un shader qui lit un pointeur à travers un autre pointeur ne peut pas être traduit ainsi : son
  résultat est jeté, il est retraduit sur le fil GPU, et ses pipelines suivants y vont directement.
- Le travail passe par la file `shader_jobs` de Lichii, pas par des fils dédiés.

À lire dans le journal, toutes les 10 s : `Async translation in the last 10 s: N queued (...),
N not detached, N adopted (...), N sent back to the GPU thread, N ready within the wait, N
lookups skipped, ... pending`. À comparer avec `Runtime pipelines in the last 10 s` et avec les
lignes `Nx2Stutter` du même trajet sans la variable.

À surveiller : objets absents plus longtemps qu'avant, végétation ou textures fausses (les
pipelines « sent back » devraient les couvrir), plantage à la fermeture.

Mesure du build-14 avec `EDEN_NX2_ASYNC_TRANSLATION=1` (même PC, 5 minutes, autre trajet) :

| | Build-13, traduction parallèle | Build-14, traduction asynchrone |
|---|---|---|
| Images de plus de 33 ms | 1552 sur 13230 (11,7 %) | 467 sur 14229 (3,3 %) |
| Temps de construction sur le fil GPU | 4,9 s | 1,5 s |
| Image médiane | 24,5 ms | 20,5 ms |

196 pipelines traduits hors du fil GPU (1,1 ms de préparation chacun, 5,3 s de travail sur les
autres cœurs), aucun renvoyé au fil GPU, aucun échec. 193 restent traduits sur le fil GPU
(6,1 ms chacun) : dessins qui ne peuvent pas être sautés, et 38 environnements non détachés.
Les images de plus d'une seconde qui restent viennent des textures aux chargements.

## Chargements de zone (build-15, trace sans manque de mémoire dû au navigateur)

Images de plus de 100 ms en 5 minutes : 23 où l'émulateur travaille (6,9 s) et 28 où les fils
GPU attendent le jeu (6,4 s). Les trois pires (1,0 à 1,8 s) sont des chargements.

- Lecture des fichiers : hors de cause. 3,1 Gio lus en 2,4 s sur toute la session
  (`EDEN_NX2_FS_READ_TRACE=1`).
- Création d'images et mémoire vidéo : 7025 images et 2873 réservations pendant les images
  longues, 4,2 s. Une image de chargement fait 198 réservations en 834 ms. Lichii règle VMA sur
  des blocs de 16 Mio pour limiter les trous (mesuré sur des cartes de 8 Go), ce qui multiplie
  les réservations.
- Mémoire de l'hôte : la limite de validation de Windows borne encore le cache de textures
  (2 à 3 Gio au lieu de 5,9). La RAM émulée de 8 Gio est validée d'avance et compte dedans.

## Taille des blocs de mémoire vidéo (build-18, testé et abandonné)

Même build et même PC, blocs de 16 Mio contre 32 Mio (interrupteur `EDEN_NX2_VMA_BLOCK_MIB`,
retiré depuis) : 650 réservations et 80 ms par minute avec 16 Mio, 1200 et 170 ms avec 32 Mio,
moins de mémoire perdue en trous avec 16 Mio, pire gel de chargement identique (1,3 s). Le
réglage de Lichii (16 Mio) reste. Le temps de réservation était de 430 à 550 ms par minute au
build-15 : la baisse vient du cache de textures de Lichii (images récentes gardées) et d'un
fichier d'échange plus grand.

## Gels de chargement : où va la création d'images (build-18)

Pire image d'un chargement de zone : 1,28 s, dont 612 images créées en 577 ms (0,94 ms chacune)
et 495 ms dans l'envoi des textures. La trace découpe maintenant `image_create` en
`image_backend` (l'image de la carte graphique elle-même : création, mémoire, vues) et
`astc_scan` (lecture d'une texture ASTC pour savoir si elle est opaque) ; le reste est la
recherche des images qui se chevauchent et leurs copies. `EDEN_NX2_TC_LOG=1` ajoute toutes les
600 images le nombre d'images créées et supprimées et les envois depuis la mémoire du jeu.

Résultat (build-19, 19 minutes, 3 chargements) : sur les 12,5 s de `image_create`,
`image_backend` ne fait que 4,6 s et `astc_scan` 1,4 s. Le reste est surtout l'envoi de la texture
depuis la mémoire du jeu, fait au moment de la création (`RefreshContents` dans `JoinImages`).
Dans les images de plus de 200 ms : 61 % du temps est ce travail du fil GPU, 39 % le jeu lui-même
qui charge (fils GPU au repos). Pire image : 1,05 s.

## Préparation des textures en bandes (activée par défaut depuis le build-21)

Reprise d'Eglantine d470788010 (Takoroka), retiré le même jour (dfe734dce6) après un blocage
dans une partie de Mario Kart World sur huit, non expliqué. Les images de 4 Mio et plus sont
découpées en bandes d'environ 1 Mio, traitées par le fil appelant et des fils dédiés.

Relecture : pas de blocage certain trouvé dans le code d'origine, mais deux points fragiles,
corrigés ici :
- il utilisait les fils de conversion des textures, dont les conversions ASTC et BCn attendent
  que toute la file soit vide (`WaitForRequests`) depuis d'autres fils : nos bandes ont leurs
  propres fils (`ImageSwizzle`) ;
- l'attente reposait sur `std::atomic::wait`, et une exception dans une bande laissait
  l'appelant attendre pour toujours : attente par mutex et variable de condition, exception
  renvoyée à l'appelant, et une ligne `Parallel swizzle: N of M bands done after more than a
  second` dans le journal si une bande traîne, pour avoir une trace si un blocage revient.
- Un fil qui traite déjà des bandes ne découpe pas une nouvelle image.

Vérifié hors émulateur (machine à 4 cœurs) : résultats identiques bit à bit à la version d'un
seul fil sur 8 formes d'images (1 à 16 octets par pixel, 3D, tuilage natif), 10 passes de 6 fils
en parallèle sans blocage, 64 Mio préparés en 3,8 ms au lieu de 11 ms.

Mesure du build-20 avec les bandes (même PC, 15 minutes, une session) : aucune ligne de
surveillance, aucun blocage, fermeture propre. Images de plus de 33 ms : 0,7 % contre 2,1 % au
build-19 ; 99 % des images sous 29,5 ms contre 38,5 ms. Dans les images de plus de 200 ms, la
part de l'émulateur tombe à 19 % (0,8 s) ; les 81 % restants sont le jeu qui charge, fils GPU au
repos. Les trajets diffèrent : ordre de grandeur plutôt que chiffre exact. `EDEN_NX2_PARALLEL_SWIZZLE=0`
revient à un seul fil.

## Zones sombres en grille dans l'eau d'Ocarina (build-20)

Cause trouvée par Lichii dans les shaders d'éclairage par grappes (une poignée de carte d'ombre
suivie à travers le résultat d'un échantillonnage, liée comme texture nulle), corrigée dans
bd0171f7b9 (#1485). Notre premier correctif, qui empêchait de sauter les dessins vers des cibles
nouvelles, partait d'une mauvaise hypothèse et a été retiré.
