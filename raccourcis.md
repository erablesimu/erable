# Raccourcis Erable

Chemins et réglages utiles sous Windows. Collez un chemin dans la barre d'adresse de
l'Explorateur, ou dans Exécuter (Win + R).

## Dossiers

| Quoi | Chemin |
|---|---|
| Journaux (`erable_log.txt`) | `%APPDATA%\erable\log` |
| Réglages (`qt-config.ini`) | `%APPDATA%\erable\config` |
| Cache de shaders, un dossier par jeu | `%APPDATA%\erable\shader` |
| Cache de shaders de Zelda Ocarina of Time | `%APPDATA%\erable\shader\0400486024dec000` |
| Rapports de compatibilité | `%APPDATA%\erable\reports` |
| Sessions enregistrées (Mode debug) | `%APPDATA%\erable\sessions` |
| Toutes les données | `%APPDATA%\erable` |
| Vos jeux (affichés à l'accueil) | `Documents\Erable\Jeux` |

Le journal n'est complet qu'une fois Erable fermé.

## Enregistrer une session (diagnostic)

Menu **Mode debug → Enregistrer ma session**, puis lancez le jeu et refaites ce qui pose
problème. Fermez le jeu, puis **Mode debug → Arrêter et enregistrer ma session** : Erable range
le journal, le journal GPU et un éventuel rapport de plantage dans un zip, dans
`%APPDATA%\erable\sessions`, à joindre au rapport (Mode debug → Signaler un problème).

L'enregistrement active les diagnostics de l'ancien lanceur `.bat` : saccades
(`EDEN_NX2_STUTTER_TRACE`), son (`EDEN_NX2_AUDIO_LEVEL`), lectures de fichiers
(`EDEN_NX2_FS_READ_TRACE`) et cache de textures (`EDEN_NX2_TC_LOG`). Si un jeu a déjà tourné
depuis le démarrage, Erable propose de redémarrer pour enregistrer. Si Erable plante pendant
l'enregistrement, la session est enregistrée au lancement suivant. Fermer Erable pendant un
enregistrement l'enregistre aussi.

### Lanceur de diagnostic (tests avancés)

Pour comparer deux réglages, un fichier `erable-trace.bat` à côté de `erable.exe` reste possible :

```bat
set EDEN_NX2_STUTTER_TRACE=1
start "" erable.exe
```

Lignes à ajouter avant `start` selon le test :

| Ligne | Effet |
|---|---|
| `set EDEN_NX2_ASYNC_TRANSLATION=0` | Coupe la traduction des nouveaux shaders hors du fil graphique, pour comparer |
| `set EDEN_NX2_PARALLEL_SWIZZLE=0` | Prépare les grosses textures sur un seul cœur, comme avant (en cas de blocage au chargement) |
| `set EDEN_NX2_PARALLEL_POOL_KIB=1024` | Remet l'ancienne limite de la traduction parallèle, pour comparer |

## Menus

| Quoi | Où |
|---|---|
| Réglages recommandés, et retour en arrière | Aide → Appliquer les réglages recommandés |
| Langue de l'interface | Émulation → Configurer → Général → Interface |
| Précision GPU, shaders asynchrones | Émulation → Configurer → Graphismes → Avancé |
| Dossier du cache d'un jeu | Clic droit sur le jeu → Ouvrir le cache de pipelines transférable |
| Rapport de compatibilité | Clic droit sur le jeu → Rapport de compatibilité… |
| Fermer le jeu sans quitter l'émulateur | Bouton ✕ de la fenêtre → Fermer le jeu, ou Émulation → Arrêter (F5), ou L + Plus + Moins sur la manette |
| Afficher le jeu en cours sur Discord | Émulation → Configurer → Général → Web |

## Liens

- Lancer un build : https://github.com/erablesimu/erable/actions/workflows/ci.yml
- Versions publiées : https://github.com/erablesimu/erable/releases
- Notes de version : `PATCHNOTES.md`
- Pistes et mesures : `docs/banst-pistes.md`
