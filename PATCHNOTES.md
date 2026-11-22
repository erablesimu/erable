# Notes de version d'Erable

La première section ci-dessous est affichée dans la fenêtre de mise à jour et sur la page de la
version. Les sections suivantes gardent l'historique.

## Nouveautés de cette version

**Panneau « En ligne » sur l'accueil**
- Un bouton en haut de l'accueil montre l'état (Hors ligne, salon et nombre de joueurs) et le
  nombre de demandes d'ami et de messages non lus. Il ouvre un panneau à droite :
  - « Connexion en ligne » (avec votre compte Erable), puis trois onglets ;
  - Salons : la liste des salons du serveur, avec leurs joueurs et leur jeu ;
  - Amis : leur présence, « Rejoindre » leur salon, « Écrire » pour discuter avec eux, et l'ajout
    d'un ami par son code ;
  - Alertes : les demandes d'ami à accepter ou refuser ;
  - le bouton « – » réduit le panneau ; l'état reste affiché en haut.
- Un salon accueille un seul jeu à la fois, celui de ses joueurs : lancer un autre jeu propose de
  changer de salon. Un salon vide redevient libre.
- Le salon choisi est gardé jusqu'à la fermeture d'Erable ; au lancement suivant, Erable démarre
  hors ligne.
- La fenêtre du salon peut maintenant être réduite.
- Le bouton d'état est rouge hors ligne et vert en ligne.
- Les salons sont triés par jeu, et la liste « Jeu » n'en montre qu'un.
- Les boutons de la musique et du thème de l'accueil sont passés en bas à gauche.
- Le jeu local passe par les salons du serveur Erable : le mode Wi-Fi (même réseau) n'est plus
  proposé, et s'il était activé il est coupé au démarrage.
- L'ancien explorateur de salons publics de yuzu ne s'ouvre plus : cliquer sur l'état du salon,
  en bas à droite, ouvre le panneau « En ligne » de l'accueil.

**Son qui ne coupe plus**
- Quand la sortie audio de Windows cesse de réclamer du son sans signaler d'erreur (par exemple
  quand un jeu ouvre le micro, comme Ocarina of Time au démarrage), Erable la rouvre au bout d'une
  seconde, au lieu de laisser le jeu muet jusqu'à la fermeture.

**Textures qui scintillent**
- Le compteur d'occlusion n'est plus remis à zéro en pleine partie quand le jeu tarde à le lire :
  dans Ocarina of Time, cette remise à zéro faisait clignoter certains objets et textures (eau,
  fenêtres).
- Les grosses textures ASTC (Ocarina of Time) sont décodées par petits lots sur toutes les cartes
  graphiques, au lieu de déborder en mémoire système et de forcer un grand ménage du cache.
- Une copie de texture linéaire ne déborde plus de sa destination.

**Corrections reprises de Lichii**
- Salons : le relais garde l'adresse et l'identité de chaque joueur, pour un jeu local plus fiable
  à travers le serveur.
- Petites cartes graphiques dédiées et anciens pilotes AMD (Radeon RX 400/500) : blocs de mémoire
  plus petits et deux compilations de pipelines au plus en même temps.
- Moins de processeur gaspillé : la partie du GPU qui lit les commandes dort au lieu de tourner en
  attendant.
- Plantages corrigés : Xenoblade Chronicles DE en passant de docké à portable, mémoire de
  Xenoblade 2, images de fenêtre au format 2 ; réglages enregistrés sans risque de fichier vide
  après une coupure.
- Cache de shaders : réutilise les pipelines déjà compilés pendant la session.

**Tutoriel**
- Un tutoriel en 8 chapitres explique Erable : ce qu'il faut (tes jeux, tes clés et ton firmware,
  tirés de ta console), ajouter ses jeux, jouer, les réglages pour son PC, les manettes, le jeu en
  ligne, les mises à jour et l'aide. Il s'ouvre depuis le bouton « Tutoriel » de l'accueil ou
  Aide › Tutoriel.
- À la toute première ouverture d'Erable, il propose d'appliquer les réglages recommandés pour
  ton PC, puis ouvre le tutoriel.

**Discussion entre amis, plus de discussion dans les salons**
- Les salons n'ont plus de discussion : on n'y croise plus les messages d'inconnus.
- On écrit à ses amis depuis l'onglet Amis du panneau, qu'ils soient dans un salon ou non. Les
  messages sont gardés 15 jours sur le serveur, puis effacés.

**Salons réservés par jeu, et salons en plus quand tout est plein**
- Les salons du serveur peuvent être réservés à un jeu, avec leur nombre de places : la liste
  montre « Mario Kart World · 3/8 joueurs » ou « complet ». Lancer un autre jeu dans un salon
  réservé propose de changer de salon.
- Quand tous les salons d'un jeu sont pleins, « Ouvrir un salon de plus » en crée un sur le
  serveur, qui se ferme après 10 minutes vide (64 salons au plus). Pour les jeux à petits groupes
  (Pokopia, Splatoon Raiders, les Pokémon), on peut en ouvrir un à tout moment, même si d'autres
  ont encore de la place.

**Pas de mods ni de codes de triche dans les salons**
- Lancer depuis l'accueil un jeu qui a des mods ou des codes de triche activés, en étant dans un
  salon, propose de les mettre en pause d'un clic (ou de jouer hors ligne avec). Les correctifs
  intégrés d'Erable ne comptent pas : tout le monde les a.
- Les mods mis en pause reviennent tout seuls en passant hors ligne, en quittant le salon ou au
  lancement suivant d'Erable.

**Filtre à insultes**
- Dans les messages entre amis, les insultes et gros mots des langues européennes s'affichent
  en étoiles (français, anglais, espagnol, italien, allemand, portugais, néerlandais, polonais,
  tchèque, slovaque, hongrois, roumain, langues nordiques, finnois, croate et serbe, turc, grec,
  russe, ukrainien, bulgare). Les messages envoyés partent déjà masqués.
- Il voit à travers les fautes et les contournements : majuscules, accents, chiffres à la place
  des lettres (« s4l0pe »), lettres répétées, espacées ou séparées par des points (« c.o.n »),
  orthographes qui se prononcent pareil (« phuck »), deux lettres inversées dans les mots longs.
- Les pseudos insultants ou offensants sont interdits : le serveur les refuse à l'inscription,
  même collés à un autre mot (« BigC0nnard »), et un compte qui en a déjà un doit en changer
  avant de rejouer en ligne (« Changer mon pseudo » dans le panneau).

**Mario Kart World en sans fil local**
- Le « Patientez » sans fin du mode sans fil local est corrigé, sans mode avion, seul comme dans un
  salon : le jeu ne tente plus sa connexion aux serveurs de Nintendo, toujours bloquée.
- Un groupe créé seul ne se ferme plus aussitôt avec une erreur.
- Deux PC dans le même salon du serveur Erable se retrouvent en sans fil local.

**Moins de saccades quand la mémoire manque**
- Quand le cache de textures doit libérer de la place en urgence, il jette d'abord les images
  qu'il n'a pas besoin de recopier, au lieu d'attendre la carte graphique pour chacune. Dans
  Ocarina of Time, avec la mémoire de Windows presque pleine, il attendait 1 441 fois en
  20 minutes, dans presque chaque saccade.
- Conseil : si Erable saccade sur un PC bien équipé, agrandissez le fichier d'échange de Windows
  (« Gestion automatique » ou au moins 16 Go) et fermez les programmes gourmands.

**Réglages recommandés selon votre PC**
- Aide > « Appliquer les réglages recommandés » choisit aussi la résolution, le mode de la console
  et la mise à l'échelle selon votre carte graphique, votre processeur et votre mémoire :
  - résolution 1x partout : les jeux Switch 2 s'agrandissent eux-mêmes (DLSS ou FSR), et le 2x
    saturait la mémoire de Windows dans Ocarina of Time, même sur une RTX 4070 SUPER ;
  - mode docké (1080p) sur les cartes graphiques dédiées, portable sur les puces intégrées ;
  - NVIDIA DLSS sur les GeForce RTX, sans anti-crénelage par-dessus ; FSR 4 sur les Radeon RDNA 4 ;
  - SMAA, filtrage anisotrope 16x et mise à l'échelle FSR à partir des cartes de milieu de gamme ;
    FXAA et filtrage bilinéaire sur les plus modestes (Radeon RX 400/500, puces intégrées) ;
  - Mario Kart World en 60 images par seconde (30 quand ça ralentit) sur les PC haut de gamme.
- « Restaurer mes réglages précédents » remet aussi ces réglages comme avant.

**Jeu en ligne**
- Erable ne rejoint plus tout seul, au démarrage, le dernier salon du serveur externe : on choisit
  « Serveur externe » (ou « Rejoindre » chez un ami) quand on veut jouer. Le mode Wi-Fi reste tel
  qu'il était.
- Mario Kart World et les jeux qui ouvrent des connexions IPv6 : Erable les accepte désormais au
  lieu de les refuser (le jeu en local sans fil de Mario Kart World restait bloqué sur
  « Patientez »).

**Corrections**
- Pokémon Épée et Bouclier : moins de flashs de lumière soudains. Le compteur de pixels dont le
  jeu se sert pour l'éblouissement du soleil était remis à zéro quatre fois par seconde ; il est
  maintenant conservé.
- Jeux Switch 2 (Ocarina of Time notamment) : plus de plantage avec l'erreur 2010-0212 au bout
  d'une trentaine de secondes, ni de blocage en quittant le jeu ensuite.
- Cartes graphiques dont le pilote refuse de créer de nouveaux échantillonneurs de textures : Erable
  en réutilise un existant au lieu d'échouer.
- Plus de stabilité : recherches dans le cache de textures bornées, événements internes orphelins
  ignorés.

**Mises à jour**
- Une mise à jour refusée, reportée ou pas terminée au lancement n'est plus perdue : un bouton
  « Mise à jour » apparaît dans le dock de l'accueil tant qu'elle n'est pas installée, et
  l'installe en un clic.
- Nouveau : Aide > « Rechercher les mises à jour… », pour vérifier à tout moment (même si la
  recherche au lancement est désactivée). Erable dit s'il est à jour ou pourquoi la recherche a
  échoué.

**À savoir**
- Cette version change de nouveau le traducteur de shaders : les shaders sont recompilés une
  fois au premier lancement de chaque jeu. Dès 60 %, vous pouvez choisir « Jouer maintenant ».
- Build 41 : sur les cartes NVIDIA, Erable garde son propre cache de shaders, pour que les mises
  à jour suivantes ne recompilent plus tout.

**Processeurs Intel récents**
- Sur les Intel à cœurs « performance » et « efficacité » (12e à 14e génération, Core Ultra),
  Erable n'oblige plus ses fils les plus pressés à rester sur les cœurs performance : Windows les
  y place en priorité et peut déborder sur les autres. Les forcer les faisait attendre les uns
  derrière les autres (jusqu'à deux fois moins d'images par seconde sur certains processeurs).
  L'ancien comportement reste possible avec `EDEN_HYBRID_CORES=1`.

**Images et textures**
- Les images que le GPU dessine et que le jeu relit ensuite sont mieux recopiées dans sa mémoire :
  moins d'objets noirs, de motifs « arc-en-ciel » ou d'éléments périmés quand une image est
  recréée (personnages de The Duskbloods notamment).
- Les shaders de plusieurs jeux Switch 2 sont mieux pris en charge (nouvelles instructions,
  tables de ressources), et ceux qui ne peuvent pas l'être sont écartés au lieu de dessiner faux.

**Jeu en ligne (nouveau)**
- Dans la fenêtre de profil (le bouton avec votre pseudo et votre image, en haut de l'accueil),
  la nouvelle section « Jeu en ligne » permet de jouer en local sans fil avec d'autres joueurs :
  - **Wi-Fi** : avec les personnes connectées au même Wi-Fi ou au même réseau que vous ;
  - **Serveur externe** : avec des joueurs où qu'ils soient, via le serveur d'Erable. Choisissez
    un des 16 salons ; tous les joueurs d'un même salon se voient comme s'ils étaient dans la
    même pièce. Lancez ensuite le mode « sans fil local » du jeu.
- Le serveur externe demande un **compte Erable** (pseudo, e-mail, mot de passe), créé depuis
  la même fenêtre. Chaque compte reçoit un code ami. En cas d'oubli, la réponse à votre question
  de sécurité et votre code de sécurité permettent de choisir un nouveau mot de passe.
  « Supprimer mon compte » efface tout ; la notice de confidentialité dit ce qui est gardé.
- **Amis** (dans la même fenêtre, une fois connecté) : ajoutez un ami avec son code ami, acceptez
  ou refusez les demandes, voyez qui est en ligne, à quoi il joue et dans quel salon, et
  rejoignez-le en un clic. Retirer ou bloquer un joueur se fait au même endroit.
- Une demande d'ami reçue s'affiche en bas de l'écran (Accepter, Refuser, Plus tard). Erable en
  arrière-plan, la barre des tâches clignote et Windows affiche une notification. Pendant une
  partie, rien ne s'affiche : la demande attend la fin du jeu.
- Le mode « en ligne » des jeux (serveurs Nintendo) n'est pas concerné : c'est le jeu en local
  sans fil qui passe par Erable.
- L'ancien menu « Multijoueur » hérité de yuzu disparaît : son hall public dépendait des serveurs
  de yuzu, fermés depuis 2024. Le salon rejoint reste affiché dans la barre d'état, d'où s'ouvre
  sa discussion.

**Accueil**
- Bouton « Plein écran » en haut à droite de l'accueil (ou touche F11) : l'accueil et la
  bibliothèque remplissent l'écran, sans menus. « Fenêtre » revient comme avant. Le choix est
  gardé ; pendant une partie, c'est le réglage plein écran du jeu qui décide.
- Au lancement d'un jeu depuis l'accueil, son image et son nom s'affichent tout de suite avec un
  anneau qui tourne (« Lancement du jeu… »), et le curseur d'attente de Windows pendant les
  secondes où Erable ouvre le jeu et démarre Vulkan : on voit que ça charge, pas que ça a planté.
- Dès que 60 % des shaders sont prêts, l'écran de chargement propose « Jouer maintenant » : le jeu
  démarre tout de suite (la précompilation qui suit est sautée aussi) et le reste se termine en
  arrière-plan pendant la partie. Les objets dont le shader n'est pas encore prêt apparaissent un
  instant plus tard au lieu de figer l'image (jusqu'à 19 s d'arrêt en entrant dans une zone avant).
  Sans cliquer, le chargement va jusqu'au bout comme avant.
- Pendant la partie, la préparation des shaders en arrière-plan ne prend plus que la moitié des
  cœurs du processeur : le jeu garde les autres (Ocarina descendait à 43-57 images/s pendant les
  15 premières minutes). Elle prépare aussi d'abord les shaders des lieux qui suivent d'habitude
  celui où vous êtes, d'après vos dernières parties.
- Le record du petit jeu des feuilles est enfin gardé d'une session à l'autre.
- Jeux introuvables : avec OneDrive, Windows a deux dossiers Documents ; Erable cherche
  maintenant dans `Documents\Erable\Jeux` des deux. Les raccourcis vers un dossier ou un jeu sont
  suivis. Et ce qu'Erable trouve sans pouvoir l'afficher est écrit sur la case « Ajouter des
  jeux » : archives .zip/.7z/.rar à décompresser, mise à jour ou DLC sans son jeu, fichier resté en
  ligne sur OneDrive, fichier abîmé (la liste des fichiers s'affiche en survolant).
- Le bouton « Dossier Jeux » en bas de l'accueil s'appelle maintenant « Mes jeux » : il ouvre le
  dossier où sont vos jeux (avec OneDrive, il demande lequel des deux).
- Erable ne reste plus en arrière-plan après sa fermeture : si quelque chose l'y retient plus de
  20 secondes, il se ferme de lui-même (cela bloquait le journal et la mémoire du PC).

**Sauvegardes**
- Nouveau bouton « Sauvegardes » en bas de l'accueil : il récupère en un clic les sauvegardes d'un
  autre émulateur du PC.
- Nouvelle fenêtre Outils > Sauvegardes : les autres émulateurs du PC (Eden, Lichii, yuzu…) sont
  trouvés tout seuls, et leurs sauvegardes reprises en un clic. Aussi : une copie de sécurité de
  tous les jeux et profils en un zip, et sa remise en place (on peut glisser le zip sur la fenêtre).
- Clic droit sur un jeu > Importer une sauvegarde : ne reprend que ce jeu, depuis un autre
  émulateur du PC, un dossier ou un zip.
- Erable fait une copie automatique de vos sauvegardes avant chaque changement. En cas d'erreur,
  « Récupérer une copie automatique… » en bas de la fenêtre remet tout comme avant.

**Zelda Ocarina of Time**
-- En mode portable avec la résolution au maximum, l'agrandissement de l'image passe aussi par
  FSR (plus net).
 Avec le déblocage des FPS, le Chant de l'Épouvantail s'enregistre à la bonne durée et les
  cinématiques vidéo restent synchronisées.
- Avec le déblocage des FPS au-dessus de 60, les vêtements et les cheveux bougent à la bonne
  vitesse.
- Plus d'écrans de la version 1.0 reconnus, et le jitter FSR ne déborde plus sur les bords de
  l'image avec une carte AMD.
- Les passes d'agrandissement de l'image sont reconnues à coup sûr, et les shaders des décors se
  compilent bien plus vite sur les cartes Intel.
- Les anciennes cibles de rendu qui se chevauchent sont libérées plus tôt : moins de mémoire vidéo
  occupée dans les lieux comme la Maison des Skulltulas.
- Le réglage « Sans netteté » de l'image s'applique bien, et la fin de la vidéo d'introduction ne
  produit plus d'erreur.

**Mario Kart World**
- Mode 60 FPS partout à vitesse normale (Configurer > jeu), avec un repli automatique à 30 FPS
  quand le PC ne suit pas.
- Horloge du GPU plus fidèle, et compteurs de synchronisation exacts.
- Le décor ne s'éclaircit plus puis revient à sa couleur en boucle : le buffer des sondes de
  lumière est lu en entier au lieu d'être coupé à 8 Mo.

**Moins de mémoire vidéo, moins de grosses saccades**
- Aux entrées de zone, les textures éparpillées dans la mémoire du jeu sont rassemblées sur
  plusieurs cœurs du processeur au lieu d'un seul, et les grosses ne sont plus recopiées une
  seconde fois avant d'être décodées (Ocarina en charge près de 900 Mo d'un coup).
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
-- Corrections de Lichii : moins d'images fausses ou périmées après les copies de mémoire du jeu,
  ombres plus propres quand la résolution est augmentée, mémoire vidéo des tampons inutilisés
  rendue après 60 images, et des attentes du GPU qui pouvaient bloquer sont débloquées. Pas de
  recompilation des shaders.
 Un jeu qui refuse de s'arrêter ne bloque plus Erable : il est fermé proprement, et les réglages
  et le temps de jeu sont enregistrés sans risque de fichier abîmé.
- Fichiers NSZ et NCZ « solides » lisibles.
- Corrections du GPU émulé (copies DMA, synchronisation) et des pilotes Intel et AMD (RADV).
- Des fichiers abîmés (clés, jeux compressés, patchs, codes de triche) ne font plus planter Erable.
- Le volume réglé dans Erable n'est plus figé au démarrage du jeu.
- Le plein écran sans bordure se place correctement depuis une fenêtre agrandie.
- Donkey Kong Bananza : un réglage du processeur dont le jeu a besoin reste actif.
- Moins de plantages quand la mémoire vidéo est pleine : Erable libère de la place et réessaie.
- Une liste interne du GPU qui grossissait sans fin pendant les longues parties est corrigée.
- Arrêter un jeu est plus sûr (codes de triche, objets du système), et Annuler dans Configurer
  annule aussi les changements de manette non appliqués.
- Cartes AMD RX 6000 : nouveau réglage « Mesh shaders » (Automatique par défaut) qui évite les
  blocages de la carte.
- Mode debug : « Arrêter et enregistrer ma session » fonctionne aussi pendant une partie ou le
  chargement des shaders, et garde le journal jusqu'à ce moment.
- Mode debug : si le jeu se bloque à l'arrêt et qu'Erable doit fermer d'urgence, la session est
  quand même enregistrée tout de suite, au lieu d'attendre le lancement suivant.
- Au tout premier lancement, une seule fenêtre demande le nom du profil (il y en avait deux).
- Accueil : clic droit sur un boîtier, touche X de la manette ou bouton « Options » : le menu du
  jeu (réglages, import de sauvegarde, conversion en NX, dossiers…), qui n'existait que dans la
  bibliothèque.
- Un jeu déplacé dans le dossier de jeux d'Erable n'apparaît plus en double quand l'ancien dossier
  le contenait aussi.
- Clic droit sur un jeu NSP ou XCI > « Convertir en NX (un seul fichier) » : le jeu, ses mises à
  jour et ses DLC trouvés à côté sont réunis dans une seule archive. Cet outil est maintenant en
  français.
- Erable démarre sur la carte graphique la plus puissante : une carte dédiée passe avant la puce
  intégrée du processeur, quelle que soit la marque (avant, une puce AMD intégrée pouvait passer
  devant une carte Intel Arc).
- Cartes de 12 Go et plus : le cache de textures garde plus d'images, au lieu d'en supprimer des
  centaines toutes les 10 secondes pour les recharger aussitôt (Mario Kart World).
- Jeux un peu plus rapides à ouvrir : leurs fichiers ne sont lus qu'au moment où ils servent.
- Le gyroscope de la manette marche toujours après l'avoir rebranchée.
- Le journal est écrit sur le disque chaque seconde : un plantage n'en perd plus la fin.

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
