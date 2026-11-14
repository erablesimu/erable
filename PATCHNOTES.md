# Notes de version d'Erable

La première section ci-dessous est affichée dans la fenêtre de mise à jour et sur la page de la
version. Les sections suivantes gardent l'historique.

## Nouveautés de cette version

**Accueil**
- Bouton « Plein écran » en haut à droite de l'accueil (ou touche F11) : l'accueil et la
  bibliothèque remplissent l'écran, sans menus. « Fenêtre » revient comme avant. Le choix est
  gardé ; pendant une partie, c'est le réglage plein écran du jeu qui décide.
- Au lancement d'un jeu depuis l'accueil, son image et son nom s'affichent tout de suite avec un
  anneau qui tourne (« Lancement du jeu… »), et le curseur d'attente de Windows pendant les
  secondes où Erable ouvre le jeu et démarre Vulkan : on voit que ça charge, pas que ça a planté.

**Zelda Ocarina of Time**
- Avec le déblocage des FPS, le Chant de l'Épouvantail s'enregistre à la bonne durée et les
  cinématiques vidéo restent synchronisées.

**Mario Kart World**
- Mode 60 FPS partout à vitesse normale (Configurer > jeu), avec un repli automatique à 30 FPS
  quand le PC ne suit pas.

**Moins de mémoire vidéo, moins de grosses saccades**
- Quand le jeu dessine l'image suivante un peu plus loin en mémoire (Bourg d'Hyrule d'Ocarina),
  les anciennes cibles de rendu sont supprimées dès qu'elles ne servent plus, au lieu de
  s'accumuler jusqu'à un grand ménage qui figeait l'image. `EDEN_NX2_TC_COLD_SWEEP=0` revient
  à l'ancien comportement.
- Les images Vulkan des cibles de rendu supprimées sont gardées quelques secondes (512 Mo au plus)
  et reprises par la prochaine image de même taille, au lieu d'être détruites puis recréées.
  `EDEN_NX2_IMAGE_POOL=0` revient à l'ancien comportement.
- Le tampon de profondeur d'Ocarina, plus grand que l'image, n'est plus rechargé depuis la mémoire
  du jeu à chaque image alors qu'il est aussitôt effacé.
- Les écritures de registres GPU qui ne changent rien sont écartées plus tôt (plus de la moitié
  de celles d'Ocarina).
- La mémoire des textures n'est plus rendue à Windows quand la RAM ne manque pas : la recréer
  coûtait des à-coups au chargement des textures.

**Stabilité et compatibilité**
- Un jeu qui refuse de s'arrêter ne bloque plus Erable : il est fermé proprement, et les réglages
  et le temps de jeu sont enregistrés sans risque de fichier abîmé.
- Fichiers NSZ et NCZ « solides » lisibles.
- Corrections du GPU émulé (copies DMA, synchronisation) et des pilotes Intel et AMD (RADV).

**Mises à jour**
- Fenêtres de mise à jour redessinées aux couleurs d'Erable et entièrement en français : notes
  plus lisibles, téléchargement avec Mo, vitesse et temps restant, boutons explicites.
- La fenêtre de mise à jour ne montre plus que ce qui a changé depuis votre version, build par
  build, au lieu de tout l'historique depuis le build 24.

**Zelda Ocarina of Time**
- Plus de flashs blancs (raccourci sur certaines commandes GPU annulé).
- Les ombres du feuillage s'affichent aussi sur les cartes sans mesh shaders.
- Déblocage des FPS au-delà de 60 : plateformes mobiles, Gohma, son, ondulations de l'eau et
  compteurs du jeu restent à la bonne vitesse.
- DLSS : image d'entrée nettoyée (plus de pixels invalides) ; « No Sharpening » sur la 1.0.

**Tous les jeux**
- Windows : la RAM émulée n'est plus réservée d'un bloc au lancement (14 Go) mais au fur et à
  mesure que le jeu la prend (environ 10 Go pour Ocarina) : environ 4 Go rendus au système et au
  cache de textures, surtout utile aux PC avec peu de mémoire.
- Fermer un jeu pendant le chargement des shaders ne bloque plus Erable.
- Moins d'attente : recompilations de shaders de calcul en arrière-plan, état de dessin réutilisé,
  préparation des shaders sur les autres cœurs pour de plus grandes tables de textures.
- Yoshi (ralentissement et plantage), grandes textures ASTC, courtes coupures du son corrigés.
- Le son ne s'arrête plus pour de bon quand le périphérique audio a une erreur (casque débranché,
  Bluetooth qui se reconnecte, pilote) : Erable rouvre le flux, et le jeu ne se fige plus à la
  fermeture.
- Jaquettes SteamGridDB avec votre propre clé API (Configurer > Général > Interface), et jaquettes ou noms
  personnalisés gardés par jeu. Une jaquette verticale remplit la face du boîtier à l'accueil.

**Thèmes**
- Erable n'a plus que ses propres thèmes : **Maple** (le thème habituel, gris chauds et érable),
  **Spring Maple** (teinté de vert) et **Winter Maple** (bleu givré). Configurer > Général >
  Interface. Les anciens thèmes (clair, sombre, bleu nuit…) sont retirés : si vous en utilisiez
  un, Erable passe sur Maple.

**Écran de chargement**
- Nouvel écran de chargement aux couleurs du thème choisi (Maple, Spring Maple ou Winter Maple) :
  icône et nom du jeu au centre, barre fine avec le pourcentage, feuilles d'érable qui tombent
  doucement en fond.
- Pendant le chargement du jeu (avant les shaders) : un anneau tourne autour de l'icône du jeu,
  une lumière parcourt la barre et des astuces d'Erable défilent.
- Le petit jeu des feuilles est refait : une vallée d'érables au crépuscule, des feuilles
  nervurées, un panier en osier qui penche quand il bouge, des étincelles et des points qui
  s'envolent à chaque prise. Feuille dorée (x5), séries jusqu'à x5, goutte de sirop d'érable qui
  élargit le panier, coups de vent qui poussent les feuilles. Aux couleurs du thème.
- Le petit jeu se joue aussi à la manette (croix ou stick gauche), pendant tout le chargement.

**Profils**
- « Qui joue ? » : un clic sur votre profil en haut à gauche de l'accueil affiche tous les profils
  en grands avatars ronds, comme sur une console. Choisissez avec qui jouer (souris, flèches ou
  manette), ajoutez un profil, renommez-le, donnez-lui une image ou supprimez-le. Chaque profil
  garde ses propres sauvegardes ; changer de profil n'y touche jamais.
- L'avatar du profil (image ou couleur) s'affiche aussi sur l'accueil.

**Accueil en boîtiers**
- Le nom de votre profil s'affiche bien à chaque démarrage (il montrait parfois « Eden »).
- En haut, le logo Erable est centré avec un champ de recherche (Ctrl + F, Échap pour revenir).
- Les boîtiers voisins sont à moitié empilés sous le jeu choisi ; la barre de raccourcis du bas,
  qui doublait les boutons, est retirée. Le numéro de jeu n'apparaît plus dans les titres.
- Second mode d'affichage, « Pile » (bouton en haut à droite) : les jeux sont rangés par la
  tranche sur une étagère, chaque tranche teintée de la couleur de son jeu, et le jeu choisi sort
  de la pile en pivotant pour se montrer de face. « Étagère » revient aux boîtiers empilés. Le
  choix est gardé.
- Le bandeau et la tranche de chaque boîtier prennent la couleur principale de l'image du jeu
  (sa teinte dominante, pas une moyenne terne), à l'étagère comme dans la pile.
- Un bouton en haut à droite change de thème depuis l'accueil (Maple, Spring Maple, Winter
  Maple). L'accueil et ses boîtiers prennent maintenant les couleurs du thème choisi.
- Les jeux sont présentés comme des boîtiers Erable en 3D : le jeu choisi au centre, ses voisins
  tournés vers lui, avec leur reflet. Ils glissent d'un jeu à l'autre (flèches, manette, molette
  ou clic), et le nom du jeu s'affiche en grand dessous.
- Fond animé : vagues de lumière et particules qui montent doucement.
- Quand une image communautaire existe pour un jeu, elle est téléchargée une fois et gardée dans
  `%APPDATA%\erable\cache\covers` ; sinon le boîtier utilise l'icône du jeu.

**Correctif**
- Lancer un jeu avec A à la manette depuis l'accueil ne fait plus planter Erable (Mario Kart
  World s'arrêtait net au lancement).

**Chargements de zone**
- Quand la mémoire de Windows est presque pleine, Erable ne relit plus des textures depuis la
  carte graphique pour libérer de la place, sauf en cas de mémoire critique : chaque relecture
  attendait que la carte graphique ait tout fini, au milieu des entrées de zone.
- Quand la mémoire manque, Erable libère d'abord les grosses textures inutilisées : il en
  supprime moins pour la même place, donc en recharge moins ensuite.
- Les blocs de mémoire vidéo gardés pour être réutilisés sont rendus tout de suite quand il reste
  moins de 3 Go de mémoire à Windows : sur les PC qui ont peu de RAM, chaque Mo sert au cache de
  textures.
- Les blocs de mémoire vidéo libérés sont gardés 30 secondes et réutilisés au lieu d'être rendus au
  pilote puis redemandés : les entrées de zone d'Ocarina en demandaient des centaines d'un coup.

**Moins de saccades**
- En entrant dans une zone, les petites textures d'un même dessin sont remplies ensemble sur
  plusieurs cœurs du processeur au lieu d'une par une (Ocarina en crée plus de 2 000 à
  l'arrivée dans le monde).
- Après le chargement, Erable prépare en arrière-plan tous les shaders mis de côté au démarrage,
  les plus récents d'abord, sans retarder ceux dont le jeu a besoin tout de suite.
- Le cache des effets précompilés est reconstruit quand ses réglages ou le jeu changent.

**Moins de travail pour le processeur**
- Les pages de mémoire que le jeu réécrit à chaque image ne sont plus protégées puis débloquées
  en boucle (Ocarina : environ 70 % d'appels système en moins, image de 25,9 à 24,7 ms). `EDEN_NX2_HOT_PAGES=0` revient à l'ancien comportement.
- Les adresses de la carte graphique les plus consultées sont gardées en cache.

**Divers**
- Couper la musique de l'accueil (ou en changer) est enregistré tout de suite, même si Erable
  est fermé de force ensuite.
- Génération d'images Lossless Scaling sur les cartes graphiques sans calcul 16 bits.
- FSR 3.1 mis à jour (3.1.6).
- Les programmes Windows portent leur nom et leur version (Propriétés > Détails), ce qui rassure
  certains antivirus.

## build-27

**Accueil**
- L'accueil est redessiné : grandes vignettes, jeu choisi mis en avant, dock d'icônes, état de la
  manette, horloge et indication « Shaders prêts » pour les jeux déjà préparés.
- À la manette : Plus ouvre les réglages du jeu, Y le met en favori, Moins signale un problème.

**Mode debug**
- Le zip de « Enregistrer ma session » n'est plus vide : il contient bien le journal.
- Mesures plus fines des saccades dues aux textures, pour les prochaines optimisations.

## build-26

**Nouvel accueil**
- Un seul Erable à la fois : le relancer ramène la fenêtre déjà ouverte au premier plan. Si
  l'ancien ne répond plus, Erable explique comment le fermer dans le Gestionnaire des tâches.
- Erable s'ouvre sur un accueil façon console : grandes vignettes de vos jeux, actions du jeu
  choisi (Jouer, Réglages du jeu, Favori, Signaler un problème) et un dock (Bibliothèque,
  Nouveautés, Dossier Jeux, Captures, Mode debug, Réglages). Il se pilote à la manette.
- Trois musiques d'accueil originales, au choix, à couper d'un clic (bouton Musique / Muet).
- Au premier lancement, Erable demande de créer son profil (votre nom) ; le profil « Eden » par
  défaut peut être renommé sans perdre les sauvegardes. Le nom par défaut devient « Joueur ».
- Nouveau dossier **Documents\Erable\Jeux** : déposez-y vos jeux, ils apparaissent à l'accueil.
- La liste et la grille d'avant restent dans Affichage → Bibliothèque (Ctrl + L) ; Ctrl + H
  revient à l'accueil.

**Chargement des shaders**
- Un petit jeu fait patienter pendant le chargement des shaders : déplacez le seau (souris ou
  flèches) pour attraper les feuilles d'érable ; une feuille dorée vaut cinq.

**Zelda Ocarina of Time**
- Les arbres ne flottent plus en l'air dans la plaine d'Hyrule et ailleurs. Pendant qu'un shader
  se prépare, Erable ne saute plus que les dessins refaits à chaque image : un dessin fait une
  seule fois pour une zone attend son shader.
- Arbres volants corrigés aussi sur les cartes GeForce GTX 10xx (Pascal).
- La qualité DLSS choisie change bien la résolution de rendu.

**Tous les jeux (suite)**
- Arrêter un jeu ne peut plus rester bloqué sur certaines attentes du GPU.
- Cartes Intel sous Windows : un plantage au chargement du cache de shaders est évité.

## build-25

**Nouveau nom : Erable (suite)**
- Cette version s'installe à la main (le zip de la page des versions, extrait par-dessus l'ancien
  dossier). Recréez ensuite vos raccourcis vers erable.exe : l'ancien programme n'existe plus.
- Nouveau menu **Mode debug → Enregistrer ma session** : plus besoin du lanceur `.bat`. Les
  diagnostics s'activent, et à la fin le journal complet est rangé dans un zip à joindre au
  rapport. Une session interrompue par un plantage est récupérée au lancement suivant.

**Zelda Ocarina of Time**
- Les arbres ne flottent plus en l'air dans la plaine d'Hyrule et ailleurs (Erable attend qu'un
  shader soit prêt au lieu de sauter le dessin).

## build-24

**Nouveau nom : Erable**
- Le programme s'appelle maintenant erable.exe et ses données sont dans `%APPDATA%\erable`.
  L'ancien dossier de données est déplacé automatiquement au premier lancement :
  sauvegardes, réglages, clés et cache de shaders sont conservés.
- Les anciens raccourcis continuent de marcher : l'ancien programme ouvre erable.exe.
- Le code aussi passe au nom Erable (dossiers, programmes, options de build). Les caches de
  shaders déjà construits restent valables.
- Le dépôt s'appelle maintenant github.com/erablesimu/erable.
- Site erablesimu.github.io/erable, en français, anglais et allemand : téléchargement, tutoriels
  (émulateur et jeux, avec un filtre par jeu), notes de version et formulaire pour signaler un
  problème (Aide > À propos > Report an Issue y mène).
- Page « Bugs » sur le site : les problèmes signalés, en cours ou fermés, filtrables par jeu et par
  type, avec une recherche. La liste est enregistrée dans un fichier SQLite à chaque publication.
- Les journaux s'appellent maintenant `erable_log.txt` (et `erable_crash.dmp`, `erable_gpu.log`).

**Zelda Ocarina of Time**
- Correction des zones sombres en forme de grille sur l'eau et le sol (ombres des lumières).
- Génération d'images NVIDIA DLSS (2x à 4x) disponible comme type de génération d'images.

**Hyrule Warriors : Les Chroniques du Sceau**
- Correction d'un blocage au premier écran de chargement, et d'un arrêt occasionnel pendant les
  vidéos.
- Mode 30 images par seconde et images par seconde dynamiques.

**Tous les jeux**
- Arrêter un jeu ne se termine plus par une erreur, et les jeux Switch 1 au format .nx sont
  reconnus.
- Moins de mémoire perdue et moins de plantages par manque de mémoire Windows.
- Vibrations améliorées, y compris la DualSense en USB.
- Rendu plus léger sur le processeur (états graphiques, rapports, textures calculées) et processeur émulé plus rapide.
- Fermer un jeu avec un très gros cache de shaders attend la fin de l'enregistrement au lieu de couper l'émulateur.
- Précompilation des effets de Mario Kart World et Fire Emblem, et reconstruction des précompilations périmées au lancement.
- Moins de mémoire utilisée en arrière-plan quand la RAM du PC est juste.

**Chargements plus fluides**
- Les grosses textures sont préparées sur plusieurs cœurs du processeur. Sur nos mesures dans
  Zelda Ocarina of Time, les images lentes sont trois fois moins nombreuses, et la part des
  gels de chargement due à l'émulateur est fortement réduite.

**Fermer un jeu sans quitter Erable**
- Le bouton ✕ de la fenêtre propose maintenant « Fermer le jeu » : le jeu s'arrête proprement et
  la liste des jeux revient. « Quitter Erable » ferme tout, comme avant.
- Autres moyens : Émulation > Arrêter (F5), ou L + Plus + Moins sur la manette.

**Nouvelle interface**
- Thème Erable : sombre, gris chauds et couleur érable de notre feuille. Il s'applique au
  premier lancement ; les anciens thèmes restent dans Configurer > Général > Interface.
- Liste de jeux redessinée, avec favoris, et fenêtres de réglages qui tiennent à l'écran.
- Une fenêtre « Quoi de neuf » présente les changements (Aide > Quoi de neuf).

**Jeu à plusieurs**
- Jeu en réseau local : les jeux en sans-fil local peuvent passer par le réseau de la maison,
  sans salon. Désactivé par défaut : Configurer > Réseau > LAN play.

**Performances**
- Les textures récentes restent en mémoire tant que la carte graphique a de la place, au lieu
  d'être jetées puis recréées.
- Le processeur travaille moins quand le jeu attend.

**Moins de saccades à la découverte d'une zone**
- Les nouveaux shaders sont préparés sur les autres cœurs du processeur pendant que le jeu
  continue. Sur nos mesures dans Zelda Ocarina of Time, les saccades sont trois fois moins
  nombreuses. Un objet peut apparaître avec un très léger retard la première fois.

**Discord**
- Erable affiche le jeu en cours dans votre statut Discord. Pour ne pas l'afficher,
  décochez « Afficher le jeu en cours dans le Statut Discord » dans
  Émulation → Configurer → Général → Web.

**The Legend of Zelda: Ocarina of Time**
- Correction du gel dans la maison des gardes.
- Revenir dans une zone déjà visitée ne retraduit plus ses shaders : ils sont gardés en mémoire.

**Tous les jeux**
- Les shaders construits restent disponibles tant que la mémoire du PC le permet, au lieu d'être
  jetés après quelques minutes.
- Sortie HDR native (Off, Auto, On).
- Nouvel affichage des statistiques : images par seconde, charge du processeur et de la carte
  graphique, graphique des 5 dernières secondes.
- Rendu et émulation du processeur allégés sur plusieurs points.
- Yoshi and the Mysterious Book : le monde 3D s'affiche.

## build-13

- Erable applique ses propres réglages recommandés par défaut : précision GPU basse,
  présentation asynchrone, décodage vidéo par la carte graphique et shaders compilés en
  arrière-plan. Sur nos PC, Zelda Ocarina of Time passe d'environ 25 à 40-55 images par seconde.
- Ils s'appliquent tout seuls aux réglages jamais modifiés. Le menu Aide → Appliquer les
  réglages recommandés reste disponible, avec retour en arrière.

## build-12

**The Legend of Zelda: Ocarina of Time**
- Image plus nette et moins de scintillement grâce à AMD FSR 3.1. FSR 4 est proposé en option
  sur les cartes AMD récentes.
- Correction d'un plantage par manque de mémoire.
- Correction du grésillement du son de l'ocarina.

**Tous les jeux**
- Au premier lancement d'un jeu, Erable propose de précompiler ses shaders : quelques
  minutes d'attente une seule fois, puis beaucoup moins de saccades en jouant.
- Nouveau cache de shaders, plus rapide à charger. Il se reconstruit une fois : quelques
  saccades sont normales à la première partie après cette mise à jour.
- Rendu plus rapide (Donkey Kong Bananza gagne environ 25 %).
- Mario Kart World : éclairage sombre corrigé.
- L'émulateur ne plante plus quand la mémoire vidéo est pleine.
- Nouveaux réglages DLSS (qualité et modèle).
- Prise en charge du format de jeux `.nx`.

**Mises à jour**
- Un téléchargement interrompu est retenté automatiquement.
- Cette fenêtre affiche désormais des notes lisibles.

## build-10

- L'interface est vraiment en français. Si elle reste en anglais, choisissez Français dans
  Emulation → Configure → General → Interface.
- Zelda Ocarina of Time : les gels de 2 à 3 secondes à la première apparition de certains effets
  sont nettement réduits.
- Mario Kart World : routes corrigées.
- Moins de plantages sur les cartes Intel Arc.

## build-9

- Correction de la mise à jour automatique.
